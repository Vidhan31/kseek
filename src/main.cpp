#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QSocketNotifier>
#include <QLoggingCategory>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusError>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>

#include "types.h"
#include "config.h"
#include "kseek_runner.h"
#include "runner_adaptor.h"

Q_LOGGING_CATEGORY(lcMain, "kseek.main")

static int sigFd[2];

static void signalHandler(int /*sig*/) {
    char a = 1;
    [[maybe_unused]] auto res = ::write(sigFd[0], &a, sizeof(a));
}

static void cleanupLegacyInstallations() {
    const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString configHome = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);

    const QStringList legacyFiles = {
        configHome + QStringLiteral("/systemd/user/plasma-runner-kseek.service"),
        dataHome + QStringLiteral("/systemd/user/plasma-runner-kseek.service"),
        configHome + QStringLiteral("/systemd/user/plasma-runner-fzf-fd.service"),
        dataHome + QStringLiteral("/systemd/user/plasma-runner-fzf-fd.service"),
        dataHome + QStringLiteral("/krunner/dbusplugins/plasma-runner-fzf-fd.desktop"),
        dataHome + QStringLiteral("/dbus-1/services/org.kde.krunner.fzf_fd.service"),
    };

    const QStringList legacyDirs = {
        dataHome + QStringLiteral("/kseek"),            // Old Python install directory (~/.local/share/kseek/kseek.py)
        dataHome + QStringLiteral("/krunner-fzf-fd"),    // Predecessor directory
    };

    bool foundLegacy = false;
    for (const auto &f : legacyFiles) {
        if (QFile::exists(f)) {
            foundLegacy = true;
            break;
        }
    }
    if (!foundLegacy) {
        for (const auto &d : legacyDirs) {
            if (QDir(d).exists()) {
                foundLegacy = true;
                break;
            }
        }
    }

    if (!foundLegacy) {
        return;
    }

    qCInfo(lcMain) << "Detected legacy Python/systemd installation artifacts. Cleaning up for a fresh start...";

    // Stop and disable legacy systemd service if active
    QProcess::execute(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("stop"), QStringLiteral("plasma-runner-kseek.service")});
    QProcess::execute(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("disable"), QStringLiteral("plasma-runner-kseek.service")});

    // Terminate any running Python kseek processes
    QProcess::execute(QStringLiteral("pkill"), {QStringLiteral("-f"), QStringLiteral("kseek.py")});

    // Delete legacy files
    for (const auto &f : legacyFiles) {
        if (QFile::exists(f)) {
            if (QFile::remove(f)) {
                qCInfo(lcMain) << "Removed legacy file:" << f;
            }
        }
    }

    // Delete legacy directories
    for (const auto &d : legacyDirs) {
        QDir dir(d);
        if (dir.exists()) {
            if (dir.removeRecursively()) {
                qCInfo(lcMain) << "Removed legacy directory:" << d;
            }
        }
    }

    // Reload systemd user daemon so it clears stale unit definitions
    QProcess::execute(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("daemon-reload")});
    QProcess::execute(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("reset-failed")});
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kseek"));
    app.setApplicationVersion(QStringLiteral(KSEEK_VERSION));

    cleanupLegacyInstallations();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Fast fuzzy file search runner for KDE Plasma 6 using fd and fzf"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption prefixOpt(
        QStringList{QStringLiteral("p"), QStringLiteral("prefix")},
        QStringLiteral("Trigger prefix for queries (default: 'f', use '' or 'none' for prefixless)."),
        QStringLiteral("prefix")
    );
    parser.addOption(prefixOpt);

    QCommandLineOption rootOpt(
        QStringList{QStringLiteral("r"), QStringLiteral("root")},
        QStringLiteral("Root directory to search (can be specified multiple times or colon-separated, default: $HOME)."),
        QStringLiteral("path")
    );
    parser.addOption(rootOpt);

    QCommandLineOption maxResultsOpt(
        QStringList{QStringLiteral("m"), QStringLiteral("max-results")},
        QStringLiteral("Maximum results returned (default: 20)."),
        QStringLiteral("count")
    );
    parser.addOption(maxResultsOpt);

    QCommandLineOption timeoutOpt(
        QStringList{QStringLiteral("t"), QStringLiteral("timeout")},
        QStringLiteral("Query timeout in seconds (default: 2.5)."),
        QStringLiteral("seconds")
    );
    parser.addOption(timeoutOpt);

    QCommandLineOption debounceOpt(
        QStringList{QStringLiteral("d"), QStringLiteral("debounce")},
        QStringLiteral("Search debounce in milliseconds (default: 75)."),
        QStringLiteral("ms")
    );
    parser.addOption(debounceOpt);

    QCommandLineOption fdArgsOpt(
        QStringLiteral("fd-args"),
        QStringLiteral("Extra arguments passed to fd (e.g. \"--hidden --follow\")."),
        QStringLiteral("args")
    );
    parser.addOption(fdArgsOpt);

    QCommandLineOption fzfArgsOpt(
        QStringLiteral("fzf-args"),
        QStringLiteral("Extra arguments passed to fzf (e.g. \"--exact\")."),
        QStringLiteral("args")
    );
    parser.addOption(fzfArgsOpt);

    QCommandLineOption fdBinOpt(
        QStringLiteral("fd-bin"),
        QStringLiteral("Path to fd / fdfind executable."),
        QStringLiteral("path")
    );
    parser.addOption(fdBinOpt);

    QCommandLineOption fzfBinOpt(
        QStringLiteral("fzf-bin"),
        QStringLiteral("Path to fzf executable."),
        QStringLiteral("path")
    );
    parser.addOption(fzfBinOpt);

    QCommandLineOption replaceOpt(
        QStringLiteral("replace"),
        QStringLiteral("Replace an already running kseek instance on D-Bus.")
    );
    parser.addOption(replaceOpt);

    QCommandLineOption debugOpt(
        QStringLiteral("debug"),
        QStringLiteral("Enable verbose debug logging.")
    );
    parser.addOption(debugOpt);

    parser.process(app);

    KSeekConfig config = KSeekConfig::loadFromEnvironment();
    config.applyCommandLine(parser);

    if (!config.debug && qEnvironmentVariableIsEmpty("QT_LOGGING_RULES")) {
        QLoggingCategory::setFilterRules(QStringLiteral("kseek.*.info=false\nkseek.*.debug=false"));
    }

    registerMetaTypes();

    ::signal(SIGPIPE, SIG_IGN);

    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, sigFd)) {
        qCCritical(lcMain) << "Couldn't create socketpair for signal handling";
        return 1;
    }

    QSocketNotifier sn(sigFd[1], QSocketNotifier::Read);
    QObject::connect(&sn, &QSocketNotifier::activated, [&]() {
        sn.setEnabled(false);
        char tmp;
        [[maybe_unused]] auto res = ::read(sigFd[1], &tmp, sizeof(tmp));
        qCInfo(lcMain) << "Received termination signal, quitting kseek";
        QCoreApplication::quit();
    });

    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);

    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCCritical(lcMain) << "Cannot connect to the D-Bus session bus:" << bus.lastError().message();
        return 1;
    }

    KSeekRunner runner(config);
    new RunnerAdaptor(&runner);

    if (!bus.registerObject(QStringLiteral("/kseek"), &runner)) {
        qCCritical(lcMain) << "Failed to register D-Bus object at /kseek:" << bus.lastError().message();
        return 1;
    }

    const auto queueOption = config.replaceExisting
        ? QDBusConnectionInterface::ReplaceExistingService
        : QDBusConnectionInterface::DontQueueService;

    const auto registerResult = bus.interface()->registerService(
        QStringLiteral("org.kde.krunner.kseek"),
        queueOption,
        QDBusConnectionInterface::DontAllowReplacement
    );

    if (registerResult == QDBusConnectionInterface::ServiceNotRegistered) {
        qCCritical(lcMain) << "Failed to register D-Bus service org.kde.krunner.kseek (service already owned by another process). Use --replace to replace.";
        return 1;
    }

    qCInfo(lcMain) << "kseek daemon registered successfully on D-Bus session bus with prefix:" << runner.prefix();
    return app.exec();
}
