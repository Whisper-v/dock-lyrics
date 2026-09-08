#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

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

private Q_SLOTS:
    void onPropertiesChanged(const QString &interfaceName,
                             const QVariantMap &changedProperties,
                             const QStringList &invalidatedProperties);

private:
    QString m_service;
};
