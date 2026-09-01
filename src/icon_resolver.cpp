#include "icon_resolver.h"

IconResolver::IconResolver() {
    m_cache.reserve(512);
}

QString IconResolver::resolve(const QString &filePath, bool isDir) {
    if (isDir) {
        return QStringLiteral("inode-directory");
    }

    const qsizetype slashIdx = filePath.lastIndexOf(u'/');
    const QString fileName = (slashIdx == -1) ? filePath : filePath.sliced(slashIdx + 1);

    if (fileName.isEmpty()) {
        return QStringLiteral("application-octet-stream");
    }

    const qsizetype dotIdx = fileName.lastIndexOf(u'.');
    const QString cacheKey = (dotIdx != -1 && dotIdx != fileName.size() - 1)
        ? fileName.sliced(dotIdx).toLower()
        : fileName.toLower();

    {
        QReadLocker locker(&m_lock);
        auto it = m_cache.constFind(cacheKey);
        if (it != m_cache.constEnd()) {
            return *it;
        }
    }

    QMimeType mime = m_mimeDb.mimeTypeForFile(filePath, QMimeDatabase::MatchExtension);
    QString icon = mime.iconName();
    if (icon.isEmpty() || icon == u"application-octet-stream") {
        QString generic = mime.genericIconName();
        if (!generic.isEmpty()) {
            icon = generic;
        }
    }

    if (icon.isEmpty()) {
        icon = QStringLiteral("application-octet-stream");
    }

    {
        QWriteLocker locker(&m_lock);
        m_cache.insert(cacheKey, icon);
    }

    return icon;
}
