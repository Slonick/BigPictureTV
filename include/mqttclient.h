#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QObject>
#include <QQmlEngine>

class QMqttClient;

// Publishes the current Big Picture / desktop state to an MQTT broker so Home
// Assistant can react (e.g. turn the TV on the right input when gamemode
// starts). Uses MQTT Discovery so BigPictureTV shows up automatically as a
// binary_sensor without any manual YAML on the HA side.
class MqttClient : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY statusChanged)

public:
    static MqttClient* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static MqttClient* instance();

    QString statusText() const { return m_statusText; }
    bool connected() const;

    // Connect now with the current settings and publish discovery + state.
    // Also re-runs whenever the broker settings are saved from the UI.
    Q_INVOKABLE void testConnection();

    // Publish an ON/OFF state without actually entering Big Picture, so the
    // user can confirm their HA automation fires.
    Q_INVOKABLE void simulateState(bool gamemode);

signals:
    void statusChanged();

private slots:
    void onGamemodeChanged();
    void onMqttEnabledChanged();
    void onConnected();
    void onDisconnected();
    void onErrorChanged();

private:
    explicit MqttClient(QObject *parent = nullptr);
    ~MqttClient();

    static MqttClient* s_instance;

    void applyConfig();
    void reconnect();
    void doConnect();
    void publishDiscovery();
    void publishState(bool gamemode);
    void setStatus(const QString &text);

    QString nodeId() const;        // sanitized id used for topics / unique_id
    QString stateTopic() const;
    QString discoveryTopic() const;

    QMqttClient *m_client;
    QString m_statusText;
    bool m_reconnectPending;
};

#endif // MQTTCLIENT_H
