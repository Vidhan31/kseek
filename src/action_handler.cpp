#include "action_handler.h"

#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusConnection>

Q_LOGGING_CATEGORY(lcActions, "kseek.actions")

ActionHandler::ActionHandler(QObject *parent)
    : QObject(parent)
{
}

void ActionHandler::execute(const QString &matchId, const QString &actionId) {
    if (matchId == u"__error__" || matchId.isEmpty()) {
        return;
    }

    const QString filePath = matchId;
    if (!QFileInfo::exists(filePath) && actionId != u"copy_path") {
        qCWarning(lcActions) << "File no longer exists:" << filePath;
        notify(QStringLiteral("File Not Found"), filePath);
        return;
    }

    if (actionId.isEmpty() || actionId == u"open_app") {
        openApp(filePath);
    } else if (actionId == u"show_item") {
        showItem(filePath);
    } else if (actionId == u"copy_path") {
        copyPath(filePath);
    } else if (actionId == u"open_terminal") {
        openTerminal(filePath);
    } else {
        qCWarning(lcActions) << "Unknown action requested:" << actionId;
    }
}

void ActionHandler::openApp(const QString &filePath) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!m_activationToken.isEmpty()) {
        env.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), m_activationToken);
    }

    const QString xdgOpen = QStandardPaths::findExecutable(QStringLiteral("xdg-open"));
    if (!xdgOpen.isEmpty()) {
        QProcess process;
        process.setProgram(xdgOpen);
        process.setArguments({filePath});
        process.setProcessEnvironment(env);
        if (process.startDetached()) {
            return;
        }
    }

    const QString gio = QStandardPaths::findExecutable(QStringLiteral("gio"));
    if (!gio.isEmpty()) {
        QProcess process;
        process.setProgram(gio);
        process.setArguments({QStringLiteral("open"), filePath});
        process.setProcessEnvironment(env);
        if (process.startDetached()) {
            return;
        }
    }

    qCWarning(lcActions) << "Neither xdg-open nor gio found to open" << filePath;
}

void ActionHandler::showItem(const QString &filePath) {
    const QString fileUri = QUrl::fromLocalFile(filePath).toString();

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.FileManager1"),
        QStringLiteral("/org/freedesktop/FileManager1"),
        QStringLiteral("org.freedesktop.FileManager1"),
        QStringLiteral("ShowItems"));

    msg << QStringList{fileUri} << (m_activationToken.isEmpty() ? QStringLiteral("") : m_activationToken);

    QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 1500);
    if (reply.type() == QDBusMessage::ReplyMessage) {
        return;
    }

    qCDebug(lcActions) << "FileManager1.ShowItems failed; opening folder directly";

    QFileInfo fi(filePath);
    const QString folder = fi.isDir() ? filePath : fi.dir().absolutePath();
    openApp(folder);
}

void ActionHandler::copyPath(const QString &filePath) {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.klipper"),
        QStringLiteral("/klipper"),
        QStringLiteral("org.kde.klipper.klipper"),
        QStringLiteral("setClipboardContents"));

    msg << filePath;

    QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 1000);
    if (reply.type() == QDBusMessage::ReplyMessage) {
        return;
    }

    qCDebug(lcActions) << "Klipper D-Bus unavailable; trying CLI clipboard tools";

    struct ClipTool {
        QString name;
        QStringList args;
    };

    const ClipTool tools[] = {
        {QStringLiteral("wl-copy"), {}},
        {QStringLiteral("xclip"), {QStringLiteral("-selection"), QStringLiteral("clipboard")}},
        {QStringLiteral("xsel"), {QStringLiteral("--clipboard")}}
    };

    for (const auto &tool : tools) {
        const QString bin = QStandardPaths::findExecutable(tool.name);
        if (bin.isEmpty()) {
            continue;
        }

        QProcess proc;
        proc.start(bin, tool.args);
        if (proc.waitForStarted(500)) {
            proc.write(filePath.toUtf8());
            proc.closeWriteChannel();
            if (proc.waitForFinished(1000) && proc.exitCode() == 0) {
                return;
            }
        }
    }

    qCWarning(lcActions) << "No clipboard mechanism succeeded for path:" << filePath;
}

void ActionHandler::openTerminal(const QString &filePath) {
    QFileInfo fi(filePath);
    const QString targetDir = fi.isDir() ? filePath : fi.dir().absolutePath();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!m_activationToken.isEmpty()) {
        env.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), m_activationToken);
    }

    const QString customTerm = env.value(QStringLiteral("TERMINAL"));
    if (!customTerm.isEmpty()) {
        const QString termBin = QStandardPaths::findExecutable(customTerm);
        if (!termBin.isEmpty()) {
            QProcess process;
            process.setProgram(termBin);
            process.setProcessEnvironment(env);
            process.setWorkingDirectory(targetDir);
            if (process.startDetached()) {
                return;
            }
        }
    }

    struct TermCandidate {
        QString name;
        QStringList prefixArgs;
        QString dirFlag;
    };

    const TermCandidate candidates[] = {
        {QStringLiteral("xdg-terminal-exec"), {}, QString()},
        {QStringLiteral("konsole"), {}, QStringLiteral("--workdir")},
        {QStringLiteral("ptyxis"), {}, QStringLiteral("--working-directory")},
        {QStringLiteral("ghostty"), {}, QStringLiteral("--working-directory")},
        {QStringLiteral("foot"), {}, QStringLiteral("--working-directory")},
        {QStringLiteral("kitty"), {}, QStringLiteral("--directory")},
        {QStringLiteral("wezterm"), {QStringLiteral("start"), QStringLiteral("--cwd")}, QString()},
        {QStringLiteral("alacritty"), {}, QStringLiteral("--working-directory")},
        {QStringLiteral("gnome-terminal"), {}, QString()},
        {QStringLiteral("xterm"), {}, QString()}
    };

    for (const auto &c : candidates) {
        const QString bin = QStandardPaths::findExecutable(c.name);
        if (bin.isEmpty()) {
            continue;
        }

        QStringList args = c.prefixArgs;
        if (!c.dirFlag.isEmpty()) {
            args << c.dirFlag << targetDir;
        } else if (!c.prefixArgs.isEmpty()) {
            args << targetDir;
        }

        QProcess process;
        process.setProgram(bin);
        process.setArguments(args);
        process.setProcessEnvironment(env);
        process.setWorkingDirectory(targetDir);
        if (process.startDetached()) {
            return;
        }
    }

    qCWarning(lcActions) << "No suitable terminal emulator found on PATH";
}

void ActionHandler::notify(const QString &title, const QString &message) {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("Notify"));

    msg << QStringLiteral("kseek File Search")
        << 0u
        << QStringLiteral("dialog-error")
        << title
        << message
        << QStringList()
        << QVariantMap()
        << 4000;

    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}
