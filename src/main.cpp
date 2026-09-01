#include <QCoreApplication>
#include <QSocketNotifier>
#include <QLoggingCategory>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusError>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>

#include "types.h"
#include "kseek_runner.h"
#include "runner_adaptor.h"

Q_LOGGING_CATEGORY(lcMain, "kseek.main")

static int sigFd[2];

static void signalHandler(int /*sig*/) {
    char a = 1;
    [[maybe_unused]] auto res = ::write(sigFd[0], &a, sizeof(a));
}

int main(int argc, char *argv[]) {
    // Headless QtCore application - zero GUI subsystem overhead
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kseek"));
    app.setApplicationVersion(QStringLiteral(KSEEK_VERSION));

    const bool debugLogging = qEnvironmentVariableIntValue("KSEEK_DEBUG") == 1;
    if (!debugLogging && qEnvironmentVariableIsEmpty("QT_LOGGING_RULES")) {
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

    KSeekRunner runner;
    new RunnerAdaptor(&runner);

    if (!bus.registerObject(QStringLiteral("/kseek"), &runner)) {
        qCCritical(lcMain) << "Failed to register D-Bus object at /kseek:" << bus.lastError().message();
        return 1;
    }

    if (!bus.registerService(QStringLiteral("org.kde.krunner.kseek"))) {
        qCCritical(lcMain) << "Failed to register D-Bus service org.kde.krunner.kseek:" << bus.lastError().message();
        return 1;
    }

    qCInfo(lcMain) << "kseek daemon registered successfully on D-Bus session bus";
    return app.exec();
}
