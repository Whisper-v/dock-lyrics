#include "playerprobe.h"


#include "qdbusutil.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QVariantList>

PlayerProbe::PlayerProbe(const QString &service, QObject *parent)
    : QObject(parent)
    , m_service(service)
{
    // Subscribe without a sender filter: a filter bound to a well-known name
    // stops working after the owning process restarts, whereas an unfiltered
    // match keeps receiving. We filter by current owner in the slot instead.
    QDBusConnection::sessionBus().connect(
        QString(),
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(onPropertiesChanged(QDBusMessage)));
    QDBusConnection::sessionBus().connect(
        QString(),
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.mpris.MediaPlayer2.Player"),
        QStringLiteral("Seeked"),
        this,
        SLOT(onSeeked(QDBusMessage)));
}

void PlayerProbe::onPropertiesChanged(const QDBusMessage &msg)
{
    QDBusReply<QString> owner =
        QDBusConnection::sessionBus().interface()->serviceOwner(m_service);
    if (!owner.isValid() || owner.value() != msg.service())
        return;

    const QVariantList args = msg.arguments();
    if (args.size() < 2)
        return;
    const QString interfaceName = args.value(0).toString();
    if (interfaceName != QStringLiteral("org.mpris.MediaPlayer2.Player"))
        return;
    const QVariantMap changed = qdbusVariantToMap(args.value(1));
    if (!changed.isEmpty())
        emit playerPropertiesChanged(m_service, changed);
    if (args.size() >= 3) {
        const QStringList invalidated = args.value(2).toStringList();
        if (!invalidated.isEmpty())
            emit playerPropertiesInvalidated(m_service, invalidated);
    }
}

void PlayerProbe::onSeeked(const QDBusMessage &msg)
{
    QDBusReply<QString> owner =
        QDBusConnection::sessionBus().interface()->serviceOwner(m_service);
    if (!owner.isValid() || owner.value() != msg.service())
        return;

    const QVariantList args = msg.arguments();
    if (args.isEmpty())
        return;
    bool ok = false;
    const qint64 positionUs = args.value(0).toLongLong(&ok);
    if (ok && positionUs >= 0)
        emit playerSeeked(m_service, positionUs);
}
