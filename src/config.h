#pragma once

#include <QString>
#include <QStringList>

class QCommandLineParser;

QStringList splitArgs(const QString &commandLine);

struct KSeekConfig {
    QString prefix = QStringLiteral("f");
    QString searchRoot;
    int maxResults = 20;
    int timeoutMs = 2500;
    int debounceMs = 75;
    QStringList extraFdArgs;
    QStringList extraFzfArgs;
    QString fdBin;
    QString fzfBin;
    bool debug = false;
    bool replaceExisting = false;

    static KSeekConfig loadFromEnvironment();
    void applyCommandLine(const QCommandLineParser &parser);
};
