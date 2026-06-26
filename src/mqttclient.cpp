#include "mqttclient.h"
#include "appconfiguration.h"
#include "logmanager.h"

#include <QTcpSocket>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

namespace {

constexpr quint16 kKeepAliveSecs = 60;

// MQTT control packet types (high nibble of the fixed header byte).
constexpr quint8 kConnect  = 0x10;
constexpr quint8 kConnack  = 0x20;
constexpr quint8 kPublish  = 0x30;
constexpr quint8 kPingReq  = 0xC0;
constexpr quint8 kDisconnect = 0xE0;

// Encode an MQTT "Remaining Length" variable byte integer.
QByteArray encodeRemainingLength(int length)
{
    QByteArray out;
    do {
        quint8 byte = length % 128;
        length /= 128;
        if (length > 0) {
            byte |= 0x80;
        }
        out.append(static_cast<char>(byte));
    } while (length > 0);
    return out;
}

// Encode an MQTT UTF-8 string: 2-byte big-endian length prefix + bytes.
QByteArray encodeString(const QString &str)
{
    const QByteArray utf8 = str.toUtf8();
    QByteArray out;
    out.append(static_cast<char>((utf8.size() >> 8) & 0xFF));
    out.append(static_cast<char>(utf8.size() & 0xFF));
    out.append(utf8);
    return out;
}

QString connackError(quint8 returnCode)
{
    switch (returnCode) {
    case 1:  return QStringLiteral("Unacceptable protocol version");
    case 2:  return QStringLiteral("Client ID rejected");
    case 3:  return QStringLiteral("Broker unavailable");
    case 4:  return QStringLiteral("Bad username or password");
    case 5:  return QStringLiteral("Not authorized");
    default: return QStringLiteral("Connection refused (code %1)").arg(returnCode);
    }
}

} // namespace

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
    , m_socket(new QTcpSocket(this))
    , m_pingTimer(new QTimer(this))
    , m_retryTimer(new QTimer(this))
    , m_statusText(QStringLiteral("Disabled"))
    , m_mqttConnected(false)
    , m_intentionalDisconnect(false)
{
    connect(m_socket, &QTcpSocket::connected, this, &MqttClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &MqttClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &MqttClient::onSocketError);
    connect(m_socket, &QTcpSocket::readyRead, this, &MqttClient::onReadyRead);

    // Keep the connection alive well within the broker's keep-alive window.
    m_pingTimer->setInterval(kKeepAliveSecs * 1000 / 2);
    connect(m_pingTimer, &QTimer::timeout, this, &MqttClient::sendPing);

    // Reconnect after an unexpected drop (e.g. broker restart) while enabled.
    m_retryTimer->setSingleShot(true);
    m_retryTimer->setInterval(10000);
    connect(m_retryTimer, &QTimer::timeout, this, [this]() {
        if (AppConfiguration::instance()->mqttEnabled()
            && m_socket->state() == QAbstractSocket::UnconnectedState) {
            doConnect();
        }
    });

    AppConfiguration *config = AppConfiguration::instance();
    connect(config, &AppConfiguration::gamemodeChanged, this, &MqttClient::onGamemodeChanged);
    connect(config, &AppConfiguration::mqttEnabledChanged, this, &MqttClient::onMqttEnabledChanged);

    if (config->mqttEnabled() && !config->mqttHost().isEmpty()) {
        reconnect();
    }
}

MqttClient::~MqttClient()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
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

void MqttClient::reconnect()
{
    m_retryTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        // Bounce the connection so the latest settings take effect. abort() is
        // synchronous, so any disconnected()/errorOccurred() it triggers is
        // delivered and handled within the call while the flag is set; we then
        // clear it so it can't leak into the fresh connection below.
        m_intentionalDisconnect = true;
        m_socket->abort();
        m_intentionalDisconnect = false;
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

    m_mqttConnected = false;
    m_readBuffer.clear();
    setStatus(QStringLiteral("Connecting to %1:%2…")
                  .arg(config->mqttHost())
                  .arg(config->mqttPort()));
    LogManager::info(QStringLiteral("MQTT: connecting to %1:%2")
                         .arg(config->mqttHost())
                         .arg(config->mqttPort()));
    m_socket->connectToHost(config->mqttHost(), static_cast<quint16>(config->mqttPort()));
}

void MqttClient::onSocketConnected()
{
    // TCP is up; perform the MQTT handshake.
    sendConnect();
}

void MqttClient::sendConnect()
{
    AppConfiguration *config = AppConfiguration::instance();
    const QString user = config->mqttUsername();
    const QString pass = config->mqttPassword();
    const bool hasUser = !user.isEmpty();
    const bool hasPass = !pass.isEmpty();

    quint8 flags = 0x02; // clean session
    if (hasUser) flags |= 0x80;
    if (hasPass) flags |= 0x40;

    QByteArray variableHeader;
    variableHeader.append(encodeString(QStringLiteral("MQTT")));
    variableHeader.append(static_cast<char>(0x04)); // protocol level 3.1.1
    variableHeader.append(static_cast<char>(flags));
    variableHeader.append(static_cast<char>((kKeepAliveSecs >> 8) & 0xFF));
    variableHeader.append(static_cast<char>(kKeepAliveSecs & 0xFF));

    QByteArray payload;
    payload.append(encodeString(QStringLiteral("BigPictureTV-") + nodeId()));
    if (hasUser) payload.append(encodeString(user));
    if (hasPass) payload.append(encodeString(pass));

    QByteArray packet;
    packet.append(static_cast<char>(kConnect));
    packet.append(encodeRemainingLength(variableHeader.size() + payload.size()));
    packet.append(variableHeader);
    packet.append(payload);

    m_socket->write(packet);
}

void MqttClient::onReadyRead()
{
    m_readBuffer.append(m_socket->readAll());
    parseBuffer();
}

void MqttClient::parseBuffer()
{
    while (m_readBuffer.size() >= 2) {
        const quint8 header = static_cast<quint8>(m_readBuffer.at(0));

        // Decode the variable-length "Remaining Length" field.
        int remainingLength = 0;
        int multiplier = 1;
        int idx = 1;
        bool lengthComplete = false;
        while (idx < m_readBuffer.size()) {
            const quint8 b = static_cast<quint8>(m_readBuffer.at(idx));
            remainingLength += (b & 0x7F) * multiplier;
            multiplier *= 128;
            idx++;
            if ((b & 0x80) == 0) { lengthComplete = true; break; }
            if (idx > 5) { // malformed: length field too long
                m_readBuffer.clear();
                return;
            }
        }
        if (!lengthComplete) {
            return; // need more bytes for the length field
        }

        const int totalLength = idx + remainingLength;
        if (m_readBuffer.size() < totalLength) {
            return; // wait for the full packet
        }

        const QByteArray packet = m_readBuffer.left(totalLength);
        m_readBuffer.remove(0, totalLength);

        const quint8 type = header & 0xF0;
        if (type == kConnack && remainingLength >= 2) {
            const quint8 returnCode = static_cast<quint8>(packet.at(idx + 1));
            if (returnCode == 0) {
                onMqttConnected();
            } else {
                setStatus(QStringLiteral("Error: ") + connackError(returnCode));
                LogManager::warning(QStringLiteral("MQTT: CONNACK refused - ") + connackError(returnCode));
                m_socket->disconnectFromHost();
            }
        }
        // PINGRESP and anything else: nothing to do for a publish-only client.
    }
}

void MqttClient::onMqttConnected()
{
    m_mqttConnected = true;
    m_pingTimer->start();
    setStatus(QStringLiteral("Connected"));
    LogManager::info(QStringLiteral("MQTT: connected"));

    // Re-announce the entity (retained) and push the current state so HA is in
    // sync immediately after a (re)connect.
    publishDiscovery();
    publishState(AppConfiguration::instance()->gamemode());
}

void MqttClient::onSocketDisconnected()
{
    const bool wasConnected = m_mqttConnected;
    m_mqttConnected = false;
    m_pingTimer->stop();

    if (m_intentionalDisconnect) {
        // Deliberate teardown (settings bounce or disable): don't auto-reconnect
        // and let the caller set the status.
        m_intentionalDisconnect = false;
        return;
    }

    if (wasConnected) {
        LogManager::info(QStringLiteral("MQTT: disconnected"));
    }

    // Unexpected drop while still enabled — schedule a retry.
    if (AppConfiguration::instance()->mqttEnabled()) {
        setStatus(QStringLiteral("Disconnected — retrying…"));
        m_retryTimer->start();
    } else {
        setStatus(QStringLiteral("Disconnected"));
    }
}

void MqttClient::onSocketError()
{
    LogManager::warning(QStringLiteral("MQTT: socket error - ") + m_socket->errorString());

    if (m_intentionalDisconnect) {
        return;
    }

    // A failed connect doesn't emit disconnected() (the socket was never
    // connected), so schedule the retry from here when still enabled.
    if (AppConfiguration::instance()->mqttEnabled()
        && m_socket->state() == QAbstractSocket::UnconnectedState
        && !m_retryTimer->isActive()) {
        setStatus(QStringLiteral("Error: %1 — retrying…").arg(m_socket->errorString()));
        m_retryTimer->start();
    } else {
        setStatus(QStringLiteral("Error: ") + m_socket->errorString());
    }
}

void MqttClient::sendPing()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        const char ping[2] = { static_cast<char>(kPingReq), 0x00 };
        m_socket->write(ping, 2);
    }
}

void MqttClient::publish(const QString &topic, const QByteArray &payload, bool retain)
{
    if (!m_mqttConnected) {
        return;
    }

    QByteArray variableHeader = encodeString(topic); // QoS 0: no packet identifier

    QByteArray packet;
    packet.append(static_cast<char>(kPublish | (retain ? 0x01 : 0x00)));
    packet.append(encodeRemainingLength(variableHeader.size() + payload.size()));
    packet.append(variableHeader);
    packet.append(payload);

    m_socket->write(packet);
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

    publish(discoveryTopic(), QJsonDocument(payload).toJson(QJsonDocument::Compact), /*retain=*/true);
    LogManager::debug(QStringLiteral("MQTT: published discovery to ") + discoveryTopic());
}

void MqttClient::publishState(bool gamemode)
{
    const QByteArray payload = gamemode ? QByteArrayLiteral("ON") : QByteArrayLiteral("OFF");
    publish(stateTopic(), payload, /*retain=*/true);
    LogManager::info(QStringLiteral("MQTT: published state %1 to %2")
                         .arg(QString::fromUtf8(payload), stateTopic()));
}

void MqttClient::onGamemodeChanged()
{
    if (!AppConfiguration::instance()->mqttEnabled()) {
        return;
    }
    if (m_mqttConnected) {
        publishState(AppConfiguration::instance()->gamemode());
    }
}

void MqttClient::onMqttEnabledChanged()
{
    if (AppConfiguration::instance()->mqttEnabled()) {
        reconnect();
    } else {
        m_retryTimer->stop();
        if (m_socket->state() == QAbstractSocket::ConnectedState) {
            // Polite MQTT disconnect, then drop the socket. The flag stays set
            // until the async disconnected() handler consumes it.
            m_intentionalDisconnect = true;
            const char bye[2] = { static_cast<char>(kDisconnect), 0x00 };
            m_socket->write(bye, 2);
            m_socket->disconnectFromHost(); // flushes the DISCONNECT, then closes
        } else if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            // Still connecting: abort() synchronously; clear the flag right after.
            m_intentionalDisconnect = true;
            m_socket->abort();
            m_intentionalDisconnect = false;
        }
        m_mqttConnected = false;
        m_pingTimer->stop();
        setStatus(QStringLiteral("Disabled"));
    }
}

void MqttClient::testConnection()
{
    // Reconnect with whatever is currently saved. onMqttConnected() / onSocketError()
    // will update statusText for the UI to display.
    reconnect();
}

void MqttClient::simulateState(bool gamemode)
{
    if (!m_mqttConnected) {
        setStatus(QStringLiteral("Not connected — hit Test first"));
        return;
    }
    publishState(gamemode);
    setStatus(QStringLiteral("Simulated state: %1").arg(gamemode ? QStringLiteral("ON") : QStringLiteral("OFF")));
}
