#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QObject>
#include <QQmlEngine>
#include <QByteArray>

class QTcpSocket;
class QTimer;

// Publishes the current Big Picture / desktop state to an MQTT broker so Home
// Assistant can react (e.g. turn the TV on the right input when gamemode
// starts). Uses MQTT Discovery so BigPictureTV shows up automatically as a
// binary_sensor without any manual YAML on the HA side.
//
// This is a small, self-contained MQTT 3.1.1 publisher built directly on
// QTcpSocket (CONNECT / PUBLISH / PINGREQ / DISCONNECT, QoS 0). We don't use the
// Qt Mqtt module because it ships only with the commercial Qt installer and
// isn't available through aqtinstall, which our CI uses.
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
    bool connected() const { return m_mqttConnected; }

    // Connect now with the current settings and publish discovery + state.
    Q_INVOKABLE void testConnection();

    // Publish an ON/OFF state without actually entering Big Picture, so the
    // user can confirm their HA automation fires.
    Q_INVOKABLE void simulateState(bool gamemode);

signals:
    void statusChanged();

private slots:
    void onGamemodeChanged();
    void onMqttEnabledChanged();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError();
    void onReadyRead();
    void sendPing();

private:
    explicit MqttClient(QObject *parent = nullptr);
    ~MqttClient();

    static MqttClient* s_instance;

    void reconnect();
    void doConnect();
    void sendConnect();
    void onMqttConnected();      // CONNACK accepted
    void publishDiscovery();
    void publishState(bool gamemode);
    void publish(const QString &topic, const QByteArray &payload, bool retain);
    void parseBuffer();
    void setStatus(const QString &text);

    QString nodeId() const;        // sanitized id used for topics / unique_id
    QString stateTopic() const;
    QString discoveryTopic() const;

    QTcpSocket *m_socket;
    QTimer *m_pingTimer;
    QTimer *m_retryTimer;          // auto-reconnect after an unexpected drop
    QByteArray m_readBuffer;
    QString m_statusText;
    bool m_mqttConnected;          // CONNACK received, ready to publish
    bool m_intentionalDisconnect;  // suppress auto-reconnect for deliberate teardowns
};

#endif // MQTTCLIENT_H
