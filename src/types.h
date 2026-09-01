#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QVariantMap>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMetaType>

// RemoteAction: (sss) -> (id, text, icon)
struct RemoteAction {
    QString id;
    QString text;
    QString icon;

    bool operator==(const RemoteAction &other) const = default;
};
using RemoteActions = QList<RemoteAction>;

Q_DECLARE_METATYPE(RemoteAction)
Q_DECLARE_METATYPE(RemoteActions)

inline QDBusArgument &operator<<(QDBusArgument &arg, const RemoteAction &act) {
    arg.beginStructure();
    arg << act.id << act.text << act.icon;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument &operator>>(const QDBusArgument &arg, RemoteAction &act) {
    arg.beginStructure();
    arg >> act.id >> act.text >> act.icon;
    arg.endStructure();
    return arg;
}

namespace CategoryRelevance {
    constexpr int Lowest = 0;
    constexpr int Low = 30;
    constexpr int Moderate = 50;
    constexpr int High = 70;
    constexpr int Highest = 100;
}

// RemoteMatch: (sssida{sv}) -> (id, text, icon, categoryRelevance, relevance, properties)
struct RemoteMatch {
    QString id;
    QString text;
    QString icon;
    int type = CategoryRelevance::High;
    double relevance = 0.0;
    QVariantMap properties;

    bool operator==(const RemoteMatch &other) const = default;
};
using RemoteMatches = QList<RemoteMatch>;

Q_DECLARE_METATYPE(RemoteMatch)
Q_DECLARE_METATYPE(RemoteMatches)

inline QDBusArgument &operator<<(QDBusArgument &arg, const RemoteMatch &match) {
    arg.beginStructure();
    arg << match.id << match.text << match.icon << match.type << match.relevance << match.properties;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument &operator>>(const QDBusArgument &arg, RemoteMatch &match) {
    arg.beginStructure();
    arg >> match.id >> match.text >> match.icon >> match.type >> match.relevance >> match.properties;
    arg.endStructure();
    return arg;
}

inline void registerMetaTypes() {
    qDBusRegisterMetaType<RemoteAction>();
    qDBusRegisterMetaType<RemoteActions>();
    qDBusRegisterMetaType<RemoteMatch>();
    qDBusRegisterMetaType<RemoteMatches>();
}
