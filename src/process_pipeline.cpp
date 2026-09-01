#include "process_pipeline.h"

#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QLoggingCategory>
#include <unistd.h>
#include <signal.h>

Q_LOGGING_CATEGORY(lcPipeline, "kseek.pipeline")

ProcessPipeline::ProcessPipeline(QObject *parent)
    : QObject(parent),
      m_timer(std::make_unique<QTimer>(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer.get(), &QTimer::timeout, this, &ProcessPipeline::onTimeout);

    detectBinaries();
    detectFzfFeatures();
}

ProcessPipeline::~ProcessPipeline() {
    killProcesses();
}

bool ProcessPipeline::isAvailable() const {
    return !m_fdBin.isEmpty() && !m_fzfBin.isEmpty();
}

void ProcessPipeline::detectBinaries() {
    const QString customFd = qEnvironmentVariable("KSEEK_FD_BIN");
    if (!customFd.isEmpty() && QFileInfo(customFd).isExecutable()) {
        m_fdBin = customFd;
    } else {
        m_fdBin = QStandardPaths::findExecutable(QStringLiteral("fd"));
        if (m_fdBin.isEmpty()) {
            m_fdBin = QStandardPaths::findExecutable(QStringLiteral("fdfind"));
        }
        if (m_fdBin.isEmpty()) {
            const QString home = QDir::homePath();
            const QStringList fallbackPaths = {
                home + QStringLiteral("/.local/bin/fd"),
                home + QStringLiteral("/.cargo/bin/fd"),
                home + QStringLiteral("/.local/bin/fdfind")
            };
            for (const auto &p : fallbackPaths) {
                if (QFileInfo(p).isExecutable()) {
                    m_fdBin = p;
                    break;
                }
            }
        }
    }

    const QString customFzf = qEnvironmentVariable("KSEEK_FZF_BIN");
    if (!customFzf.isEmpty() && QFileInfo(customFzf).isExecutable()) {
        m_fzfBin = customFzf;
    } else {
        m_fzfBin = QStandardPaths::findExecutable(QStringLiteral("fzf"));
        if (m_fzfBin.isEmpty()) {
            const QString home = QDir::homePath();
            const QStringList fallbackPaths = {
                home + QStringLiteral("/.local/bin/fzf"),
                home + QStringLiteral("/.cargo/bin/fzf"),
                home + QStringLiteral("/.fzf/bin/fzf")
            };
            for (const auto &p : fallbackPaths) {
                if (QFileInfo(p).isExecutable()) {
                    m_fzfBin = p;
                    break;
                }
            }
        }
    }

    if (m_fdBin.isEmpty()) {
        qCWarning(lcPipeline) << "Neither 'fd' nor 'fdfind' found on PATH or custom locations";
    }
    if (m_fzfBin.isEmpty()) {
        qCWarning(lcPipeline) << "'fzf' not found on PATH or custom locations";
    }
}

void ProcessPipeline::detectFzfFeatures() {
    if (m_fzfBin.isEmpty()) {
        return;
    }

    QProcess fzfHelp;
    fzfHelp.start(m_fzfBin, {QStringLiteral("--help")});
    if (fzfHelp.waitForFinished(2000)) {
        const QString out = QString::fromUtf8(fzfHelp.readAllStandardOutput() + fzfHelp.readAllStandardError());
        m_features.read0 = out.contains(QLatin1StringView("--read0"));
        m_features.print0 = out.contains(QLatin1StringView("--print0"));
        m_features.schemePath = out.contains(QLatin1StringView("--scheme"));
        m_features.algoV2 = out.contains(QLatin1StringView("--algo"));
        m_features.tiebreak = out.contains(QLatin1StringView("--tiebreak"));
    }
}

void ProcessPipeline::startSearch(quint64 requestId,
                                 const QString &searchRoot,
                                 const QString &query,
                                 const QStringList &extraFdArgs,
                                 const QStringList &extraFzfArgs,
                                 int maxResults,
                                 int timeoutMs)
{
    if (!isAvailable()) {
        if (m_fdBin.isEmpty()) {
            emit searchError(requestId, QStringLiteral("'fd' is not installed or not on PATH"));
        } else {
            emit searchError(requestId, QStringLiteral("'fzf' is not installed or not on PATH"));
        }
        return;
    }

    killProcesses();

    m_activeRequestId = requestId;
    m_maxResults = maxResults;
    m_useNullIo = m_features.read0 && m_features.print0;

    QStringList fdArgs;
    fdArgs << QStringLiteral("--color=never") << QStringLiteral("--base-directory") << searchRoot;
    if (!extraFdArgs.isEmpty()) {
        fdArgs.append(extraFdArgs);
    }
    if (m_useNullIo) {
        fdArgs << QStringLiteral("--print0");
    }

    QStringList fzfArgs;
    fzfArgs << (QStringLiteral("--filter=") + query);
    if (m_useNullIo) {
        fzfArgs << QStringLiteral("--read0") << QStringLiteral("--print0");
    }
    if (m_features.schemePath) {
        fzfArgs << QStringLiteral("--scheme=path");
    }
    if (m_features.algoV2) {
        fzfArgs << QStringLiteral("--algo=v2");
    }
    if (m_features.tiebreak) {
        fzfArgs << QStringLiteral("--tiebreak=length,begin,index");
    }
    if (!extraFzfArgs.isEmpty()) {
        fzfArgs.append(extraFzfArgs);
    }

    m_fdProc = std::make_unique<QProcess>();
    m_fzfProc = std::make_unique<QProcess>();

    m_fdProc->setWorkingDirectory(searchRoot);
    m_fzfProc->setWorkingDirectory(searchRoot);

    // Sanitize fzf environment from user shell terminal options
    QProcessEnvironment fzfEnv = QProcessEnvironment::systemEnvironment();
    fzfEnv.remove(QStringLiteral("FZF_DEFAULT_OPTS"));
    fzfEnv.remove(QStringLiteral("FZF_DEFAULT_COMMAND"));
    fzfEnv.remove(QStringLiteral("FZF_DEFAULT_OPTS_FILE"));
    m_fzfProc->setProcessEnvironment(fzfEnv);

    auto childModifier = []() {
        ::setpgid(0, 0);
    };
    m_fdProc->setChildProcessModifier(childModifier);
    m_fzfProc->setChildProcessModifier(childModifier);

    m_fdProc->setStandardOutputProcess(m_fzfProc.get());

    connect(m_fzfProc.get(), &QProcess::finished, this, &ProcessPipeline::onFzfFinished);

    auto errorHandler = [this, requestId](QProcess::ProcessError error) {
        qCWarning(lcPipeline) << "Subprocess error occurred for requestId:" << requestId << "error:" << error;
        killProcesses();
        emit searchError(requestId, QStringLiteral("Process execution failed"));
    };
    connect(m_fdProc.get(), &QProcess::errorOccurred, this, errorHandler);
    connect(m_fzfProc.get(), &QProcess::errorOccurred, this, errorHandler);

    m_timer->start(timeoutMs > 0 ? timeoutMs : 2500);

    qCInfo(lcPipeline) << "Running:" << m_fdBin << fdArgs.join(u' ') << "|" << m_fzfBin << fzfArgs.join(u' ');
    m_fdProc->start(m_fdBin, fdArgs, QIODevice::ReadOnly);
    m_fzfProc->start(m_fzfBin, fzfArgs, QIODevice::ReadOnly);
}

void ProcessPipeline::cancel() {
    killProcesses();
}

void ProcessPipeline::killProcesses() {
    if (m_timer && m_timer->isActive()) {
        m_timer->stop();
    }

    if (m_fdProc) {
        m_fdProc->disconnect(this);
        if (m_fdProc->state() != QProcess::NotRunning) {
            qint64 pid = m_fdProc->processId();
            if (pid > 0) {
                ::kill(-static_cast<pid_t>(pid), SIGKILL);
            }
            m_fdProc->kill();
            m_fdProc->waitForFinished(50);
        }
        m_fdProc.reset();
    }

    if (m_fzfProc) {
        m_fzfProc->disconnect(this);
        if (m_fzfProc->state() != QProcess::NotRunning) {
            qint64 pid = m_fzfProc->processId();
            if (pid > 0) {
                ::kill(-static_cast<pid_t>(pid), SIGKILL);
            }
            m_fzfProc->kill();
            m_fzfProc->waitForFinished(50);
        }
        m_fzfProc.reset();
    }
}

void ProcessPipeline::onFzfFinished(int exitCode, QProcess::ExitStatus /*exitStatus*/) {
    if (m_timer) {
        m_timer->stop();
    }

    const quint64 reqId = m_activeRequestId;

    if (!m_fzfProc) {
        return;
    }

    const QByteArray rawOutput = m_fzfProc->readAllStandardOutput();
    const QByteArray rawStderr = m_fzfProc->readAllStandardError();

    qCInfo(lcPipeline) << "fzf finished exitCode:" << exitCode << "output bytes:" << rawOutput.size();

    // 0 = matches found, 1 = no matches found (valid fzf exit code)
    if (exitCode != 0 && exitCode != 1) {
        const QString err = QString::fromUtf8(rawStderr).trimmed();
        qCWarning(lcPipeline) << "fzf exited with code" << exitCode << ":" << err;
        killProcesses();
        emit searchError(reqId, QStringLiteral("fzf error: ") + err.left(120));
        return;
    }

    QStringList paths;
    if (!rawOutput.isEmpty()) {
        const char delimiter = m_useNullIo ? '\0' : '\n';
        int start = 0;
        const int size = rawOutput.size();
        const char *data = rawOutput.constData();

        for (int i = 0; i <= size && paths.size() < m_maxResults; ++i) {
            if (i == size || data[i] == delimiter) {
                if (i > start) {
                    paths.append(QString::fromUtf8(data + start, i - start));
                }
                start = i + 1;
            }
        }
    }

    qCInfo(lcPipeline) << "Parsed" << paths.size() << "paths from fzf output";
    killProcesses();
    emit searchFinished(reqId, paths);
}

void ProcessPipeline::onTimeout() {
    qCWarning(lcPipeline) << "Search request" << m_activeRequestId << "timed out";
    const quint64 reqId = m_activeRequestId;
    killProcesses();
    emit searchFinished(reqId, QStringList());
}
