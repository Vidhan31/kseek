#include "kseek_runner.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QProcessEnvironment>
#include <QLoggingCategory>
#include <QtDBus/QDBusConnection>

Q_LOGGING_CATEGORY(lcRunner, "kseek.runner")

KSeekRunner::KSeekRunner(const QString &searchRoot, QObject *parent)
    : QObject(parent),
      m_queryRegex(QStringLiteral(R"(^f\s+(.+)$)")),
      m_pipeline(this),
      m_actionHandler(this),
      m_debounceTimer(this)
{
    m_debounceTimer.setSingleShot(true);
    connect(&m_debounceTimer, &QTimer::timeout, this, &KSeekRunner::onDebounceTimeout);

    loadEnvironment();
    if (!searchRoot.isEmpty()) {
        setSearchRoot(searchRoot);
    }

    connect(&m_pipeline, &ProcessPipeline::searchFinished, this, &KSeekRunner::onPipelineFinished);
    connect(&m_pipeline, &ProcessPipeline::searchError, this, &KSeekRunner::onPipelineError);
}

KSeekRunner::~KSeekRunner() {
    teardown();
}

void KSeekRunner::loadEnvironment() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    QString root = env.value(QStringLiteral("KSEEK_ROOT"));
    if (root.isEmpty()) {
        root = env.value(QStringLiteral("KRUNNER_FZF_FD_ROOT"));
    }
    if (root.isEmpty() || !QDir(root).exists()) {
        root = QDir::homePath();
    }
    setSearchRoot(root);

    bool ok = false;
    QString maxResultsStr = env.value(QStringLiteral("KSEEK_MAX_RESULTS"));
    if (maxResultsStr.isEmpty()) {
        maxResultsStr = env.value(QStringLiteral("KRUNNER_FZF_FD_MAX_RESULTS"));
    }
    if (!maxResultsStr.isEmpty()) {
        int maxResults = maxResultsStr.toInt(&ok);
        if (ok && maxResults > 0) {
            m_maxResults = maxResults;
        }
    }

    QString timeoutStr = env.value(QStringLiteral("KSEEK_TIMEOUT"));
    if (timeoutStr.isEmpty()) {
        timeoutStr = env.value(QStringLiteral("KRUNNER_FZF_FD_TIMEOUT"));
    }
    if (!timeoutStr.isEmpty()) {
        double timeoutSec = timeoutStr.toDouble(&ok);
        if (ok && timeoutSec > 0.0) {
            m_timeoutMs = static_cast<int>(timeoutSec * 1000.0);
        }
    }

    QString debounceStr = env.value(QStringLiteral("KSEEK_DEBOUNCE"));
    if (debounceStr.isEmpty()) {
        debounceStr = env.value(QStringLiteral("KRUNNER_FZF_FD_DEBOUNCE"));
    }
    if (!debounceStr.isEmpty()) {
        int debounce = debounceStr.toInt(&ok);
        if (ok && debounce >= 0) {
            m_debounceMs = debounce;
        }
    }

    const QString extraArgs = env.value(QStringLiteral("KSEEK_FD_ARGS"));
    if (!extraArgs.isEmpty()) {
        m_extraFdArgs = extraArgs.split(QRegularExpression(QStringLiteral(R"(\s+)")), Qt::SkipEmptyParts);
    }

    const QString extraFzfArgs = env.value(QStringLiteral("KSEEK_FZF_ARGS"));
    if (!extraFzfArgs.isEmpty()) {
        m_extraFzfArgs = extraFzfArgs.split(QRegularExpression(QStringLiteral(R"(\s+)")), Qt::SkipEmptyParts);
    }
}

void KSeekRunner::setSearchRoot(const QString &root) {
    QFileInfo fi(root);
    if (fi.isDir()) {
        m_searchRoot = fi.canonicalFilePath();
    } else {
        m_searchRoot = QDir::homePath();
    }
}

RemoteActions KSeekRunner::actions() const {
    return RemoteActions{
        {QStringLiteral("open_app"), QStringLiteral("Open in Default Application"), QStringLiteral("system-run")},
        {QStringLiteral("show_item"), QStringLiteral("Show in Folder"), QStringLiteral("folder-open")},
        {QStringLiteral("copy_path"), QStringLiteral("Copy File Path"), QStringLiteral("edit-copy")},
        {QStringLiteral("open_terminal"), QStringLiteral("Open Terminal Here"), QStringLiteral("utilities-terminal")}
    };
}

void KSeekRunner::matchAsync(const QString &query, const QDBusMessage &replyMessage) {
    const quint64 reqId = ++m_currentRequestId;
    qCInfo(lcRunner) << "matchAsync received query:" << query << "reqId:" << reqId;

    m_debounceTimer.stop();

    for (auto it = m_pendingReplies.begin(); it != m_pendingReplies.end(); ) {
        QDBusMessage oldReply = it.value();
        oldReply << QVariant::fromValue(RemoteMatches{});
        QDBusConnection::sessionBus().send(oldReply);
        it = m_pendingReplies.erase(it);
    }

    m_pendingReplies.insert(reqId, replyMessage);
    m_pipeline.cancel();

    const QString trimmed = query.trimmed();
    const auto match = m_queryRegex.match(trimmed);
    if (!match.hasMatch()) {
        qCInfo(lcRunner) << "Query does not match regex prefix ^f\\s+:" << trimmed;
        sendReply(reqId, RemoteMatches{});
        return;
    }

    const QString searchTerm = match.captured(1).trimmed();
    if (searchTerm.isEmpty()) {
        qCInfo(lcRunner) << "Empty search term after prefix";
        sendReply(reqId, RemoteMatches{});
        return;
    }

    if (m_debounceMs <= 0) {
        qCInfo(lcRunner) << "Launching search (no debounce) for term:" << searchTerm << "in root:" << m_searchRoot;
        m_pipeline.startSearch(reqId, m_searchRoot, searchTerm, m_extraFdArgs, m_extraFzfArgs, m_maxResults, m_timeoutMs);
    } else {
        m_pendingSearchReqId = reqId;
        m_pendingSearchTerm = searchTerm;
        m_debounceTimer.start(m_debounceMs);
    }
}

void KSeekRunner::sendReply(quint64 requestId, const RemoteMatches &matches) {
    auto it = m_pendingReplies.find(requestId);
    if (it != m_pendingReplies.end()) {
        QDBusMessage reply = it.value();
        m_pendingReplies.erase(it);
        reply << QVariant::fromValue(matches);
        const bool ok = QDBusConnection::sessionBus().send(reply);
        qCInfo(lcRunner) << "Sent reply for reqId:" << requestId << "matches:" << matches.size() << "dbus_send_ok:" << ok;
    } else {
        qCInfo(lcRunner) << "sendReply: no pending reply for reqId:" << requestId;
    }
}

void KSeekRunner::onPipelineFinished(quint64 requestId, const QStringList &relativePaths) {
    if (!m_pendingReplies.contains(requestId)) {
        return;
    }

    RemoteMatches matches;
    matches.reserve(relativePaths.size());

    const int total = relativePaths.size();
    for (int rank = 0; rank < total; ++rank) {
        RemoteMatch m = buildMatch(relativePaths[rank], rank, total);
        if (!m.id.isEmpty()) {
            matches.append(std::move(m));
        }
    }

    sendReply(requestId, matches);
}

void KSeekRunner::onPipelineError(quint64 requestId, const QString &errorMessage) {
    if (!m_pendingReplies.contains(requestId)) {
        return;
    }

    RemoteMatches matches;
    matches.append(buildErrorMatch(errorMessage));
    sendReply(requestId, matches);
}

static const QStringList s_defaultActions = {
    QStringLiteral("open_app"),
    QStringLiteral("show_item"),
    QStringLiteral("copy_path"),
    QStringLiteral("open_terminal")
};
static const QString s_categoryFiles = QStringLiteral("Files & Folders");
static const QString s_categoryError = QStringLiteral("Error");
static const QString s_subtextKey = QStringLiteral("subtext");
static const QString s_categoryKey = QStringLiteral("category");
static const QString s_urlsKey = QStringLiteral("urls");
static const QString s_actionsKey = QStringLiteral("actions");
static const QString s_multilineKey = QStringLiteral("multiline");

RemoteMatch KSeekRunner::buildMatch(const QString &relPath, int rank, int total) {
    QString cleanRel = relPath;
    if (cleanRel.startsWith(QLatin1StringView("./"))) {
        cleanRel.remove(0, 2);
    }
    while (cleanRel.endsWith(u'/') && cleanRel.size() > 1) {
        cleanRel.chop(1);
    }

    QString fullPath;
    if (cleanRel.startsWith(u'/')) {
        fullPath = cleanRel;
    } else {
        fullPath = m_searchRoot.endsWith(u'/')
            ? (m_searchRoot + cleanRel)
            : (m_searchRoot + u'/' + cleanRel);
    }

    QFileInfo fi(fullPath);
    if (!fi.exists()) {
        return RemoteMatch{};
    }

    const bool isDir = fi.isDir();
    const qsizetype lastSlash = cleanRel.lastIndexOf(u'/');
    QString text = (lastSlash == -1) ? cleanRel : cleanRel.sliced(lastSlash + 1);
    if (text.isEmpty()) {
        text = fullPath;
    }

    const QString icon = m_iconResolver.resolve(fullPath, isDir);
    const double relevance = (total <= 1) ? 1.0 : (1.0 - 0.5 * (static_cast<double>(rank) / static_cast<double>(total - 1)));
    const QString fileUri = QUrl::fromLocalFile(fullPath).toString();

    QVariantMap props;
    props.insert(s_subtextKey, fullPath);
    props.insert(s_categoryKey, s_categoryFiles);
    props.insert(s_urlsKey, QStringList{fileUri});
    props.insert(s_actionsKey, s_defaultActions);
    props.insert(s_multilineKey, false);

    return RemoteMatch{
        fullPath,
        text,
        icon,
        CategoryRelevance::High,
        relevance,
        std::move(props)
    };
}

RemoteMatch KSeekRunner::buildErrorMatch(const QString &message) const {
    QVariantMap props;
    props.insert(s_subtextKey, QString());
    props.insert(s_categoryKey, s_categoryError);
    props.insert(s_urlsKey, QStringList());
    props.insert(s_actionsKey, QStringList());
    props.insert(s_multilineKey, false);

    return RemoteMatch{
        QStringLiteral("__error__"),
        message,
        QStringLiteral("dialog-warning"),
        CategoryRelevance::Lowest,
        0.0,
        std::move(props)
    };
}

void KSeekRunner::onDebounceTimeout() {
    if (!m_pendingReplies.contains(m_pendingSearchReqId)) {
        return;
    }
    qCInfo(lcRunner) << "Launching debounced search for term:" << m_pendingSearchTerm << "reqId:" << m_pendingSearchReqId;
    m_pipeline.startSearch(m_pendingSearchReqId, m_searchRoot, m_pendingSearchTerm, m_extraFdArgs, m_extraFzfArgs, m_maxResults, m_timeoutMs);
}

void KSeekRunner::run(const QString &matchId, const QString &actionId) {
    m_actionHandler.execute(matchId, actionId);
}

void KSeekRunner::teardown() {
    m_debounceTimer.stop();
    m_pipeline.cancel();
}

void KSeekRunner::config() {
    m_debounceTimer.stop();
    m_pipeline.cancel();
}

void KSeekRunner::setActivationToken(const QString &token) {
    m_actionHandler.setActivationToken(token);
}
