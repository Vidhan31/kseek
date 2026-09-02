#pragma once

#include <QString>
#include <QStringList>

class QCommandLineParser;

QStringList splitArgs(const QString &commandLine);
QStringList parseSearchRoots(const QStringList &inputs);
QStringList parseSearchRoots(const QString &input);

struct KSeekConfig {
    QString prefix = QStringLiteral("f");
    QStringList searchRoots;
    int maxResults = 20;
    int timeoutMs = 2500;
    int debounceMs = 75;
    QStringList extraFdArgs;
    QStringList extraFzfArgs;
    QString fdBin;
    QString fzfBin;
    bool debug = false;
    bool replaceExisting = false;

    QString searchRoot() const { return searchRoots.isEmpty() ? QString() : searchRoots.first(); }

    static KSeekConfig loadFromEnvironment();
    void applyCommandLine(const QCommandLineParser &parser);
};
