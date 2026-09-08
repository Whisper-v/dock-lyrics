#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class QDBusMessage;

// Wraps MPRIS D-Bus signal watching for one concrete player service so that
// we always know which player emitted PropertiesChanged.
class PlayerProbe : public QObject
{
    Q_OBJECT
public:
    explicit PlayerProbe(const QString &service, QObject *parent = nullptr);

    QString service() const { return m_service; }

Q_SIGNALS:
    // Forwarded only when the Player interface changed.
    void playerPropertiesChanged(const QString &service, const QVariantMap &changedProperties);
    // A seek happened (MPRIS Seeked signal): positionUs is the new position.
    void playerSeeked(const QString &service, qint64 positionUs);

private Q_SLOTS:
    // Receives the raw QDBusMessage: Qt cannot reliably demarshal the a{sv}
    // argument of PropertiesChanged on its own (nested maps arrive as
    // QDBusArgument), so we parse it manually.
    void onPropertiesChanged(const QDBusMessage &msg);
    void onSeeked(const QDBusMessage &msg);

private:
    QString m_service;
};
