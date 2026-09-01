#include "kseek_runner.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QLoggingCategory>
#include <QtDBus/QDBusConnection>

Q_LOGGING_CATEGORY(lcRunner, "kseek.runner")

KSeekRunner::KSeekRunner(const KSeekConfig &config, QObject *parent)
    : QObject(parent),
      m_pipeline(this),
      m_actionHandler(this),
      m_debounceTimer(this)
{
    m_debounceTimer.setSingleShot(true);
    connect(&m_debounceTimer, &QTimer::timeout, this, &KSeekRunner::onDebounceTimeout);

    connect(&m_pipeline, &ProcessPipeline::searchFinished, this, &KSeekRunner::onPipelineFinished);
    connect(&m_pipeline, &ProcessPipeline::searchError, this, &KSeekRunner::onPipelineError);

    applyConfig(config);
}

KSeekRunner::KSeekRunner(const QString &searchRoot, QObject *parent)
    : KSeekRunner(KSeekConfig::loadFromEnvironment(), parent)
{
    if (!searchRoot.isEmpty()) {
        setSearchRoot(searchRoot);
    }
}

KSeekRunner::~KSeekRunner() {
    teardown();
}

void KSeekRunner::applyConfig(const KSeekConfig &cfg) {
    m_config = cfg;
    setPrefix(cfg.prefix);
    setSearchRoot(cfg.searchRoot);
    setMaxResults(cfg.maxResults);
    setTimeoutMs(cfg.timeoutMs);
    setDebounceMs(cfg.debounceMs);
    setExtraFdArgs(cfg.extraFdArgs);
    setExtraFzfArgs(cfg.extraFzfArgs);

    if (!cfg.fdBin.isEmpty()) {
        m_pipeline.setFdBinary(cfg.fdBin);
    }
    if (!cfg.fzfBin.isEmpty()) {
        m_pipeline.setFzfBinary(cfg.fzfBin);
    }
}

void KSeekRunner::setPrefix(const QString &prefix) {
    const QString trimmed = prefix.trimmed();
    if (trimmed.isEmpty() || trimmed == u"-" || trimmed.compare(u"none", Qt::CaseInsensitive) == 0) {
        m_prefix.clear();
        m_queryRegex = QRegularExpression();
    } else {
        m_prefix = trimmed;
        const QString escaped = QRegularExpression::escape(m_prefix);
        const QChar lastChar = m_prefix.at(m_prefix.size() - 1);
        QString pattern;
        if (!lastChar.isLetterOrNumber()) {
            pattern = QStringLiteral(R"(^%1\s*(.+)$)").arg(escaped);
        } else {
            pattern = QStringLiteral(R"(^%1(?:\s+|:\s*)(.+)$)").arg(escaped);
        }
        m_queryRegex = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
    }
}

bool KSeekRunner::parseQuery(const QString &query, QString &searchTerm) const {
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    if (m_prefix.isEmpty()) {
        searchTerm = trimmed;
        return true;
    }

    const auto match = m_queryRegex.match(trimmed);
    if (!match.hasMatch()) {
        return false;
    }

    searchTerm = match.captured(1).trimmed();
    return !searchTerm.isEmpty();
}

void KSeekRunner::setSearchRoot(const QString &root) {
    QFileInfo fi(root);
    if (fi.isDir()) {
        m_searchRoot = fi.canonicalFilePath();
    } else {
        m_searchRoot = QDir::homePath();
    }
    m_config.searchRoot = m_searchRoot;
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

    QString searchTerm;
    if (!parseQuery(query, searchTerm)) {
        qCInfo(lcRunner) << "Query ignored (does not match prefix or is empty):" << query;
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
