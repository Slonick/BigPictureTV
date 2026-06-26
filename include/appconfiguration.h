#ifndef APPCONFIGURATION_H
#define APPCONFIGURATION_H

#include <QObject>
#include <QQmlEngine>
#include <QSettings>
#include <QVariantList>

class AppConfiguration : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Window detection settings
    Q_PROPERTY(int targetWindowMode READ targetWindowMode WRITE setTargetWindowMode NOTIFY targetWindowModeChanged)
    Q_PROPERTY(QString customWindowTitle READ customWindowTitle WRITE setCustomWindowTitle NOTIFY customWindowTitleChanged)
    Q_PROPERTY(bool skipIntro READ skipIntro WRITE setSkipIntro NOTIFY skipIntroChanged)
    Q_PROPERTY(bool launchAtStartup READ launchAtStartup WRITE setLaunchAtStartup NOTIFY launchAtStartupChanged)
    Q_PROPERTY(bool doNotSwitchIfSunshineActive READ doNotSwitchIfSunshineActive WRITE setDoNotSwitchIfSunshineActive NOTIFY doNotSwitchIfSunshineActiveChanged)

    // Audio settings
    Q_PROPERTY(bool disableAudioSwitch READ disableAudioSwitch WRITE setDisableAudioSwitch NOTIFY disableAudioSwitchChanged)
    Q_PROPERTY(bool useHdmiAudioForGamemode READ useHdmiAudioForGamemode WRITE setUseHdmiAudioForGamemode NOTIFY useHdmiAudioForGamemodeChanged)
    Q_PROPERTY(QString gamemodeAudioDevice READ gamemodeAudioDevice WRITE setGamemodeAudioDevice NOTIFY gamemodeAudioDeviceChanged)
    Q_PROPERTY(QString desktopAudioDevice READ desktopAudioDevice WRITE setDesktopAudioDevice NOTIFY desktopAudioDeviceChanged)
    Q_PROPERTY(QString gamemodeAudioDeviceId READ gamemodeAudioDeviceId WRITE setGamemodeAudioDeviceId NOTIFY gamemodeAudioDeviceIdChanged)
    Q_PROPERTY(QString desktopAudioDeviceId READ desktopAudioDeviceId WRITE setDesktopAudioDeviceId NOTIFY desktopAudioDeviceIdChanged)

    // Monitor settings
    Q_PROPERTY(bool disableMonitorSwitch READ disableMonitorSwitch WRITE setDisableMonitorSwitch NOTIFY disableMonitorSwitchChanged)
    Q_PROPERTY(QVariantList gamemodeDisplays READ gamemodeDisplays WRITE setGamemodeDisplays NOTIFY gamemodeDisplaysChanged)
    Q_PROPERTY(QString gamemodePrimaryDisplay READ gamemodePrimaryDisplay WRITE setGamemodePrimaryDisplay NOTIFY gamemodePrimaryDisplayChanged)

    // Action settings
    Q_PROPERTY(bool closeDiscordAction READ closeDiscordAction WRITE setCloseDiscordAction NOTIFY closeDiscordActionChanged)
    Q_PROPERTY(bool performancePowerplanAction READ performancePowerplanAction WRITE setPerformancePowerplanAction NOTIFY performancePowerplanActionChanged)
    Q_PROPERTY(bool pauseMediaAction READ pauseMediaAction WRITE setPauseMediaAction NOTIFY pauseMediaActionChanged)
    Q_PROPERTY(bool disableNightlightAction READ disableNightlightAction WRITE setDisableNightlightAction NOTIFY disableNightlightActionChanged)
    Q_PROPERTY(bool enableHdr READ enableHdr WRITE setEnableHdr NOTIFY enableHdrChanged)
    Q_PROPERTY(bool beacnAudienceMixRouting READ beacnAudienceMixRouting WRITE setBeacnAudienceMixRouting NOTIFY beacnAudienceMixRoutingChanged)

    // Home Assistant / MQTT settings
    Q_PROPERTY(bool mqttEnabled READ mqttEnabled WRITE setMqttEnabled NOTIFY mqttEnabledChanged)
    Q_PROPERTY(QString mqttHost READ mqttHost WRITE setMqttHost NOTIFY mqttHostChanged)
    Q_PROPERTY(int mqttPort READ mqttPort WRITE setMqttPort NOTIFY mqttPortChanged)
    Q_PROPERTY(QString mqttUsername READ mqttUsername WRITE setMqttUsername NOTIFY mqttUsernameChanged)
    Q_PROPERTY(QString mqttPassword READ mqttPassword WRITE setMqttPassword NOTIFY mqttPasswordChanged)
    Q_PROPERTY(QString mqttEntityName READ mqttEntityName WRITE setMqttEntityName NOTIFY mqttEntityNameChanged)
    Q_PROPERTY(QString mqttBaseTopic READ mqttBaseTopic WRITE setMqttBaseTopic NOTIFY mqttBaseTopicChanged)
    Q_PROPERTY(QString mqttDiscoveryPrefix READ mqttDiscoveryPrefix WRITE setMqttDiscoveryPrefix NOTIFY mqttDiscoveryPrefixChanged)

    // Internal state
    Q_PROPERTY(bool gamemode READ gamemode WRITE setGamemode NOTIFY gamemodeChanged)

    Q_PROPERTY(bool firstRun READ firstRun WRITE setFirstRun NOTIFY firstRunChanged)

public:
    static AppConfiguration* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static AppConfiguration* instance();

    // Getters
    int targetWindowMode() const { return m_targetWindowMode; }
    QString customWindowTitle() const { return m_customWindowTitle; }
    bool skipIntro() const { return m_skipIntro; }
    bool launchAtStartup() const { return m_launchAtStartup; }
    bool doNotSwitchIfSunshineActive() const { return m_doNotSwitchIfSunshineActive; }

    bool disableAudioSwitch() const { return m_disableAudioSwitch; }
    bool useHdmiAudioForGamemode() const { return m_useHdmiAudioForGamemode; }
    QString gamemodeAudioDevice() const { return m_gamemodeAudioDevice; }
    QString desktopAudioDevice() const { return m_desktopAudioDevice; }
    QString gamemodeAudioDeviceId() const { return m_gamemodeAudioDeviceId; }
    QString desktopAudioDeviceId() const { return m_desktopAudioDeviceId; }

    bool disableMonitorSwitch() const { return m_disableMonitorSwitch; }
    QVariantList gamemodeDisplays() const { return m_gamemodeDisplays; }
    QString gamemodePrimaryDisplay() const { return m_gamemodePrimaryDisplay; }

    bool closeDiscordAction() const { return m_closeDiscordAction; }
    bool performancePowerplanAction() const { return m_performancePowerplanAction; }
    bool pauseMediaAction() const { return m_pauseMediaAction; }
    bool disableNightlightAction() const { return m_disableNightlightAction; }
    bool enableHdr() const { return m_enableHdr; }
    bool beacnAudienceMixRouting() const { return m_beacnAudienceMixRouting; }
    QString beacnPreviousAudienceDevice() const { return m_beacnPreviousAudienceDevice; }
    void setBeacnPreviousAudienceDevice(const QString &value);

    bool mqttEnabled() const { return m_mqttEnabled; }
    QString mqttHost() const { return m_mqttHost; }
    int mqttPort() const { return m_mqttPort; }
    QString mqttUsername() const { return m_mqttUsername; }
    QString mqttPassword() const { return m_mqttPassword; }
    QString mqttEntityName() const { return m_mqttEntityName; }
    QString mqttBaseTopic() const { return m_mqttBaseTopic; }
    QString mqttDiscoveryPrefix() const { return m_mqttDiscoveryPrefix; }

    bool gamemode() const { return m_gamemode; }
    bool firstRun() const { return m_firstRun; }

    // Setters
    void setTargetWindowMode(int value);
    void setCustomWindowTitle(const QString &value);
    void setSkipIntro(bool value);
    void setLaunchAtStartup(bool value);
    void setDoNotSwitchIfSunshineActive(bool value);

    void setDisableAudioSwitch(bool value);
    void setUseHdmiAudioForGamemode(bool value);
    void setGamemodeAudioDevice(const QString &value);
    void setDesktopAudioDevice(const QString &value);
    void setGamemodeAudioDeviceId(const QString &value);
    void setDesktopAudioDeviceId(const QString &value);

    void setDisableMonitorSwitch(bool value);
    void setGamemodeDisplays(const QVariantList &value);
    void setGamemodePrimaryDisplay(const QString &value);

    void setCloseDiscordAction(bool value);
    void setPerformancePowerplanAction(bool value);
    void setPauseMediaAction(bool value);
    void setDisableNightlightAction(bool value);
    void setEnableHdr(bool value);
    void setBeacnAudienceMixRouting(bool value);

    void setMqttEnabled(bool value);
    void setMqttHost(const QString &value);
    void setMqttPort(int value);
    void setMqttUsername(const QString &value);
    void setMqttPassword(const QString &value);
    void setMqttEntityName(const QString &value);
    void setMqttBaseTopic(const QString &value);
    void setMqttDiscoveryPrefix(const QString &value);

    void setGamemode(bool value);
    void setFirstRun(bool value);

    Q_INVOKABLE void resetToDefaults();

signals:
    void targetWindowModeChanged();
    void customWindowTitleChanged();
    void skipIntroChanged();
    void launchAtStartupChanged();
    void doNotSwitchIfSunshineActiveChanged();

    void disableAudioSwitchChanged();
    void useHdmiAudioForGamemodeChanged();
    void gamemodeAudioDeviceChanged();
    void desktopAudioDeviceChanged();
    void gamemodeAudioDeviceIdChanged();
    void desktopAudioDeviceIdChanged();

    void disableMonitorSwitchChanged();
    void gamemodeDisplaysChanged();
    void gamemodePrimaryDisplayChanged();

    void closeDiscordActionChanged();
    void performancePowerplanActionChanged();
    void pauseMediaActionChanged();
    void disableNightlightActionChanged();
    void enableHdrChanged();
    void beacnAudienceMixRoutingChanged();

    void mqttEnabledChanged();
    void mqttHostChanged();
    void mqttPortChanged();
    void mqttUsernameChanged();
    void mqttPasswordChanged();
    void mqttEntityNameChanged();
    void mqttBaseTopicChanged();
    void mqttDiscoveryPrefixChanged();

    void gamemodeChanged();
    void firstRunChanged();

private:
    explicit AppConfiguration(QObject *parent = nullptr);
    ~AppConfiguration();

    static AppConfiguration* s_instance;

    void loadSettings();
    void saveSettings();

    QSettings m_settings;

    int m_targetWindowMode;
    QString m_customWindowTitle;
    bool m_skipIntro;
    bool m_launchAtStartup;
    bool m_doNotSwitchIfSunshineActive;

    bool m_disableAudioSwitch;
    bool m_useHdmiAudioForGamemode;
    QString m_gamemodeAudioDevice;
    QString m_desktopAudioDevice;
    QString m_gamemodeAudioDeviceId;
    QString m_desktopAudioDeviceId;

    bool m_disableMonitorSwitch;
    QVariantList m_gamemodeDisplays;
    QString m_gamemodePrimaryDisplay;

    bool m_closeDiscordAction;
    bool m_performancePowerplanAction;
    bool m_pauseMediaAction;
    bool m_disableNightlightAction;
    bool m_enableHdr;
    bool m_beacnAudienceMixRouting;
    QString m_beacnPreviousAudienceDevice;

    bool m_mqttEnabled;
    QString m_mqttHost;
    int m_mqttPort;
    QString m_mqttUsername;
    QString m_mqttPassword;
    QString m_mqttEntityName;
    QString m_mqttBaseTopic;
    QString m_mqttDiscoveryPrefix;

    bool m_gamemode;
    bool m_firstRun;
};

#endif // APPCONFIGURATION_H
