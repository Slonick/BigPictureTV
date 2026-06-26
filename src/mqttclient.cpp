#include "mqttclient.h"
#include "appconfiguration.h"
#include "logmanager.h"

#include <QtMqtt/QMqttClient>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

MqttClient* MqttClient::s_instance = nullptr;

MqttClient* MqttClient::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    if (!s_instance) {
        s_instance = new MqttClient();
    }
    return s_instance;
}

MqttClient* MqttClient::instance()
{
    return s_instance;
}

MqttClient::MqttClient(QObject *parent)
    : QObject(parent)
    , m_client(new QMqttClient(this))
    , m_statusText(QStringLiteral("Disabled"))
    , m_reconnectPending(false)
{
    connect(m_client, &QMqttClient::connected, this, &MqttClient::onConnected);
    connect(m_client, &QMqttClient::disconnected, this, &MqttClient::onDisconnected);
    connect(m_client, &QMqttClient::errorChanged, this, &MqttClient::onErrorChanged);

    AppConfiguration *config = AppConfiguration::instance();
    connect(config, &AppConfiguration::gamemodeChanged, this, &MqttClient::onGamemodeChanged);
    connect(config, &AppConfiguration::mqttEnabledChanged, this, &MqttClient::onMqttEnabledChanged);

    if (config->mqttEnabled() && !config->mqttHost().isEmpty()) {
        reconnect();
    }
}

MqttClient::~MqttClient()
{
    if (m_client->state() != QMqttClient::Disconnected) {
        m_client->disconnectFromHost();
    }
}

bool MqttClient::connected() const
{
    return m_client->state() == QMqttClient::Connected;
}

QString MqttClient::nodeId() const
{
    QString base = AppConfiguration::instance()->mqttBaseTopic();
    if (base.isEmpty()) {
        base = QStringLiteral("bigpicturetv");
    }
    // HA discovery object_id / unique_id only accept [a-zA-Z0-9_-].
    base.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9_-]")), QStringLiteral("_"));
    return base;
}

QString MqttClient::stateTopic() const
{
    QString base = AppConfiguration::instance()->mqttBaseTopic();
    if (base.isEmpty()) {
        base = QStringLiteral("bigpicturetv");
    }
    return base + QStringLiteral("/state");
}

QString MqttClient::discoveryTopic() const
{
    QString prefix = AppConfiguration::instance()->mqttDiscoveryPrefix();
    if (prefix.isEmpty()) {
        prefix = QStringLiteral("homeassistant");
    }
    return QStringLiteral("%1/binary_sensor/%2/config").arg(prefix, nodeId());
}

void MqttClient::setStatus(const QString &text)
{
    // Always emit: the derived `connected` property may have flipped even when
    // the status text itself is unchanged.
    m_statusText = text;
    emit statusChanged();
}

void MqttClient::applyConfig()
{
    AppConfiguration *config = AppConfiguration::instance();
    m_client->setHostname(config->mqttHost());
    m_client->setPort(static_cast<quint16>(config->mqttPort()));
    m_client->setClientId(QStringLiteral("BigPictureTV-") + nodeId());

    const QString user = config->mqttUsername();
    const QString pass = config->mqttPassword();
    if (!user.isEmpty()) {
        m_client->setUsername(user);
    }
    if (!pass.isEmpty()) {
        m_client->setPassword(pass);
    }
}

void MqttClient::reconnect()
{
    if (m_client->state() != QMqttClient::Disconnected) {
        // Bounce the connection so the latest settings take effect, reconnecting
        // once the disconnect actually lands.
        m_reconnectPending = true;
        m_client->disconnectFromHost();
        return;
    }
    doConnect();
}

void MqttClient::doConnect()
{
    AppConfiguration *config = AppConfiguration::instance();
    if (config->mqttHost().isEmpty()) {
        setStatus(QStringLiteral("No broker configured"));
        return;
    }

    applyConfig();
    setStatus(QStringLiteral("Connecting to %1:%2…")
                  .arg(config->mqttHost())
                  .arg(config->mqttPort()));
    LogManager::info(QStringLiteral("MQTT: connecting to %1:%2")
                         .arg(config->mqttHost())
                         .arg(config->mqttPort()));
    m_client->connectToHost();
}

void MqttClient::onConnected()
{
    setStatus(QStringLiteral("Connected"));
    LogManager::info(QStringLiteral("MQTT: connected"));

    // Re-announce the entity (retained) and push the current state so HA is in
    // sync immediately after a (re)connect.
    publishDiscovery();
    publishState(AppConfiguration::instance()->gamemode());
}

void MqttClient::onDisconnected()
{
    LogManager::info(QStringLiteral("MQTT: disconnected"));
    if (m_reconnectPending) {
        m_reconnectPending = false;
        doConnect();
        return;
    }
    setStatus(QStringLiteral("Disconnected"));
}

void MqttClient::onErrorChanged()
{
    const QMqttClient::ClientError error = m_client->error();
    if (error == QMqttClient::NoError) {
        return;
    }

    QString message;
    switch (error) {
    case QMqttClient::InvalidProtocolVersion: message = QStringLiteral("Invalid protocol version"); break;
    case QMqttClient::IdRejected:             message = QStringLiteral("Client ID rejected"); break;
    case QMqttClient::ServerUnavailable:      message = QStringLiteral("Server unavailable"); break;
    case QMqttClient::BadUsernameOrPassword:  message = QStringLiteral("Bad username or password"); break;
    case QMqttClient::NotAuthorized:          message = QStringLiteral("Not authorized"); break;
    case QMqttClient::TransportInvalid:       message = QStringLiteral("Transport error (host/port unreachable)"); break;
    case QMqttClient::ProtocolViolation:      message = QStringLiteral("Protocol violation"); break;
    case QMqttClient::UnknownError:           message = QStringLiteral("Unknown error"); break;
    default:                                  message = QStringLiteral("Error %1").arg(static_cast<int>(error)); break;
    }
    setStatus(QStringLiteral("Error: ") + message);
    LogManager::warning(QStringLiteral("MQTT: error - ") + message);
}

void MqttClient::publishDiscovery()
{
    AppConfiguration *config = AppConfiguration::instance();

    QJsonObject device;
    device["identifiers"] = QJsonArray{ nodeId() };
    device["name"] = QStringLiteral("BigPictureTV");
    device["manufacturer"] = QStringLiteral("Odizinne");
    device["model"] = QStringLiteral("BigPictureTV");

    QJsonObject payload;
    payload["name"] = config->mqttEntityName().isEmpty()
                          ? QStringLiteral("Big Picture Mode")
                          : config->mqttEntityName();
    payload["unique_id"] = nodeId() + QStringLiteral("_state");
    payload["object_id"] = nodeId();
    payload["state_topic"] = stateTopic();
    payload["payload_on"] = QStringLiteral("ON");
    payload["payload_off"] = QStringLiteral("OFF");
    payload["device_class"] = QStringLiteral("running");
    payload["device"] = device;

    const QByteArray data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    m_client->publish(discoveryTopic(), data, /*qos=*/0, /*retain=*/true);
    LogManager::debug(QStringLiteral("MQTT: published discovery to ") + discoveryTopic());
}

void MqttClient::publishState(bool gamemode)
{
    const QByteArray payload = gamemode ? QByteArrayLiteral("ON") : QByteArrayLiteral("OFF");
    m_client->publish(stateTopic(), payload, /*qos=*/0, /*retain=*/true);
    LogManager::info(QStringLiteral("MQTT: published state %1 to %2")
                         .arg(QString::fromUtf8(payload), stateTopic()));
}

void MqttClient::onGamemodeChanged()
{
    if (!AppConfiguration::instance()->mqttEnabled()) {
        return;
    }
    if (connected()) {
        publishState(AppConfiguration::instance()->gamemode());
    }
}

void MqttClient::onMqttEnabledChanged()
{
    if (AppConfiguration::instance()->mqttEnabled()) {
        reconnect();
    } else {
        m_reconnectPending = false;
        if (m_client->state() != QMqttClient::Disconnected) {
            m_client->disconnectFromHost();
        }
        setStatus(QStringLiteral("Disabled"));
    }
}

void MqttClient::testConnection()
{
    // Reconnect with whatever is currently saved. onConnected() / onErrorChanged()
    // will update statusText for the UI to display.
    reconnect();
}

void MqttClient::simulateState(bool gamemode)
{
    if (!connected()) {
        setStatus(QStringLiteral("Not connected — hit Test first"));
        return;
    }
    publishState(gamemode);
    setStatus(QStringLiteral("Simulated state: %1").arg(gamemode ? QStringLiteral("ON") : QStringLiteral("OFF")));
}
