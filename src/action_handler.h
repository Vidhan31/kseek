#pragma once

#include <QString>
#include <QObject>

class ActionHandler : public QObject {
    Q_OBJECT
public:
    explicit ActionHandler(QObject *parent = nullptr);
    ~ActionHandler() override = default;

    void setActivationToken(const QString &token) { m_activationToken = token; }
    QString activationToken() const { return m_activationToken; }

    void execute(const QString &matchId, const QString &actionId);

    void openApp(const QString &filePath);
    void showItem(const QString &filePath);
    void copyPath(const QString &filePath);
    void openTerminal(const QString &filePath);
    void notify(const QString &title, const QString &message);

private:
    QString m_activationToken;
};
