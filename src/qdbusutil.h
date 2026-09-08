#pragma once

#include <QDBusArgument>
#include <QMetaType>
#include <QVariant>
#include <QVariantMap>

// QtDBus keeps nested a{sv} values wrapped as QDBusArgument (they are decoded
// lazily).  The one reliable way to honour the current read position is to
// stream them out with operator>>.  qdbus_cast / toMap do not work here.
inline QVariantMap qdbusVariantToMap(const QVariant &v)
{
    if (v.typeId() == QMetaType::QVariantMap)
        return v.toMap();
    if (v.metaType() == QMetaType::fromType<QDBusArgument>()) {
        QDBusArgument arg = v.value<QDBusArgument>();
        QVariantMap m;
        arg >> m;
        return m;
    }
    return v.toMap();
}
