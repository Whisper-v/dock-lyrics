#include "playerprobe.h"

#include <QDBusConnection>

PlayerProbe::PlayerProbe(const QString &service, QObject *parent)
    : QObject(parent)
    , m_service(service)
{
    QDBusConnection::sessionBus().connect(
        service,
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
}

void PlayerProbe::onPropertiesChanged(const QString &interfaceName,
                                      const QVariantMap &changedProperties,
                                      const QStringList &)
{
    if (interfaceName != QStringLiteral("org.mpris.MediaPlayer2.Player"))
        return;
    emit playerPropertiesChanged(m_service, changedProperties);
}
