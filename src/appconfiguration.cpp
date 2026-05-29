#include "appconfiguration.h"
#include "shortcutmanager.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

AppConfiguration* AppConfiguration::s_instance = nullptr;

AppConfiguration* AppConfiguration::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    if (!s_instance) {
        s_instance = new AppConfiguration();
    }
    return s_instance;
}

AppConfiguration* AppConfiguration::instance()
{
    return s_instance;
}

AppConfiguration::AppConfiguration(QObject *parent)
    : QObject(parent)
    , m_settings("Odizinne", "BigPictureTV")
{
    loadSettings();
}

AppConfiguration::~AppConfiguration()
{
}

void AppConfiguration::loadSettings()
{
    m_targetWindowMode = m_settings.value("target_window_mode", 0).toInt();
    m_customWindowTitle = m_settings.value("custom_window_title", "").toString();
    m_skipIntro = m_settings.value("skip_intro", false).toBool();
    m_launchAtStartup = ShortcutManager::isShortcutPresent("BigPictureTV");
    m_doNotSwitchIfSunshineActive = m_settings.value("do_not_switch_if_sunshine_active", false).toBool();

    m_disableAudioSwitch = m_settings.value("disable_audio_switch", false).toBool();
    m_useHdmiAudioForGamemode = m_settings.value("use_hdmi_audio_for_gamemode", false).toBool();
    m_gamemodeAudioDevice = m_settings.value("gamemode_audio_device", "").toString();
    m_desktopAudioDevice = m_settings.value("desktop_audio_device", "").toString();
    m_gamemodeAudioDeviceId = m_settings.value("gamemode_audio_device_id", "").toString();
    m_desktopAudioDeviceId = m_settings.value("desktop_audio_device_id", "").toString();

    m_disableMonitorSwitch = m_settings.value("disable_monitor_switch", false).toBool();

    m_gamemodeDisplays.clear();
    bool migrated = false;
    QString displaysJson = m_settings.value("gamemode_displays", "").toString();
    if (!displaysJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(displaysJson.toUtf8());
        if (doc.isArray()) {
            for (const QJsonValue &val : doc.array()) {
                QJsonObject obj = val.toObject();
                QVariantMap entry;
                entry["devicePath"] = obj.value("devicePath").toString();
                entry["width"] = obj.value("width").toInt();
                entry["height"] = obj.value("height").toInt();
                entry["refreshRate"] = obj.value("refreshRate").toInt();
                m_gamemodeDisplays.append(entry);
            }
        }
    } else if (m_settings.contains("gamemode_display_device")) {
        // Migrate legacy single-display settings
        QString legacyDevice = m_settings.value("gamemode_display_device", "").toString();
        if (!legacyDevice.isEmpty()) {
            QVariantMap entry;
            entry["devicePath"] = legacyDevice;
            entry["width"] = m_settings.value("gamemode_display_width", 3840).toInt();
            entry["height"] = m_settings.value("gamemode_display_height", 2160).toInt();
            entry["refreshRate"] = m_settings.value("gamemode_display_refresh_rate", 60).toInt();
            m_gamemodeDisplays.append(entry);
        }
        m_settings.remove("gamemode_display_device");
        m_settings.remove("gamemode_display_width");
        m_settings.remove("gamemode_display_height");
        m_settings.remove("gamemode_display_refresh_rate");
        migrated = true;
    }

    m_gamemodePrimaryDisplay = m_settings.value("gamemode_primary_display", "").toString();
    if (m_gamemodePrimaryDisplay.isEmpty() && !m_gamemodeDisplays.isEmpty()) {
        m_gamemodePrimaryDisplay = m_gamemodeDisplays.first().toMap().value("devicePath").toString();
    }

    m_closeDiscordAction = m_settings.value("close_discord_action", false).toBool();
    m_performancePowerplanAction = m_settings.value("performance_powerplan_action", false).toBool();
    m_pauseMediaAction = m_settings.value("pause_media_action", false).toBool();
    m_disableNightlightAction = m_settings.value("disable_nightlight_action", false).toBool();
    m_enableHdr = m_settings.value("enable_hdr", false).toBool();
    m_beacnAudienceMixRouting = m_settings.value("beacn_audience_mix_routing", false).toBool();
    m_beacnPreviousAudienceDevice = m_settings.value("beacn_previous_audience_device", "").toString();

    m_gamemode = m_settings.value("gamemode", false).toBool();
    m_firstRun = m_settings.value("first_run", true).toBool();

    if (migrated) {
        saveSettings();
    }
}

void AppConfiguration::saveSettings()
{
    m_settings.setValue("target_window_mode", m_targetWindowMode);
    m_settings.setValue("custom_window_title", m_customWindowTitle);
    m_settings.setValue("skip_intro", m_skipIntro);
    m_settings.setValue("do_not_switch_if_sunshine_active", m_doNotSwitchIfSunshineActive);

    m_settings.setValue("disable_audio_switch", m_disableAudioSwitch);
    m_settings.setValue("use_hdmi_audio_for_gamemode", m_useHdmiAudioForGamemode);
    m_settings.setValue("gamemode_audio_device", m_gamemodeAudioDevice);
    m_settings.setValue("desktop_audio_device", m_desktopAudioDevice);
    m_settings.setValue("gamemode_audio_device_id", m_gamemodeAudioDeviceId);
    m_settings.setValue("desktop_audio_device_id", m_desktopAudioDeviceId);

    m_settings.setValue("disable_monitor_switch", m_disableMonitorSwitch);

    QJsonArray displaysArray;
    for (const QVariant &entry : m_gamemodeDisplays) {
        QVariantMap map = entry.toMap();
        QJsonObject obj;
        obj["devicePath"] = map.value("devicePath").toString();
        obj["width"] = map.value("width").toInt();
        obj["height"] = map.value("height").toInt();
        obj["refreshRate"] = map.value("refreshRate").toInt();
        displaysArray.append(obj);
    }
    m_settings.setValue("gamemode_displays",
                        QString::fromUtf8(QJsonDocument(displaysArray).toJson(QJsonDocument::Compact)));
    m_settings.setValue("gamemode_primary_display", m_gamemodePrimaryDisplay);

    m_settings.setValue("close_discord_action", m_closeDiscordAction);
    m_settings.setValue("performance_powerplan_action", m_performancePowerplanAction);
    m_settings.setValue("pause_media_action", m_pauseMediaAction);
    m_settings.setValue("disable_nightlight_action", m_disableNightlightAction);
    m_settings.setValue("enable_hdr", m_enableHdr);
    m_settings.setValue("beacn_audience_mix_routing", m_beacnAudienceMixRouting);
    m_settings.setValue("beacn_previous_audience_device", m_beacnPreviousAudienceDevice);

    m_settings.setValue("gamemode", m_gamemode);
    m_settings.setValue("first_run", m_firstRun);
}

void AppConfiguration::setTargetWindowMode(int value)
{
    if (m_targetWindowMode != value) {
        m_targetWindowMode = value;
        saveSettings();
        emit targetWindowModeChanged();
    }
}

void AppConfiguration::setCustomWindowTitle(const QString &value)
{
    if (m_customWindowTitle != value) {
        m_customWindowTitle = value;
        saveSettings();
        emit customWindowTitleChanged();
    }
}

void AppConfiguration::setSkipIntro(bool value)
{
    if (m_skipIntro != value) {
        m_skipIntro = value;
        saveSettings();
        emit skipIntroChanged();
    }
}

void AppConfiguration::setLaunchAtStartup(bool value)
{
    if (m_launchAtStartup != value) {
        m_launchAtStartup = value;
        ShortcutManager::manageShortcut(value, "BigPictureTV");
        emit launchAtStartupChanged();
    }
}

void AppConfiguration::setDoNotSwitchIfSunshineActive(bool value)
{
    if (m_doNotSwitchIfSunshineActive != value) {
        m_doNotSwitchIfSunshineActive = value;
        saveSettings();
        emit doNotSwitchIfSunshineActiveChanged();
    }
}

void AppConfiguration::setDisableAudioSwitch(bool value)
{
    if (m_disableAudioSwitch != value) {
        m_disableAudioSwitch = value;
        saveSettings();
        emit disableAudioSwitchChanged();
    }
}

void AppConfiguration::setUseHdmiAudioForGamemode(bool value)
{
    if (m_useHdmiAudioForGamemode != value) {
        m_useHdmiAudioForGamemode = value;
        saveSettings();
        emit useHdmiAudioForGamemodeChanged();
    }
}

void AppConfiguration::setGamemodeAudioDevice(const QString &value)
{
    if (m_gamemodeAudioDevice != value) {
        m_gamemodeAudioDevice = value;
        saveSettings();
        emit gamemodeAudioDeviceChanged();
    }
}

void AppConfiguration::setDesktopAudioDevice(const QString &value)
{
    if (m_desktopAudioDevice != value) {
        m_desktopAudioDevice = value;
        saveSettings();
        emit desktopAudioDeviceChanged();
    }
}

void AppConfiguration::setGamemodeAudioDeviceId(const QString &value)
{
    if (m_gamemodeAudioDeviceId != value) {
        m_gamemodeAudioDeviceId = value;
        saveSettings();
        emit gamemodeAudioDeviceIdChanged();
    }
}

void AppConfiguration::setDesktopAudioDeviceId(const QString &value)
{
    if (m_desktopAudioDeviceId != value) {
        m_desktopAudioDeviceId = value;
        saveSettings();
        emit desktopAudioDeviceIdChanged();
    }
}

void AppConfiguration::setDisableMonitorSwitch(bool value)
{
    if (m_disableMonitorSwitch != value) {
        m_disableMonitorSwitch = value;
        saveSettings();
        emit disableMonitorSwitchChanged();
    }
}

void AppConfiguration::setGamemodeDisplays(const QVariantList &value)
{
    if (m_gamemodeDisplays != value) {
        m_gamemodeDisplays = value;

        bool primaryStillPresent = false;
        for (const QVariant &entry : m_gamemodeDisplays) {
            if (entry.toMap().value("devicePath").toString() == m_gamemodePrimaryDisplay) {
                primaryStillPresent = true;
                break;
            }
        }
        if (!primaryStillPresent) {
            QString newPrimary = m_gamemodeDisplays.isEmpty()
                ? QString()
                : m_gamemodeDisplays.first().toMap().value("devicePath").toString();
            if (newPrimary != m_gamemodePrimaryDisplay) {
                m_gamemodePrimaryDisplay = newPrimary;
                saveSettings();
                emit gamemodeDisplaysChanged();
                emit gamemodePrimaryDisplayChanged();
                return;
            }
        }

        saveSettings();
        emit gamemodeDisplaysChanged();
    }
}

void AppConfiguration::setGamemodePrimaryDisplay(const QString &value)
{
    if (m_gamemodePrimaryDisplay != value) {
        m_gamemodePrimaryDisplay = value;
        saveSettings();
        emit gamemodePrimaryDisplayChanged();
    }
}

void AppConfiguration::setCloseDiscordAction(bool value)
{
    if (m_closeDiscordAction != value) {
        m_closeDiscordAction = value;
        saveSettings();
        emit closeDiscordActionChanged();
    }
}

void AppConfiguration::setPerformancePowerplanAction(bool value)
{
    if (m_performancePowerplanAction != value) {
        m_performancePowerplanAction = value;
        saveSettings();
        emit performancePowerplanActionChanged();
    }
}

void AppConfiguration::setPauseMediaAction(bool value)
{
    if (m_pauseMediaAction != value) {
        m_pauseMediaAction = value;
        saveSettings();
        emit pauseMediaActionChanged();
    }
}

void AppConfiguration::setDisableNightlightAction(bool value)
{
    if (m_disableNightlightAction != value) {
        m_disableNightlightAction = value;
        saveSettings();
        emit disableNightlightActionChanged();
    }
}

void AppConfiguration::setEnableHdr(bool value)
{
    if (m_enableHdr != value) {
        m_enableHdr = value;
        saveSettings();
        emit enableHdrChanged();
    }
}

void AppConfiguration::setBeacnAudienceMixRouting(bool value)
{
    if (m_beacnAudienceMixRouting != value) {
        m_beacnAudienceMixRouting = value;
        saveSettings();
        emit beacnAudienceMixRoutingChanged();
    }
}

void AppConfiguration::setBeacnPreviousAudienceDevice(const QString &value)
{
    if (m_beacnPreviousAudienceDevice != value) {
        m_beacnPreviousAudienceDevice = value;
        saveSettings();
    }
}

void AppConfiguration::setGamemode(bool value)
{
    if (m_gamemode != value) {
        m_gamemode = value;
        saveSettings();
        emit gamemodeChanged();
    }
}

void AppConfiguration::setFirstRun(bool value)
{
    if (m_firstRun != value) {
        m_firstRun = value;
        saveSettings();
        emit firstRunChanged();
    }
}

void AppConfiguration::resetToDefaults()
{
    setTargetWindowMode(0);
    setCustomWindowTitle("");
    setSkipIntro(false);
    setLaunchAtStartup(false);
    setDoNotSwitchIfSunshineActive(false);

    setDisableAudioSwitch(false);
    setUseHdmiAudioForGamemode(false);
    setGamemodeAudioDevice("");
    setDesktopAudioDevice("");
    setGamemodeAudioDeviceId("");
    setDesktopAudioDeviceId("");

    setDisableMonitorSwitch(false);
    setGamemodeDisplays(QVariantList());
    setGamemodePrimaryDisplay("");

    setCloseDiscordAction(false);
    setPerformancePowerplanAction(false);
    setPauseMediaAction(false);
    setDisableNightlightAction(false);
    setEnableHdr(false);
    setBeacnAudienceMixRouting(false);
}
