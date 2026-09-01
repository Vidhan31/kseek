#pragma once

#include <QtDBus/QDBusAbstractAdaptor>
#include "types.h"

class KSeekRunner;

class RunnerAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.krunner1")
public:
    explicit RunnerAdaptor(KSeekRunner *parent);
    ~RunnerAdaptor() override = default;

public slots:
    RemoteActions Actions();
    RemoteMatches Match(const QString &query);
    void Run(const QString &matchId, const QString &actionId);
    void Teardown();
    void Config();
    void SetActivationToken(const QString &token);

private:
    KSeekRunner *runner() const;
};
