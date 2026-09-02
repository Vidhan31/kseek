#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QTimer>
#include <memory>

struct FdFeatures {
    bool searchPath = false;
};

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
    void setFdBinary(const QString &bin);
    QString fzfBinary() const { return m_fzfBin; }
    void setFzfBinary(const QString &bin);
    const FdFeatures &fdFeatures() const { return m_fdFeatures; }
    const FzfFeatures &features() const { return m_features; }

    void startSearch(quint64 requestId,
                     const QStringList &searchRoots,
                     const QString &query,
                     const QStringList &extraFdArgs = {},
                     const QStringList &extraFzfArgs = {},
                     int maxResults = 20,
                     int timeoutMs = 2500);

    void startSearch(quint64 requestId,
                     const QString &searchRoot,
                     const QString &query,
                     const QStringList &extraFdArgs = {},
                     const QStringList &extraFzfArgs = {},
                     int maxResults = 20,
                     int timeoutMs = 2500)
    {
        startSearch(requestId, QStringList{searchRoot}, query, extraFdArgs, extraFzfArgs, maxResults, timeoutMs);
    }

    void cancel();

signals:
    void searchFinished(quint64 requestId, const QStringList &relativePaths);
    void searchError(quint64 requestId, const QString &errorMessage);

private slots:
    void onFzfFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onTimeout();

private:
    void detectBinaries();
    void detectFdFeatures();
    void detectFzfFeatures();
    void killProcesses();

    QString m_fdBin;
    QString m_fzfBin;
    FdFeatures m_fdFeatures;
    FzfFeatures m_features;

    quint64 m_activeRequestId = 0;
    int m_maxResults = 20;
    bool m_useNullIo = false;

    std::unique_ptr<QProcess> m_fdProc;
    std::unique_ptr<QProcess> m_fzfProc;
    std::unique_ptr<QTimer> m_timer;
};
