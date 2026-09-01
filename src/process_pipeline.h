#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QTimer>
#include <memory>

struct FzfFeatures {
    bool read0 = false;
    bool print0 = false;
    bool schemePath = false;
    bool algoV2 = false;
    bool tiebreak = false;
};

class ProcessPipeline : public QObject {
    Q_OBJECT
public:
    explicit ProcessPipeline(QObject *parent = nullptr);
    ~ProcessPipeline() override;

    bool isAvailable() const;
    QString fdBinary() const { return m_fdBin; }
    QString fzfBinary() const { return m_fzfBin; }
    const FzfFeatures &features() const { return m_features; }

    void startSearch(quint64 requestId,
                     const QString &searchRoot,
                     const QString &query,
                     const QStringList &extraFdArgs = {},
                     const QStringList &extraFzfArgs = {},
                     int maxResults = 20,
                     int timeoutMs = 2500);

    void cancel();

signals:
    void searchFinished(quint64 requestId, const QStringList &relativePaths);
    void searchError(quint64 requestId, const QString &errorMessage);

private slots:
    void onFzfFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onTimeout();

private:
    void detectBinaries();
    void detectFzfFeatures();
    void killProcesses();

    QString m_fdBin;
    QString m_fzfBin;
    FzfFeatures m_features;

    quint64 m_activeRequestId = 0;
    int m_maxResults = 20;
    bool m_useNullIo = false;

    std::unique_ptr<QProcess> m_fdProc;
    std::unique_ptr<QProcess> m_fzfProc;
    std::unique_ptr<QTimer> m_timer;
};
