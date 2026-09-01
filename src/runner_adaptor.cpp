#include "runner_adaptor.h"
#include "kseek_runner.h"

RunnerAdaptor::RunnerAdaptor(KSeekRunner *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

KSeekRunner *RunnerAdaptor::runner() const {
    return qobject_cast<KSeekRunner *>(parent());
}

RemoteActions RunnerAdaptor::Actions() {
    return runner()->actions();
}

RemoteMatches RunnerAdaptor::Match(const QString &query) {
    if (runner()->calledFromDBus()) {
        runner()->setDelayedReply(true);
        const QDBusMessage replyMsg = runner()->message().createReply();
        runner()->matchAsync(query, replyMsg);
        return RemoteMatches{};
    }
    return RemoteMatches{};
}

void RunnerAdaptor::Run(const QString &matchId, const QString &actionId) {
    runner()->run(matchId, actionId);
}

void RunnerAdaptor::Teardown() {
    runner()->teardown();
}

void RunnerAdaptor::Config() {
    runner()->config();
}

void RunnerAdaptor::SetActivationToken(const QString &token) {
    runner()->setActivationToken(token);
}
