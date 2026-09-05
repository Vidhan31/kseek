#include "config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcessEnvironment>
#include <QCommandLineParser>
#include <QStringTokenizer>
#include <QStringView>
#include <QSet>
#include <QStandardPaths>

QStringList splitArgs(const QString &commandLine) {
    QStringList args;
    if (commandLine.trimmed().isEmpty()) {
        return args;
    }

    args.reserve(8);
    QString current;
    current.reserve(commandLine.size());

    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escapeNext = false;
    bool hasToken = false;

    const QStringView view(commandLine);
    for (qsizetype i = 0; i < view.size(); ++i) {
        const QChar c = view.at(i);

        if (escapeNext) {
            current.append(c);
            escapeNext = false;
            hasToken = true;
            continue;
        }

        if (c == u'\\' && !inSingleQuote) {
            escapeNext = true;
            hasToken = true;
            continue;
        }

        if (c == u'\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            hasToken = true;
            continue;
        }

        if (c == u'"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            hasToken = true;
            continue;
        }

        if (c.isSpace() && !inSingleQuote && !inDoubleQuote) {
            if (hasToken) {
                args.append(current);
                current.clear();
                hasToken = false;
            }
            continue;
        }

        current.append(c);
        hasToken = true;
    }

    if (escapeNext) {
        current.append(u'\\');
    }

    if (hasToken) {
        args.append(current);
    }

    return args;
}

QStringList parseSearchRoots(const QStringList &inputs) {
    QStringList roots;
    QSet<QString> seen;
    const QString home = QDir::homePath();

    for (const QString &rawInput : inputs) {
        const QStringView inputView = QStringView(rawInput).trimmed();
        if (inputView.isEmpty()) {
            continue;
        }

        for (auto part : QStringTokenizer{inputView, u':'}) {
            for (auto subPart : QStringTokenizer{part, u';'}) {
                const QStringView tokenView = subPart.trimmed();
                if (tokenView.isEmpty()) {
                    continue;
                }

                QString token;
                if (tokenView == u"~") {
                    token = home;
                } else if (tokenView.startsWith(QLatin1StringView("~/"))) {
                    token = home + tokenView.sliced(1).toString();
                } else {
                    token = tokenView.toString();
                }

                QFileInfo fi(token);
                if (fi.exists() && fi.isDir()) {
                    const QString canonical = fi.canonicalFilePath();
                    if (!canonical.isEmpty() && !seen.contains(canonical)) {
                        seen.insert(canonical);
                        roots.append(canonical);
                    }
                }
            }
        }
    }

    if (roots.isEmpty()) {
        const QString canonicalHome = QFileInfo(home).canonicalFilePath();
        roots.append(canonicalHome.isEmpty() ? home : canonicalHome);
    }

    return roots;
}

QStringList parseSearchRoots(const QString &input) {
    if (input.isEmpty()) {
        return parseSearchRoots(QStringList{});
    }
    return parseSearchRoots(QStringList{input});
}

QString KSeekConfig::defaultUserConfigPath() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (configDir.isEmpty()) {
        configDir = QDir::homePath() + QStringLiteral("/.config");
    }
    return configDir + QStringLiteral("/kseek/kseek.conf");
}

bool KSeekConfig::loadFromFile(const QString &filePath) {
    const QString targetPath = filePath.trimmed().isEmpty()
        ? defaultUserConfigPath()
        : filePath.trimmed();

    QFile file(targetPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const QFileInfo fi(targetPath);
    const QString canonical = fi.canonicalFilePath();
    configFilePath = canonical.isEmpty() ? targetPath : canonical;

    const QString home = QDir::homePath();
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(u'#') || line.startsWith(u';')) {
            continue;
        }
        if (line.startsWith(u'[') && line.endsWith(u']')) {
            continue;
        }

        qsizetype sepIdx = line.indexOf(u'=');
        if (sepIdx == -1) {
            sepIdx = line.indexOf(u':');
        }
        if (sepIdx == -1) {
            continue;
        }

        const QString key = line.left(sepIdx).trimmed().toLower();
        QString val = line.mid(sepIdx + 1).trimmed();

        if ((val.startsWith(u'"') && val.endsWith(u'"') && val.size() >= 2) ||
            (val.startsWith(u'\'') && val.endsWith(u'\'') && val.size() >= 2)) {
            val = val.mid(1, val.size() - 2);
        }

        if (val.contains(QLatin1StringView("$HOME"))) {
            val.replace(QStringLiteral("$HOME"), home);
        }

        bool ok = false;
        if (key == u"prefix" || key == u"trigger" || key == u"kseek_prefix" || key == u"kseek_trigger") {
            prefix = val;
        } else if (key == u"root" || key == u"roots" || key == u"kseek_root") {
            searchRoots = parseSearchRoots(val);
        } else if (key == u"max_results" || key == u"max-results" || key == u"maxresults" || key == u"kseek_max_results") {
            const int maxR = val.toInt(&ok);
            if (ok && maxR > 0) {
                maxResults = maxR;
            }
        } else if (key == u"timeout" || key == u"kseek_timeout") {
            const double timeoutSec = val.toDouble(&ok);
            if (ok && timeoutSec > 0.0) {
                timeoutMs = static_cast<int>(timeoutSec * 1000.0);
            }
        } else if (key == u"debounce" || key == u"kseek_debounce") {
            const int deb = val.toInt(&ok);
            if (ok && deb >= 0) {
                debounceMs = deb;
            }
        } else if (key == u"fd_args" || key == u"fd-args" || key == u"fdargs" || key == u"kseek_fd_args") {
            extraFdArgs = splitArgs(val);
        } else if (key == u"fzf_args" || key == u"fzf-args" || key == u"fzfargs" || key == u"kseek_fzf_args") {
            extraFzfArgs = splitArgs(val);
        } else if (key == u"fd_bin" || key == u"fd-bin" || key == u"fdbin" || key == u"kseek_fd_bin") {
            fdBin = val;
        } else if (key == u"fzf_bin" || key == u"fzf-bin" || key == u"fzfbin" || key == u"kseek_fzf_bin") {
            fzfBin = val;
        } else if (key == u"debug" || key == u"kseek_debug") {
            debug = (val == u"1" ||
                     val.compare(u"true", Qt::CaseInsensitive) == 0 ||
                     val.compare(u"yes", Qt::CaseInsensitive) == 0 ||
                     val.compare(u"on", Qt::CaseInsensitive) == 0);
        }
    }

    return true;
}

void KSeekConfig::loadEnvironment() {
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    if (env.contains(QStringLiteral("KSEEK_PREFIX"))) {
        prefix = env.value(QStringLiteral("KSEEK_PREFIX"));
    } else if (env.contains(QStringLiteral("KSEEK_TRIGGER"))) {
        prefix = env.value(QStringLiteral("KSEEK_TRIGGER"));
    } else if (env.contains(QStringLiteral("KRUNNER_FZF_FD_PREFIX"))) {
        prefix = env.value(QStringLiteral("KRUNNER_FZF_FD_PREFIX"));
    }

    if (env.contains(QStringLiteral("KSEEK_ROOT"))) {
        searchRoots = parseSearchRoots(env.value(QStringLiteral("KSEEK_ROOT")));
    } else if (env.contains(QStringLiteral("KRUNNER_FZF_FD_ROOT"))) {
        searchRoots = parseSearchRoots(env.value(QStringLiteral("KRUNNER_FZF_FD_ROOT")));
    }

    bool ok = false;
    QString maxResultsStr;
    if (env.contains(QStringLiteral("KSEEK_MAX_RESULTS"))) {
        maxResultsStr = env.value(QStringLiteral("KSEEK_MAX_RESULTS"));
    } else if (env.contains(QStringLiteral("KRUNNER_FZF_FD_MAX_RESULTS"))) {
        maxResultsStr = env.value(QStringLiteral("KRUNNER_FZF_FD_MAX_RESULTS"));
    }
    if (!maxResultsStr.isEmpty()) {
        const int maxR = maxResultsStr.toInt(&ok);
        if (ok && maxR > 0) {
            maxResults = maxR;
        }
    }

    QString timeoutStr;
    if (env.contains(QStringLiteral("KSEEK_TIMEOUT"))) {
        timeoutStr = env.value(QStringLiteral("KSEEK_TIMEOUT"));
    } else if (env.contains(QStringLiteral("KRUNNER_FZF_FD_TIMEOUT"))) {
        timeoutStr = env.value(QStringLiteral("KRUNNER_FZF_FD_TIMEOUT"));
    }
    if (!timeoutStr.isEmpty()) {
        const double timeoutSec = timeoutStr.toDouble(&ok);
        if (ok && timeoutSec > 0.0) {
            timeoutMs = static_cast<int>(timeoutSec * 1000.0);
        }
    }

    QString debounceStr;
    if (env.contains(QStringLiteral("KSEEK_DEBOUNCE"))) {
        debounceStr = env.value(QStringLiteral("KSEEK_DEBOUNCE"));
    } else if (env.contains(QStringLiteral("KRUNNER_FZF_FD_DEBOUNCE"))) {
        debounceStr = env.value(QStringLiteral("KRUNNER_FZF_FD_DEBOUNCE"));
    }
    if (!debounceStr.isEmpty()) {
        const int deb = debounceStr.toInt(&ok);
        if (ok && deb >= 0) {
            debounceMs = deb;
        }
    }

    if (env.contains(QStringLiteral("KSEEK_FD_ARGS"))) {
        extraFdArgs = splitArgs(env.value(QStringLiteral("KSEEK_FD_ARGS")));
    }

    if (env.contains(QStringLiteral("KSEEK_FZF_ARGS"))) {
        extraFzfArgs = splitArgs(env.value(QStringLiteral("KSEEK_FZF_ARGS")));
    }

    if (env.contains(QStringLiteral("KSEEK_FD_BIN"))) {
        fdBin = env.value(QStringLiteral("KSEEK_FD_BIN"));
    }

    if (env.contains(QStringLiteral("KSEEK_FZF_BIN"))) {
        fzfBin = env.value(QStringLiteral("KSEEK_FZF_BIN"));
    }

    if (env.contains(QStringLiteral("KSEEK_DEBUG"))) {
        debug = (env.value(QStringLiteral("KSEEK_DEBUG")) == QStringLiteral("1"));
    }
}

KSeekConfig KSeekConfig::load(const QString &configFilePath) {
    KSeekConfig cfg;
    cfg.loadFromFile(configFilePath);
    cfg.loadEnvironment();
    if (cfg.searchRoots.isEmpty()) {
        cfg.searchRoots = parseSearchRoots(QStringList{});
    }
    return cfg;
}

KSeekConfig KSeekConfig::loadFromEnvironment() {
    KSeekConfig cfg;
    cfg.loadEnvironment();
    if (cfg.searchRoots.isEmpty()) {
        cfg.searchRoots = parseSearchRoots(QStringList{});
    }
    return cfg;
}

void KSeekConfig::applyCommandLine(const QCommandLineParser &parser) {
    if (parser.isSet(QStringLiteral("prefix"))) {
        prefix = parser.value(QStringLiteral("prefix"));
    }
    if (parser.isSet(QStringLiteral("root"))) {
        const QStringList rootVals = parser.values(QStringLiteral("root"));
        const QStringList parsed = parseSearchRoots(rootVals);
        if (!parsed.isEmpty()) {
            searchRoots = parsed;
        }
    }
    if (parser.isSet(QStringLiteral("max-results"))) {
        bool ok = false;
        const int val = parser.value(QStringLiteral("max-results")).toInt(&ok);
        if (ok && val > 0) {
            maxResults = val;
        }
    }
    if (parser.isSet(QStringLiteral("timeout"))) {
        bool ok = false;
        const double val = parser.value(QStringLiteral("timeout")).toDouble(&ok);
        if (ok && val > 0.0) {
            timeoutMs = static_cast<int>(val * 1000.0);
        }
    }
    if (parser.isSet(QStringLiteral("debounce"))) {
        bool ok = false;
        const int val = parser.value(QStringLiteral("debounce")).toInt(&ok);
        if (ok && val >= 0) {
            debounceMs = val;
        }
    }
    if (parser.isSet(QStringLiteral("fd-args"))) {
        extraFdArgs = splitArgs(parser.value(QStringLiteral("fd-args")));
    }
    if (parser.isSet(QStringLiteral("fzf-args"))) {
        extraFzfArgs = splitArgs(parser.value(QStringLiteral("fzf-args")));
    }
    if (parser.isSet(QStringLiteral("fd-bin"))) {
        fdBin = parser.value(QStringLiteral("fd-bin"));
    }
    if (parser.isSet(QStringLiteral("fzf-bin"))) {
        fzfBin = parser.value(QStringLiteral("fzf-bin"));
    }
    if (parser.isSet(QStringLiteral("debug"))) {
        debug = true;
    }
    if (parser.isSet(QStringLiteral("replace"))) {
        replaceExisting = true;
    }
}
