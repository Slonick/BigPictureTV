#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <windows.h>
#include <vector>

struct DisplayInfo {
    std::wstring friendly_name;
    std::wstring device_path;
    LUID adapter_id;
    UINT32 target_id;
    UINT32 source_id;
    bool is_active;
};

struct SavedConfig {
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
};

struct DisplayMode {
    UINT32 width;
    UINT32 height;
    UINT32 refreshRate;
};

struct DisplayTarget {
    std::wstring device_path;
    DisplayMode mode;
    bool has_mode;
    bool is_primary;
};

class DisplayManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList displays READ displays NOTIFY displaysChanged)

public:
    static DisplayManager* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static DisplayManager* instance();

    explicit DisplayManager(QObject *parent = nullptr);
    ~DisplayManager();

    QVariantList displays() const { return m_displays; }

    Q_INVOKABLE bool switchToDisplay(const QString &devicePath);
    Q_INVOKABLE bool switchToDisplayWithResolution(const QString &devicePath, quint32 width, quint32 height, quint32 refreshRate);
    Q_INVOKABLE bool switchToDisplays(const QVariantList &displays, const QString &primaryDevicePath = QString());
    Q_INVOKABLE bool restoreOriginalConfiguration();
    Q_INVOKABLE bool saveCurrentConfiguration();
    Q_INVOKABLE void refreshDisplays();
    Q_INVOKABLE QVariantList getSupportedModes(const QString &devicePath);
    Q_INVOKABLE QVariantMap getCurrentOrNativeMode(const QString &devicePath);

    // Public methods for saved topology access
    const std::vector<DISPLAYCONFIG_PATH_INFO>& getSavedPaths() const { return m_savedConfig.paths; }
    const std::vector<DISPLAYCONFIG_MODE_INFO>& getSavedModes() const { return m_savedConfig.modes; }
    bool restoreTopology(const std::vector<DISPLAYCONFIG_PATH_INFO> &paths, const std::vector<DISPLAYCONFIG_MODE_INFO> &modes);

signals:
    void displaysChanged();

private:
    static DisplayManager* s_instance;

    std::vector<DisplayInfo> enumerateDisplays();
    void updateDisplays();
    SavedConfig saveTopology();
    bool setOnlyDisplay(const DisplayInfo &target);
    bool setOnlyDisplayWithMode(const DisplayInfo &target, const DisplayMode &mode);
    bool setOnlyDisplaysWithModes(const std::vector<DisplayTarget> &targets);
    bool restoreTopologyFromSavedConfig(const SavedConfig &config);

    QVariantList m_displays;
    SavedConfig m_savedConfig;
    bool m_configSaved;
};

#endif // DISPLAYMANAGER_H
