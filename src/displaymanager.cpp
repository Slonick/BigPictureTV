#include "displaymanager.h"
#include "logmanager.h"
#include <QDebug>
#include <map>
#include <set>
#include <tuple>

DisplayManager* DisplayManager::s_instance = nullptr;

DisplayManager* DisplayManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    if (!s_instance) {
        s_instance = new DisplayManager();
    }
    return s_instance;
}

DisplayManager* DisplayManager::instance()
{
    return s_instance;
}

DisplayManager::DisplayManager(QObject *parent)
    : QObject(parent)
    , m_configSaved(false)
{
    updateDisplays();
}

DisplayManager::~DisplayManager()
{
}

std::vector<DisplayInfo> DisplayManager::enumerateDisplays()
{
    std::vector<DisplayInfo> displays;
    std::map<std::wstring, DisplayInfo> uniqueDisplays;

    UINT32 pathCount = 0, modeCount = 0;

    if (GetDisplayConfigBufferSizes(QDC_ALL_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) {
        qWarning() << "Failed to get display config buffer sizes";
        return displays;
    }

    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);

    if (QueryDisplayConfig(QDC_ALL_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) != ERROR_SUCCESS) {
        qWarning() << "Failed to query display config";
        return displays;
    }

    for (UINT32 i = 0; i < pathCount; i++) {
        DISPLAYCONFIG_TARGET_DEVICE_NAME targetName = {};
        targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        targetName.header.size = sizeof(targetName);
        targetName.header.adapterId = paths[i].targetInfo.adapterId;
        targetName.header.id = paths[i].targetInfo.id;

        if (DisplayConfigGetDeviceInfo(&targetName.header) != ERROR_SUCCESS) {
            continue;
        }

        if (wcslen(targetName.monitorDevicePath) == 0) {
            continue;
        }

        std::wstring devicePath = targetName.monitorDevicePath;

        if (uniqueDisplays.find(devicePath) == uniqueDisplays.end()) {
            DisplayInfo info;
            info.device_path = devicePath;
            info.adapter_id = paths[i].targetInfo.adapterId;
            info.target_id = paths[i].targetInfo.id;
            info.source_id = paths[i].sourceInfo.id;
            info.friendly_name = targetName.monitorFriendlyDeviceName;
            info.is_active = (paths[i].flags & DISPLAYCONFIG_PATH_ACTIVE) != 0;

            if (info.friendly_name.empty()) {
                info.friendly_name = L"Generic Monitor";
            }

            uniqueDisplays[devicePath] = info;
        }
    }

    for (auto &pair : uniqueDisplays) {
        displays.push_back(pair.second);
    }

    return displays;
}

void DisplayManager::updateDisplays()
{
    QVariantList result;
    auto displays = enumerateDisplays();

    for (const auto &display : displays) {
        QVariantMap displayMap;
        displayMap["name"] = QString::fromStdWString(display.friendly_name);
        displayMap["devicePath"] = QString::fromStdWString(display.device_path);
        displayMap["isActive"] = display.is_active;
        result.append(displayMap);
    }

    if (m_displays != result) {
        m_displays = result;
        emit displaysChanged();
    }
}

void DisplayManager::refreshDisplays()
{
    updateDisplays();
}

SavedConfig DisplayManager::saveTopology()
{
    SavedConfig config;
    UINT32 pathCount = 0, modeCount = 0;

    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) {
        qWarning() << "Failed to get display config buffer sizes for saving";
        return config;
    }

    config.paths.resize(pathCount);
    config.modes.resize(modeCount);

    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, config.paths.data(),
                          &modeCount, config.modes.data(), nullptr) != ERROR_SUCCESS) {
        qWarning() << "Failed to query display config for saving";
        config.paths.clear();
        config.modes.clear();
    }

    return config;
}

bool DisplayManager::saveCurrentConfiguration()
{
    m_savedConfig = saveTopology();
    m_configSaved = !m_savedConfig.paths.empty();

    if (!m_configSaved) {
        qWarning() << "Failed to save current display configuration";
    }

    return m_configSaved;
}

bool DisplayManager::setOnlyDisplay(const DisplayInfo &target)
{
    UINT32 pathCount = 0, modeCount = 0;

    if (GetDisplayConfigBufferSizes(QDC_ALL_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) {
        qWarning() << "Failed to get display config buffer sizes for setting display";
        return false;
    }

    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);

    if (QueryDisplayConfig(QDC_ALL_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) != ERROR_SUCCESS) {
        qWarning() << "Failed to query display config for setting display";
        return false;
    }

    std::vector<DISPLAYCONFIG_PATH_INFO> newPaths;
    std::vector<DISPLAYCONFIG_MODE_INFO> newModes;

    // Find the matching path by comparing device path
    for (UINT32 i = 0; i < pathCount; i++) {
        DISPLAYCONFIG_TARGET_DEVICE_NAME targetName = {};
        targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        targetName.header.size = sizeof(targetName);
        targetName.header.adapterId = paths[i].targetInfo.adapterId;
        targetName.header.id = paths[i].targetInfo.id;

        if (DisplayConfigGetDeviceInfo(&targetName.header) != ERROR_SUCCESS) {
            continue;
        }

        std::wstring devicePath = targetName.monitorDevicePath;

        if (devicePath == target.device_path) {
            // This is our target display - keep it active
            auto path = paths[i];
            path.flags = DISPLAYCONFIG_PATH_ACTIVE;

            // Copy modes
            UINT32 sourceModeIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;
            UINT32 targetModeIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;

            if (path.sourceInfo.modeInfoIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
                path.sourceInfo.modeInfoIdx < modeCount) {
                newModes.push_back(modes[path.sourceInfo.modeInfoIdx]);
                sourceModeIdx = (UINT32)newModes.size() - 1;
            }

            if (path.targetInfo.modeInfoIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
                path.targetInfo.modeInfoIdx < modeCount) {
                newModes.push_back(modes[path.targetInfo.modeInfoIdx]);
                targetModeIdx = (UINT32)newModes.size() - 1;
            }

            path.sourceInfo.modeInfoIdx = sourceModeIdx;
            path.targetInfo.modeInfoIdx = targetModeIdx;

            newPaths.push_back(path);
            break; // Found our display
        }
    }

    if (newPaths.empty()) {
        qWarning() << "Failed to find target display path";
        return false;
    }

    LONG result = SetDisplayConfig((UINT32)newPaths.size(), newPaths.data(),
                                   (UINT32)newModes.size(), newModes.data(),
                                   SDC_APPLY | SDC_USE_SUPPLIED_DISPLAY_CONFIG |
                                       SDC_ALLOW_CHANGES | SDC_SAVE_TO_DATABASE);

    if (result != ERROR_SUCCESS) {
        qWarning() << "SetDisplayConfig failed with error:" << result;
        return false;
    }

    return true;
}

bool DisplayManager::setOnlyDisplayWithMode(const DisplayInfo &target, const DisplayMode &mode)
{
    UINT32 pathCount = 0, modeCount = 0;

    if (GetDisplayConfigBufferSizes(QDC_ALL_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) {
        qWarning() << "Failed to get display config buffer sizes for setting display with mode";
        return false;
    }

    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);

    if (QueryDisplayConfig(QDC_ALL_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) != ERROR_SUCCESS) {
        qWarning() << "Failed to query display config for setting display with mode";
        return false;
    }

    std::vector<DISPLAYCONFIG_PATH_INFO> newPaths;
    std::vector<DISPLAYCONFIG_MODE_INFO> newModes;

    // Find the matching path by comparing device path
    for (UINT32 i = 0; i < pathCount; i++) {
        DISPLAYCONFIG_TARGET_DEVICE_NAME targetName = {};
        targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        targetName.header.size = sizeof(targetName);
        targetName.header.adapterId = paths[i].targetInfo.adapterId;
        targetName.header.id = paths[i].targetInfo.id;

        if (DisplayConfigGetDeviceInfo(&targetName.header) != ERROR_SUCCESS) {
            continue;
        }

        std::wstring devicePath = targetName.monitorDevicePath;

        if (devicePath == target.device_path) {
            // This is our target display - keep it active
            auto path = paths[i];
            path.flags = DISPLAYCONFIG_PATH_ACTIVE;

            // Copy and modify modes
            UINT32 sourceModeIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;
            UINT32 targetModeIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;

            if (path.sourceInfo.modeInfoIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
                path.sourceInfo.modeInfoIdx < modeCount) {
                auto sourceMode = modes[path.sourceInfo.modeInfoIdx];
                // Modify source mode resolution
                sourceMode.sourceMode.width = mode.width;
                sourceMode.sourceMode.height = mode.height;
                newModes.push_back(sourceMode);
                sourceModeIdx = (UINT32)newModes.size() - 1;
            }

            if (path.targetInfo.modeInfoIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
                path.targetInfo.modeInfoIdx < modeCount) {
                auto targetMode = modes[path.targetInfo.modeInfoIdx];
                // Modify target mode resolution and refresh rate
                targetMode.targetMode.targetVideoSignalInfo.activeSize.cx = mode.width;
                targetMode.targetMode.targetVideoSignalInfo.activeSize.cy = mode.height;
                targetMode.targetMode.targetVideoSignalInfo.totalSize.cx = mode.width;
                targetMode.targetMode.targetVideoSignalInfo.totalSize.cy = mode.height;

                // Set refresh rate (vSyncFreq is a rational number: numerator/denominator)
                targetMode.targetMode.targetVideoSignalInfo.vSyncFreq.Numerator = mode.refreshRate;
                targetMode.targetMode.targetVideoSignalInfo.vSyncFreq.Denominator = 1;

                newModes.push_back(targetMode);
                targetModeIdx = (UINT32)newModes.size() - 1;
            }

            path.sourceInfo.modeInfoIdx = sourceModeIdx;
            path.targetInfo.modeInfoIdx = targetModeIdx;

            newPaths.push_back(path);
            break; // Found our display
        }
    }

    if (newPaths.empty()) {
        qWarning() << "Failed to find target display path for mode setting";
        return false;
    }

    qDebug() << "Setting display to" << mode.width << "x" << mode.height << "@" << mode.refreshRate << "Hz";

    LONG result = SetDisplayConfig((UINT32)newPaths.size(), newPaths.data(),
                                   (UINT32)newModes.size(), newModes.data(),
                                   SDC_APPLY | SDC_USE_SUPPLIED_DISPLAY_CONFIG |
                                       SDC_ALLOW_CHANGES | SDC_SAVE_TO_DATABASE);

    if (result != ERROR_SUCCESS) {
        qWarning() << "SetDisplayConfig failed with error:" << result;
        return false;
    }

    return true;
}

bool DisplayManager::switchToDisplay(const QString &devicePath)
{
    auto displays = enumerateDisplays();
    std::wstring targetPath = devicePath.toStdWString();

    for (const auto &display : displays) {
        if (display.device_path == targetPath) {
            return setOnlyDisplay(display);
        }
    }

    qWarning() << "Display not found:" << devicePath;
    return false;
}

bool DisplayManager::switchToDisplayWithResolution(const QString &devicePath, quint32 width, quint32 height, quint32 refreshRate)
{
    auto displays = enumerateDisplays();
    std::wstring targetPath = devicePath.toStdWString();

    for (const auto &display : displays) {
        if (display.device_path == targetPath) {
            DisplayMode mode;
            mode.width = width;
            mode.height = height;
            mode.refreshRate = refreshRate;
            return setOnlyDisplayWithMode(display, mode);
        }
    }

    qWarning() << "Display not found:" << devicePath;
    return false;
}

bool DisplayManager::switchToDisplays(const QVariantList &displays, const QString &primaryDevicePath)
{
    if (displays.isEmpty()) {
        qWarning() << "No displays specified for switching";
        return false;
    }

    std::wstring primaryPath = primaryDevicePath.toStdWString();
    std::vector<DisplayTarget> targets;
    for (const QVariant &item : displays) {
        QVariantMap map = item.toMap();
        QString devicePath = map.value("devicePath").toString();
        if (devicePath.isEmpty()) {
            continue;
        }

        DisplayTarget target;
        target.device_path = devicePath.toStdWString();
        quint32 width = map.value("width").toUInt();
        quint32 height = map.value("height").toUInt();
        quint32 refreshRate = map.value("refreshRate").toUInt();

        target.has_mode = (width > 0 && height > 0 && refreshRate > 0);
        target.mode.width = width;
        target.mode.height = height;
        target.mode.refreshRate = refreshRate;
        target.is_primary = !primaryPath.empty() && target.device_path == primaryPath;

        targets.push_back(target);
    }

    if (targets.empty()) {
        qWarning() << "No valid display targets after parsing";
        return false;
    }

    bool anyPrimary = false;
    for (const auto &t : targets) {
        if (t.is_primary) { anyPrimary = true; break; }
    }
    if (!anyPrimary && !targets.empty()) {
        targets.front().is_primary = true;
    }

    return setOnlyDisplaysWithModes(targets);
}

bool DisplayManager::setOnlyDisplaysWithModes(const std::vector<DisplayTarget> &targets)
{
    LogManager::info(QString("setOnlyDisplaysWithModes: %1 target(s) requested").arg(targets.size()));
    for (const auto &t : targets) {
        LogManager::info(QString("  target=%1 mode=%2x%3@%4 primary=%5")
                         .arg(QString::fromStdWString(t.device_path))
                         .arg(t.mode.width).arg(t.mode.height).arg(t.mode.refreshRate)
                         .arg(t.is_primary ? "yes" : "no"));
    }

    UINT32 pathCount = 0, modeCount = 0;

    if (GetDisplayConfigBufferSizes(QDC_ALL_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) {
        LogManager::error("Failed to get display config buffer sizes for setting displays");
        return false;
    }

    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);

    if (QueryDisplayConfig(QDC_ALL_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) != ERROR_SUCCESS) {
        LogManager::error("Failed to query display config for setting displays");
        return false;
    }

    LogManager::debug(QString("QDC_ALL_PATHS returned %1 paths, %2 modes").arg(pathCount).arg(modeCount));

    std::vector<DISPLAYCONFIG_PATH_INFO> newPaths;
    std::vector<DISPLAYCONFIG_MODE_INFO> newModes;
    std::vector<bool> targetMatched(targets.size(), false);
    std::vector<UINT32> sourceModeIndices;
    UINT32 primarySourceModeIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;

    for (UINT32 i = 0; i < pathCount; i++) {
        DISPLAYCONFIG_TARGET_DEVICE_NAME targetName = {};
        targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        targetName.header.size = sizeof(targetName);
        targetName.header.adapterId = paths[i].targetInfo.adapterId;
        targetName.header.id = paths[i].targetInfo.id;

        if (DisplayConfigGetDeviceInfo(&targetName.header) != ERROR_SUCCESS) {
            continue;
        }

        std::wstring devicePath = targetName.monitorDevicePath;

        size_t matchedIdx = SIZE_MAX;
        for (size_t t = 0; t < targets.size(); t++) {
            if (!targetMatched[t] && targets[t].device_path == devicePath) {
                matchedIdx = t;
                break;
            }
        }

        if (matchedIdx == SIZE_MAX) {
            continue;
        }
        targetMatched[matchedIdx] = true;

        const DisplayTarget &target = targets[matchedIdx];
        bool wasActive = (paths[i].flags & DISPLAYCONFIG_PATH_ACTIVE) != 0;
        LogManager::debug(QString("Matched target=%1 wasActive=%2")
                          .arg(QString::fromStdWString(target.device_path))
                          .arg(wasActive ? "true" : "false"));

        auto path = paths[i];
        path.flags = DISPLAYCONFIG_PATH_ACTIVE;

        UINT32 sourceModeIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;
        UINT32 targetModeIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;

        bool hasExistingSource = (path.sourceInfo.modeInfoIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
                                  path.sourceInfo.modeInfoIdx < modeCount);
        bool hasExistingTarget = (path.targetInfo.modeInfoIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
                                  path.targetInfo.modeInfoIdx < modeCount);

        if (hasExistingSource && hasExistingTarget) {
            // Active path: reuse existing modes, override resolution/refresh
            auto sourceMode = modes[path.sourceInfo.modeInfoIdx];
            auto targetMode = modes[path.targetInfo.modeInfoIdx];
            if (target.has_mode) {
                sourceMode.sourceMode.width = target.mode.width;
                sourceMode.sourceMode.height = target.mode.height;
                targetMode.targetMode.targetVideoSignalInfo.activeSize.cx = target.mode.width;
                targetMode.targetMode.targetVideoSignalInfo.activeSize.cy = target.mode.height;
                targetMode.targetMode.targetVideoSignalInfo.totalSize.cx = target.mode.width;
                targetMode.targetMode.targetVideoSignalInfo.totalSize.cy = target.mode.height;
                targetMode.targetMode.targetVideoSignalInfo.vSyncFreq.Numerator = target.mode.refreshRate;
                targetMode.targetMode.targetVideoSignalInfo.vSyncFreq.Denominator = 1;
            }
            newModes.push_back(sourceMode);
            sourceModeIdx = (UINT32)newModes.size() - 1;
            newModes.push_back(targetMode);
            targetModeIdx = (UINT32)newModes.size() - 1;
        } else {
            // Inactive path: construct modes from the target's preferred mode
            DISPLAYCONFIG_TARGET_PREFERRED_MODE preferred = {};
            preferred.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_PREFERRED_MODE;
            preferred.header.size = sizeof(preferred);
            preferred.header.adapterId = paths[i].targetInfo.adapterId;
            preferred.header.id = paths[i].targetInfo.id;

            LONG prefResult = DisplayConfigGetDeviceInfo(&preferred.header);
            if (prefResult != ERROR_SUCCESS) {
                LogManager::warning(QString("Failed to get preferred mode for inactive target=%1 error=%2")
                                    .arg(QString::fromStdWString(target.device_path))
                                    .arg(prefResult));
                continue;
            }

            quint32 useWidth = target.has_mode ? target.mode.width : preferred.width;
            quint32 useHeight = target.has_mode ? target.mode.height : preferred.height;
            quint32 useRefresh = target.has_mode ? target.mode.refreshRate : 60;

            LogManager::debug(QString("Inactive path: building modes %1x%2@%3 (preferred=%4x%5)")
                              .arg(useWidth).arg(useHeight).arg(useRefresh)
                              .arg(preferred.width).arg(preferred.height));

            DISPLAYCONFIG_MODE_INFO sourceModeInfo = {};
            sourceModeInfo.infoType = DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE;
            sourceModeInfo.adapterId = paths[i].sourceInfo.adapterId;
            sourceModeInfo.id = paths[i].sourceInfo.id;
            sourceModeInfo.sourceMode.width = useWidth;
            sourceModeInfo.sourceMode.height = useHeight;
            sourceModeInfo.sourceMode.pixelFormat = DISPLAYCONFIG_PIXELFORMAT_32BPP;
            sourceModeInfo.sourceMode.position.x = 0;
            sourceModeInfo.sourceMode.position.y = 0;
            newModes.push_back(sourceModeInfo);
            sourceModeIdx = (UINT32)newModes.size() - 1;

            DISPLAYCONFIG_MODE_INFO targetModeInfo = {};
            targetModeInfo.infoType = DISPLAYCONFIG_MODE_INFO_TYPE_TARGET;
            targetModeInfo.adapterId = paths[i].targetInfo.adapterId;
            targetModeInfo.id = paths[i].targetInfo.id;
            targetModeInfo.targetMode = preferred.targetMode;

            if (target.has_mode) {
                targetModeInfo.targetMode.targetVideoSignalInfo.activeSize.cx = useWidth;
                targetModeInfo.targetMode.targetVideoSignalInfo.activeSize.cy = useHeight;
                targetModeInfo.targetMode.targetVideoSignalInfo.totalSize.cx = useWidth;
                targetModeInfo.targetMode.targetVideoSignalInfo.totalSize.cy = useHeight;
                targetModeInfo.targetMode.targetVideoSignalInfo.vSyncFreq.Numerator = useRefresh;
                targetModeInfo.targetMode.targetVideoSignalInfo.vSyncFreq.Denominator = 1;
            }
            newModes.push_back(targetModeInfo);
            targetModeIdx = (UINT32)newModes.size() - 1;
        }

        sourceModeIndices.push_back(sourceModeIdx);
        if (target.is_primary) {
            primarySourceModeIdx = sourceModeIdx;
        }

        path.sourceInfo.modeInfoIdx = sourceModeIdx;
        path.targetInfo.modeInfoIdx = targetModeIdx;

        newPaths.push_back(path);
    }

    if (newPaths.empty()) {
        LogManager::error("No matching display paths found for any of the requested targets");
        return false;
    }

    for (size_t t = 0; t < targets.size(); t++) {
        if (!targetMatched[t]) {
            LogManager::warning(QString("Display not matched in QDC_ALL_PATHS: %1")
                                .arg(QString::fromStdWString(targets[t].device_path)));
        }
    }

    if (primarySourceModeIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
        primarySourceModeIdx < newModes.size()) {
        LONG dx = newModes[primarySourceModeIdx].sourceMode.position.x;
        LONG dy = newModes[primarySourceModeIdx].sourceMode.position.y;
        if (dx != 0 || dy != 0) {
            LogManager::debug(QString("Shifting all source positions by (%1, %2) to place primary at (0,0)").arg(-dx).arg(-dy));
            for (UINT32 idx : sourceModeIndices) {
                if (idx < newModes.size()) {
                    newModes[idx].sourceMode.position.x -= dx;
                    newModes[idx].sourceMode.position.y -= dy;
                }
            }
        }
    }

    LogManager::info(QString("Calling SetDisplayConfig with %1 paths, %2 modes")
                     .arg(newPaths.size()).arg(newModes.size()));

    LONG result = SetDisplayConfig((UINT32)newPaths.size(), newPaths.data(),
                                   (UINT32)newModes.size(), newModes.data(),
                                   SDC_APPLY | SDC_USE_SUPPLIED_DISPLAY_CONFIG |
                                       SDC_ALLOW_CHANGES | SDC_SAVE_TO_DATABASE);

    if (result != ERROR_SUCCESS) {
        LogManager::error(QString("SetDisplayConfig failed with error: %1").arg(result));
        return false;
    }

    LogManager::info("SetDisplayConfig succeeded");
    return true;
}

bool DisplayManager::restoreTopology(const std::vector<DISPLAYCONFIG_PATH_INFO> &paths,
                                    const std::vector<DISPLAYCONFIG_MODE_INFO> &modes)
{
    if (paths.empty() || modes.empty()) {
        qWarning() << "Cannot restore empty topology";
        return false;
    }

    auto pathsCopy = paths;
    auto modesCopy = modes;

    LONG result = SetDisplayConfig((UINT32)pathsCopy.size(), pathsCopy.data(),
                                   (UINT32)modesCopy.size(), modesCopy.data(),
                                   SDC_APPLY | SDC_USE_SUPPLIED_DISPLAY_CONFIG |
                                       SDC_ALLOW_CHANGES | SDC_SAVE_TO_DATABASE);

    if (result != ERROR_SUCCESS) {
        qWarning() << "RestoreTopology failed with error:" << result;
        return false;
    }

    return true;
}

bool DisplayManager::restoreTopologyFromSavedConfig(const SavedConfig &config)
{
    if (config.paths.empty()) {
        qWarning() << "Cannot restore empty configuration";
        return false;
    }

    return restoreTopology(config.paths, config.modes);
}

static std::wstring findGdiNameForMonitor(const std::wstring &targetMonitorPath)
{
    DISPLAY_DEVICEW adapter = {};
    adapter.cb = sizeof(adapter);

    for (DWORD adapterIdx = 0; EnumDisplayDevicesW(nullptr, adapterIdx, &adapter, 0); adapterIdx++) {
        if (adapter.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) continue;

        DISPLAY_DEVICEW monitor = {};
        monitor.cb = sizeof(monitor);
        for (DWORD monIdx = 0; EnumDisplayDevicesW(adapter.DeviceName, monIdx, &monitor, EDD_GET_DEVICE_INTERFACE_NAME); monIdx++) {
            if (std::wstring(monitor.DeviceID) == targetMonitorPath) {
                return std::wstring(adapter.DeviceName);
            }
        }
    }
    return std::wstring();
}

QVariantList DisplayManager::getSupportedModes(const QString &devicePath)
{
    QVariantList result;
    std::wstring targetPath = devicePath.toStdWString();
    std::wstring gdiName = findGdiNameForMonitor(targetPath);

    if (gdiName.empty()) {
        qWarning() << "Could not find GDI device name for" << devicePath;
        return result;
    }

    std::set<std::tuple<DWORD, DWORD, DWORD>> seen;

    DEVMODEW devMode = {};
    devMode.dmSize = sizeof(devMode);
    for (DWORD i = 0; EnumDisplaySettingsExW(gdiName.c_str(), i, &devMode, 0); i++) {
        if (devMode.dmBitsPerPel != 32) continue;
        if (devMode.dmDisplayFlags & DM_INTERLACED) continue;
        if (devMode.dmDisplayFrequency < 24) continue;

        auto key = std::make_tuple(devMode.dmPelsWidth, devMode.dmPelsHeight, devMode.dmDisplayFrequency);
        if (!seen.insert(key).second) continue;

        QVariantMap entry;
        entry["width"] = (quint32)devMode.dmPelsWidth;
        entry["height"] = (quint32)devMode.dmPelsHeight;
        entry["refreshRate"] = (quint32)devMode.dmDisplayFrequency;
        result.append(entry);
    }

    return result;
}

QVariantMap DisplayManager::getCurrentOrNativeMode(const QString &devicePath)
{
    QVariantMap result;
    std::wstring targetPath = devicePath.toStdWString();

    // First, try to find the current mode from active paths
    UINT32 pathCount = 0, modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) == ERROR_SUCCESS) {
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);

        if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(),
                               &modeCount, modes.data(), nullptr) == ERROR_SUCCESS) {
            for (UINT32 i = 0; i < pathCount; i++) {
                DISPLAYCONFIG_TARGET_DEVICE_NAME targetName = {};
                targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
                targetName.header.size = sizeof(targetName);
                targetName.header.adapterId = paths[i].targetInfo.adapterId;
                targetName.header.id = paths[i].targetInfo.id;

                if (DisplayConfigGetDeviceInfo(&targetName.header) != ERROR_SUCCESS) continue;
                if (std::wstring(targetName.monitorDevicePath) != targetPath) continue;

                quint32 width = 0, height = 0, refreshRate = 0;
                if (paths[i].sourceInfo.modeInfoIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
                    paths[i].sourceInfo.modeInfoIdx < modeCount) {
                    width = modes[paths[i].sourceInfo.modeInfoIdx].sourceMode.width;
                    height = modes[paths[i].sourceInfo.modeInfoIdx].sourceMode.height;
                }
                if (paths[i].targetInfo.modeInfoIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
                    paths[i].targetInfo.modeInfoIdx < modeCount) {
                    const auto &freq = modes[paths[i].targetInfo.modeInfoIdx].targetMode.targetVideoSignalInfo.vSyncFreq;
                    if (freq.Denominator > 0) {
                        refreshRate = (quint32)((freq.Numerator + freq.Denominator / 2) / freq.Denominator);
                    }
                }

                if (width > 0 && height > 0 && refreshRate > 0) {
                    result["width"] = width;
                    result["height"] = height;
                    result["refreshRate"] = refreshRate;
                    return result;
                }
            }
        }
    }

    // Fallback: display is inactive — pick the highest supported mode
    QVariantList supported = getSupportedModes(devicePath);
    QVariantMap best;
    quint64 bestScore = 0;
    for (const QVariant &item : supported) {
        QVariantMap m = item.toMap();
        quint32 w = m.value("width").toUInt();
        quint32 h = m.value("height").toUInt();
        quint32 r = m.value("refreshRate").toUInt();
        quint64 score = (quint64)w * h * 1000ULL + r;
        if (score > bestScore) {
            bestScore = score;
            best = m;
        }
    }
    return best;
}

bool DisplayManager::restoreOriginalConfiguration()
{
    if (!m_configSaved) {
        qWarning() << "No saved configuration to restore";
        return false;
    }

    return restoreTopologyFromSavedConfig(m_savedConfig);
}
