#include "displaymanager.h"
#include "logmanager.h"
#include <QDebug>
#include <functional>
#include <map>
#include <set>
#include <tuple>

static std::wstring findGdiNameForMonitor(const std::wstring &targetMonitorPath);

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

    auto sourceKey = [](const LUID &adapter, UINT32 id) -> std::pair<uint64_t, uint32_t> {
        return std::make_pair(((uint64_t)adapter.HighPart << 32) | (uint32_t)adapter.LowPart, id);
    };

    auto getMonitorPathForPath = [&](UINT32 i, std::wstring &out) -> bool {
        DISPLAYCONFIG_TARGET_DEVICE_NAME targetName = {};
        targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        targetName.header.size = sizeof(targetName);
        targetName.header.adapterId = paths[i].targetInfo.adapterId;
        targetName.header.id = paths[i].targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&targetName.header) != ERROR_SUCCESS) return false;
        out = targetName.monitorDevicePath;
        return true;
    };

    // Indices into the `paths` array, in the order we want to process them.
    std::vector<UINT32> chosenPathIndices;
    std::set<std::pair<uint64_t, uint32_t>> usedSources;

    // Pass 1: claim ACTIVE paths first — their source IDs are already in use by Windows.
    for (UINT32 i = 0; i < pathCount; i++) {
        if (!(paths[i].flags & DISPLAYCONFIG_PATH_ACTIVE)) continue;
        std::wstring devicePath;
        if (!getMonitorPathForPath(i, devicePath)) continue;

        for (size_t t = 0; t < targets.size(); t++) {
            if (targetMatched[t]) continue;
            if (targets[t].device_path != devicePath) continue;
            targetMatched[t] = true;
            chosenPathIndices.push_back(i);
            usedSources.insert(sourceKey(paths[i].sourceInfo.adapterId, paths[i].sourceInfo.id));
            break;
        }
    }

    // Pass 2: pick inactive paths for the remaining targets, choosing a source not yet taken.
    for (size_t t = 0; t < targets.size(); t++) {
        if (targetMatched[t]) continue;
        UINT32 bestPathIdx = UINT32_MAX;
        for (UINT32 i = 0; i < pathCount; i++) {
            if (paths[i].flags & DISPLAYCONFIG_PATH_ACTIVE) continue;
            std::wstring devicePath;
            if (!getMonitorPathForPath(i, devicePath)) continue;
            if (devicePath != targets[t].device_path) continue;

            auto key = sourceKey(paths[i].sourceInfo.adapterId, paths[i].sourceInfo.id);
            if (usedSources.count(key)) continue;
            bestPathIdx = i;
            break;
        }
        if (bestPathIdx == UINT32_MAX) {
            LogManager::warning(QString("No free source-path for inactive target %1")
                                .arg(QString::fromStdWString(targets[t].device_path)));
            continue;
        }
        targetMatched[t] = true;
        chosenPathIndices.push_back(bestPathIdx);
        usedSources.insert(sourceKey(paths[bestPathIdx].sourceInfo.adapterId, paths[bestPathIdx].sourceInfo.id));
    }

    LogManager::debug(QString("Chose %1 paths after dedup").arg(chosenPathIndices.size()));

    for (UINT32 i : chosenPathIndices) {
        std::wstring devicePath;
        if (!getMonitorPathForPath(i, devicePath)) continue;

        size_t matchedIdx = SIZE_MAX;
        for (size_t t = 0; t < targets.size(); t++) {
            if (targets[t].device_path == devicePath) {
                matchedIdx = t;
                break;
            }
        }
        if (matchedIdx == SIZE_MAX) continue;

        const DisplayTarget &target = targets[matchedIdx];
        bool wasActive = (paths[i].flags & DISPLAYCONFIG_PATH_ACTIVE) != 0;
        LogManager::debug(QString("Building path for target=%1 wasActive=%2 sourceId=%3")
                          .arg(QString::fromStdWString(target.device_path))
                          .arg(wasActive ? "true" : "false")
                          .arg(paths[i].sourceInfo.id));

        auto path = paths[i];
        path.flags = DISPLAYCONFIG_PATH_ACTIVE;

        UINT32 sourceModeIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;
        UINT32 targetModeIdx = DISPLAYCONFIG_PATH_MODE_IDX_INVALID;

        bool hasExistingSource = (path.sourceInfo.modeInfoIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
                                  path.sourceInfo.modeInfoIdx < modeCount);
        bool hasExistingTarget = (path.targetInfo.modeInfoIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
                                  path.targetInfo.modeInfoIdx < modeCount);

        if (hasExistingSource && hasExistingTarget) {
            // Active path: keep existing mode info verbatim. Resolution / refresh
            // changes (if any) are applied per-monitor via ChangeDisplaySettingsExW
            // after SetDisplayConfig succeeds — that path is far more tolerant of
            // timing recomputation than mutating DISPLAYCONFIG_VIDEO_SIGNAL_INFO.
            newModes.push_back(modes[path.sourceInfo.modeInfoIdx]);
            sourceModeIdx = (UINT32)newModes.size() - 1;
            newModes.push_back(modes[path.targetInfo.modeInfoIdx]);
            targetModeIdx = (UINT32)newModes.size() - 1;
        } else {
            // Inactive path: activate it at its preferred mode. The user-requested
            // resolution / refresh is applied afterwards via ChangeDisplaySettingsExW.
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

            LogManager::debug(QString("Inactive path: activating at preferred mode %1x%2")
                              .arg(preferred.width).arg(preferred.height));

            DISPLAYCONFIG_MODE_INFO sourceModeInfo = {};
            sourceModeInfo.infoType = DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE;
            sourceModeInfo.adapterId = paths[i].sourceInfo.adapterId;
            sourceModeInfo.id = paths[i].sourceInfo.id;
            sourceModeInfo.sourceMode.width = preferred.width;
            sourceModeInfo.sourceMode.height = preferred.height;
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

    // Step 2: apply per-monitor resolution / refresh for targets that explicitly
    // asked for one. ChangeDisplaySettingsExW handles timing recomputation properly.
    bool anyChange = false;
    for (const auto &target : targets) {
        if (!target.has_mode) continue;
        anyChange = true;

        std::wstring gdiName = findGdiNameForMonitor(target.device_path);
        if (gdiName.empty()) {
            LogManager::warning(QString("Custom mode skipped: no GDI name for %1")
                                .arg(QString::fromStdWString(target.device_path)));
            continue;
        }

        DEVMODEW devMode = {};
        devMode.dmSize = sizeof(devMode);
        if (!EnumDisplaySettingsExW(gdiName.c_str(), ENUM_CURRENT_SETTINGS, &devMode, 0)) {
            LogManager::warning(QString("Custom mode skipped: cannot query current DEVMODE for %1")
                                .arg(QString::fromStdWString(target.device_path)));
            continue;
        }
        devMode.dmPelsWidth = target.mode.width;
        devMode.dmPelsHeight = target.mode.height;
        devMode.dmDisplayFrequency = target.mode.refreshRate;
        devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

        LONG cds = ChangeDisplaySettingsExW(gdiName.c_str(), &devMode, nullptr,
                                            CDS_UPDATEREGISTRY | CDS_NORESET, nullptr);
        if (cds != DISP_CHANGE_SUCCESSFUL) {
            LogManager::warning(QString("ChangeDisplaySettingsExW failed for %1 mode=%2x%3@%4 err=%5")
                                .arg(QString::fromStdWString(target.device_path))
                                .arg(target.mode.width).arg(target.mode.height).arg(target.mode.refreshRate)
                                .arg(cds));
        } else {
            LogManager::info(QString("Queued custom mode for %1: %2x%3@%4")
                             .arg(QString::fromStdWString(target.device_path))
                             .arg(target.mode.width).arg(target.mode.height).arg(target.mode.refreshRate));
        }
    }

    if (anyChange) {
        ChangeDisplaySettingsExW(nullptr, nullptr, nullptr, 0, nullptr);
    }

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

static QByteArray readMonitorEdid(const QString &devicePath)
{
    QString path = devicePath;
    if (path.startsWith("\\\\?\\")) path = path.mid(4);
    QStringList parts = path.split('#', Qt::SkipEmptyParts);
    if (parts.size() < 3) return QByteArray();

    std::wstring regPath = QString("SYSTEM\\CurrentControlSet\\Enum\\DISPLAY\\%1\\%2\\Device Parameters")
                               .arg(parts[1], parts[2]).toStdWString();

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return QByteArray();
    }

    DWORD type = 0;
    DWORD size = 0;
    QByteArray result;
    if (RegQueryValueExW(key, L"EDID", nullptr, &type, nullptr, &size) == ERROR_SUCCESS &&
        type == REG_BINARY && size > 0 && size < 4096) {
        result.resize((int)size);
        if (RegQueryValueExW(key, L"EDID", nullptr, &type,
                             reinterpret_cast<LPBYTE>(result.data()), &size) != ERROR_SUCCESS) {
            result.clear();
        }
    }
    RegCloseKey(key);
    return result;
}

namespace {
struct VicMode { quint8 vic; quint16 w; quint16 h; quint16 r; };
// Subset of CTA-861 VIC codes — covers SDTV/HDTV up to 4K@240. Multiple VICs that
// represent the same (w,h,r) with different color subsampling get deduped naturally.
static const VicMode kVicTable[] = {
    {  1,  640,  480, 60}, {  2,  720,  480, 60}, {  3,  720,  480, 60},
    {  4, 1280,  720, 60}, {  6,  720,  480, 60}, {  7,  720,  480, 60},
    { 16, 1920, 1080, 60}, { 17,  720,  576, 50}, { 18,  720,  576, 50},
    { 19, 1280,  720, 50}, { 31, 1920, 1080, 50}, { 32, 1920, 1080, 24},
    { 33, 1920, 1080, 25}, { 34, 1920, 1080, 30}, { 60, 1280,  720, 24},
    { 61, 1280,  720, 25}, { 62, 1280,  720, 30}, { 63, 1920, 1080,120},
    { 64, 1920, 1080,100}, { 65, 1280,  720,100}, { 66, 1280,  720,120},
    { 93, 3840, 2160, 24}, { 94, 3840, 2160, 25}, { 95, 3840, 2160, 30},
    { 96, 3840, 2160, 50}, { 97, 3840, 2160, 60}, { 98, 4096, 2160, 24},
    { 99, 4096, 2160, 25}, {100, 4096, 2160, 30}, {101, 4096, 2160, 50},
    {102, 4096, 2160, 60}, {103, 3840, 2160,100}, {104, 3840, 2160,120},
    {117, 3840, 2160,100}, {118, 3840, 2160,120}, {119, 3840, 2160,100},
    {120, 3840, 2160,120}, {193, 5120, 2160,100}, {194, 5120, 2160,120},
    {195, 7680, 4320, 24}, {196, 7680, 4320, 25}, {197, 7680, 4320, 30},
    {198, 7680, 4320, 48}, {199, 7680, 4320, 50}, {200, 7680, 4320, 60},
    {201, 7680, 4320,100}, {202, 7680, 4320,120},
};
}

static void parseEdidDtd(const quint8 *dtd,
                         std::function<void(quint32, quint32, quint32)> addMode)
{
    quint16 pixelClock = dtd[0] | (dtd[1] << 8);
    if (pixelClock == 0) return; // descriptor, not a timing
    quint32 hActive = dtd[2] | ((quint32)(dtd[4] & 0xF0) << 4);
    quint32 hBlank  = dtd[3] | ((quint32)(dtd[4] & 0x0F) << 8);
    quint32 vActive = dtd[5] | ((quint32)(dtd[7] & 0xF0) << 4);
    quint32 vBlank  = dtd[6] | ((quint32)(dtd[7] & 0x0F) << 8);
    if (dtd[17] & 0x80) return; // interlaced
    quint32 htotal = hActive + hBlank;
    quint32 vtotal = vActive + vBlank;
    if (htotal == 0 || vtotal == 0) return;

    quint64 pixelHz = (quint64)pixelClock * 10000ULL;
    quint64 denom = (quint64)htotal * vtotal;
    quint32 refresh = (quint32)((pixelHz + denom / 2) / denom);
    addMode(hActive, vActive, refresh);
}

static void parseEdid(const QByteArray &edid,
                      std::function<void(quint32, quint32, quint32)> addMode)
{
    if (edid.size() < 128) return;
    const quint8 *b = reinterpret_cast<const quint8 *>(edid.constData());

    static const quint8 header[] = {0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0};
    if (memcmp(b, header, 8) != 0) return;

    // Established Timings I (byte 35)
    static const struct { quint8 mask; quint32 w, h, r; } estI[] = {
        {0x80, 720, 400, 70}, {0x40, 720, 400, 88},
        {0x20, 640, 480, 60}, {0x10, 640, 480, 67},
        {0x08, 640, 480, 72}, {0x04, 640, 480, 75},
        {0x02, 800, 600, 56}, {0x01, 800, 600, 60},
    };
    for (auto &e : estI) {
        if (b[35] & e.mask) addMode(e.w, e.h, e.r);
    }
    // Established Timings II (byte 36)
    static const struct { quint8 mask; quint32 w, h, r; } estII[] = {
        {0x80, 800, 600, 72}, {0x40, 800, 600, 75},
        {0x20, 832, 624, 75},
        {0x08, 1024, 768, 60}, {0x04, 1024, 768, 70}, {0x02, 1024, 768, 75},
        {0x01, 1280, 1024, 75},
    };
    for (auto &e : estII) {
        if (b[36] & e.mask) addMode(e.w, e.h, e.r);
    }

    // Base block DTDs (bytes 54..125, four 18-byte entries)
    for (int i = 0; i < 4; i++) {
        parseEdidDtd(b + 54 + i * 18, addMode);
    }

    // Extension blocks
    quint8 extCount = b[126];
    int totalLen = edid.size();
    for (int e = 0; e < extCount; e++) {
        int extOffset = 128 * (e + 1);
        if (extOffset + 128 > totalLen) break;
        const quint8 *ext = b + extOffset;
        if (ext[0] != 0x02) continue; // not CTA-861
        quint8 dtdOffset = ext[2];
        if (dtdOffset == 0) continue;

        // Data block collection (bytes 4 to dtdOffset-1)
        int idx = 4;
        while (idx < dtdOffset && idx < 128) {
            quint8 tag = (ext[idx] & 0xE0) >> 5;
            quint8 len = ext[idx] & 0x1F;
            if (tag == 2) { // Short Video Descriptors
                for (int i = 1; i <= len && (idx + i) < 128; i++) {
                    quint8 vic = ext[idx + i] & 0x7F;
                    for (auto &v : kVicTable) {
                        if (v.vic == vic) { addMode(v.w, v.h, v.r); break; }
                    }
                }
            } else if (tag == 3 && len >= 7) { // Vendor-Specific Data Block — may carry HDMI 4K codes
                // HDMI VSDB: bytes 1-3 are OUI, check for 00-0C-03 (HDMI Licensing LLC)
                if (ext[idx + 1] == 0x03 && ext[idx + 2] == 0x0C && ext[idx + 3] == 0x00) {
                    // HDMI_VIC codes are after byte 7+VLEN+ALEN; complex parsing — skip for now.
                }
            }
            idx += len + 1;
        }

        // DTDs in the extension block
        int dtdIdx = dtdOffset;
        while (dtdIdx + 18 <= 128) {
            // First two bytes 0 => end of DTDs
            if (ext[dtdIdx] == 0 && ext[dtdIdx + 1] == 0) break;
            parseEdidDtd(ext + dtdIdx, addMode);
            dtdIdx += 18;
        }
    }
}

QVariantList DisplayManager::getSupportedModes(const QString &devicePath)
{
    QVariantList result;
    std::set<std::tuple<quint32, quint32, quint32>> seen;
    auto addMode = [&](quint32 w, quint32 h, quint32 r) {
        if (w < 640 || h < 480 || r < 24) return;
        auto key = std::make_tuple(w, h, r);
        if (!seen.insert(key).second) return;
        QVariantMap m;
        m["width"] = w;
        m["height"] = h;
        m["refreshRate"] = r;
        result.append(m);
    };

    std::wstring targetPath = devicePath.toStdWString();
    std::wstring gdiName = findGdiNameForMonitor(targetPath);
    int fromEnum = 0;
    if (!gdiName.empty()) {
        DEVMODEW devMode = {};
        devMode.dmSize = sizeof(devMode);
        for (DWORD i = 0; EnumDisplaySettingsExW(gdiName.c_str(), i, &devMode, 0); i++) {
            if (devMode.dmBitsPerPel != 32) continue;
            if (devMode.dmDisplayFlags & DM_INTERLACED) continue;
            size_t before = seen.size();
            addMode((quint32)devMode.dmPelsWidth, (quint32)devMode.dmPelsHeight,
                    (quint32)devMode.dmDisplayFrequency);
            if (seen.size() > before) fromEnum++;
        }
    } else {
        LogManager::warning(QString("Could not find GDI device name for ") + devicePath);
    }

    // EDID parse — fills in modes the source's mode list misses (especially for
    // inactive monitors where Windows reports a truncated set).
    QByteArray edid = readMonitorEdid(devicePath);
    int fromEdid = 0;
    if (!edid.isEmpty()) {
        size_t before = seen.size();
        parseEdid(edid, addMode);
        fromEdid = (int)(seen.size() - before);
    }

    // Preferred mode from DisplayConfig — last-resort native mode.
    UINT32 pathCount = 0, modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ALL_PATHS, &pathCount, &modeCount) == ERROR_SUCCESS) {
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        if (QueryDisplayConfig(QDC_ALL_PATHS, &pathCount, paths.data(),
                               &modeCount, modes.data(), nullptr) == ERROR_SUCCESS) {
            for (UINT32 i = 0; i < pathCount; i++) {
                DISPLAYCONFIG_TARGET_DEVICE_NAME name = {};
                name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
                name.header.size = sizeof(name);
                name.header.adapterId = paths[i].targetInfo.adapterId;
                name.header.id = paths[i].targetInfo.id;
                if (DisplayConfigGetDeviceInfo(&name.header) != ERROR_SUCCESS) continue;
                if (std::wstring(name.monitorDevicePath) != targetPath) continue;

                DISPLAYCONFIG_TARGET_PREFERRED_MODE pref = {};
                pref.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_PREFERRED_MODE;
                pref.header.size = sizeof(pref);
                pref.header.adapterId = paths[i].targetInfo.adapterId;
                pref.header.id = paths[i].targetInfo.id;
                if (DisplayConfigGetDeviceInfo(&pref.header) == ERROR_SUCCESS) {
                    const auto &freq = pref.targetMode.targetVideoSignalInfo.vSyncFreq;
                    quint32 r = (freq.Denominator > 0)
                        ? (quint32)((freq.Numerator + freq.Denominator / 2) / freq.Denominator) : 60;
                    addMode(pref.width, pref.height, r);
                }
                break;
            }
        }
    }

    LogManager::debug(QString("getSupportedModes(%1): %2 total (enum=%3, edid=%4)")
                      .arg(devicePath).arg(result.size()).arg(fromEnum).arg(fromEdid));
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
