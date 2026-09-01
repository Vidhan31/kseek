#pragma once

#include <QString>
#include <QHash>
#include <QMimeDatabase>
#include <QReadWriteLock>

class IconResolver {
public:
    IconResolver();
    ~IconResolver() = default;

    // Non-copyable
    IconResolver(const IconResolver &) = delete;
    IconResolver &operator=(const IconResolver &) = delete;

    QString resolve(const QString &filePath, bool isDir);

private:
    QMimeDatabase m_mimeDb;
    QHash<QString, QString> m_cache;
    mutable QReadWriteLock m_lock;
};
