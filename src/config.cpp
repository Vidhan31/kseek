#include "config.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QCommandLineParser>
#include <QStringTokenizer>
#include <QStringView>
#include <QSet>

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

KSeekConfig KSeekConfig::loadFromEnvironment() {
    KSeekConfig cfg;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // Prefix: check KSEEK_PREFIX, fallback to KSEEK_TRIGGER, fallback to KRUNNER_FZF_FD_PREFIX
    if (env.contains(QStringLiteral("KSEEK_PREFIX"))) {
        cfg.prefix = env.value(QStringLiteral("KSEEK_PREFIX"));
    } else if (env.contains(QStringLiteral("KSEEK_TRIGGER"))) {
        cfg.prefix = env.value(QStringLiteral("KSEEK_TRIGGER"));
    } else if (env.contains(QStringLiteral("KRUNNER_FZF_FD_PREFIX"))) {
        cfg.prefix = env.value(QStringLiteral("KRUNNER_FZF_FD_PREFIX"));
    } else {
        cfg.prefix = QStringLiteral("f");
    }

    // Search roots: check KSEEK_ROOT, fallback to KRUNNER_FZF_FD_ROOT, fallback to home
    QString root = env.value(QStringLiteral("KSEEK_ROOT"));
    if (root.isEmpty()) {
        root = env.value(QStringLiteral("KRUNNER_FZF_FD_ROOT"));
    }
    cfg.searchRoots = parseSearchRoots(root);

    // Max results: check KSEEK_MAX_RESULTS, fallback to KRUNNER_FZF_FD_MAX_RESULTS
    bool ok = false;
    QString maxResultsStr = env.value(QStringLiteral("KSEEK_MAX_RESULTS"));
    if (maxResultsStr.isEmpty()) {
        maxResultsStr = env.value(QStringLiteral("KRUNNER_FZF_FD_MAX_RESULTS"));
    }
    if (!maxResultsStr.isEmpty()) {
        const int maxResults = maxResultsStr.toInt(&ok);
        if (ok && maxResults > 0) {
            cfg.maxResults = maxResults;
        }
    }

    // Timeout: check KSEEK_TIMEOUT, fallback to KRUNNER_FZF_FD_TIMEOUT
    QString timeoutStr = env.value(QStringLiteral("KSEEK_TIMEOUT"));
    if (timeoutStr.isEmpty()) {
        timeoutStr = env.value(QStringLiteral("KRUNNER_FZF_FD_TIMEOUT"));
    }
    if (!timeoutStr.isEmpty()) {
        const double timeoutSec = timeoutStr.toDouble(&ok);
        if (ok && timeoutSec > 0.0) {
            cfg.timeoutMs = static_cast<int>(timeoutSec * 1000.0);
        }
    }

    // Debounce: check KSEEK_DEBOUNCE, fallback to KRUNNER_FZF_FD_DEBOUNCE
    QString debounceStr = env.value(QStringLiteral("KSEEK_DEBOUNCE"));
    if (debounceStr.isEmpty()) {
        debounceStr = env.value(QStringLiteral("KRUNNER_FZF_FD_DEBOUNCE"));
    }
    if (!debounceStr.isEmpty()) {
        const int debounce = debounceStr.toInt(&ok);
        if (ok && debounce >= 0) {
            cfg.debounceMs = debounce;
        }
    }

    // Extra fd / fzf args
    const QString extraFdStr = env.value(QStringLiteral("KSEEK_FD_ARGS"));
    if (!extraFdStr.isEmpty()) {
        cfg.extraFdArgs = splitArgs(extraFdStr);
    }

    const QString extraFzfStr = env.value(QStringLiteral("KSEEK_FZF_ARGS"));
    if (!extraFzfStr.isEmpty()) {
        cfg.extraFzfArgs = splitArgs(extraFzfStr);
    }

    // Custom binary paths
    cfg.fdBin = env.value(QStringLiteral("KSEEK_FD_BIN"));
    cfg.fzfBin = env.value(QStringLiteral("KSEEK_FZF_BIN"));

    // Debug logging
    cfg.debug = env.value(QStringLiteral("KSEEK_DEBUG")) == QStringLiteral("1");

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
