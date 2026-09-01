#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QHash>
#include <QTimer>
#include <QtDBus/QDBusMessage>

#include "types.h"
#include "icon_resolver.h"
#include "process_pipeline.h"
#include "action_handler.h"
#include <QtDBus/QDBusContext>

class KSeekRunner : public QObject, public QDBusContext {
    Q_OBJECT
public:
    explicit KSeekRunner(const QString &searchRoot = QString(), QObject *parent = nullptr);
    ~KSeekRunner() override;

    RemoteActions actions() const;
    void matchAsync(const QString &query, const QDBusMessage &replyMessage);
    void run(const QString &matchId, const QString &actionId);
    void teardown();
    void config();
    void setActivationToken(const QString &token);

    // Configuration accessors for testing & inspection
    QString searchRoot() const { return m_searchRoot; }
    void setSearchRoot(const QString &root);
    int maxResults() const { return m_maxResults; }
    void setMaxResults(int count) { m_maxResults = count; }
    int timeoutMs() const { return m_timeoutMs; }
    void setTimeoutMs(int ms) { m_timeoutMs = ms; }
    int debounceMs() const { return m_debounceMs; }
    void setDebounceMs(int ms) { m_debounceMs = ms; }
    QStringList extraFdArgs() const { return m_extraFdArgs; }
    void setExtraFdArgs(const QStringList &args) { m_extraFdArgs = args; }
    QStringList extraFzfArgs() const { return m_extraFzfArgs; }
    void setExtraFzfArgs(const QStringList &args) { m_extraFzfArgs = args; }

    ProcessPipeline &pipeline() { return m_pipeline; }
    IconResolver &iconResolver() { return m_iconResolver; }
    ActionHandler &actionHandler() { return m_actionHandler; }

    RemoteMatch buildMatch(const QString &relPath, int rank, int total);
    RemoteMatch buildErrorMatch(const QString &message) const;

private slots:
    void onPipelineFinished(quint64 requestId, const QStringList &relativePaths);
    void onPipelineError(quint64 requestId, const QString &errorMessage);
    void onDebounceTimeout();

private:
    void loadEnvironment();
    void sendReply(quint64 requestId, const RemoteMatches &matches);

    QString m_searchRoot;
    int m_maxResults = 20;
    int m_timeoutMs = 2500;
    int m_debounceMs = 75;
    QStringList m_extraFdArgs;
    QStringList m_extraFzfArgs;
    QRegularExpression m_queryRegex;

    ProcessPipeline m_pipeline;
    IconResolver m_iconResolver;
    ActionHandler m_actionHandler;

    QTimer m_debounceTimer;
    quint64 m_pendingSearchReqId = 0;
    QString m_pendingSearchTerm;

    quint64 m_currentRequestId = 0;
    QHash<quint64, QDBusMessage> m_pendingReplies;
};
