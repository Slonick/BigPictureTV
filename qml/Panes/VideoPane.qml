pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.FluentWinUI3
import QtQuick.Layouts
import Odizinne.BigPictureTV

Pane {
    id: root

    function findDisplayIndex(devicePath) {
        const list = AppConfiguration.gamemodeDisplays
        for (let i = 0; i < list.length; i++) {
            if (list[i].devicePath === devicePath) {
                return i
            }
        }
        return -1
    }

    function isDisplayEnabled(devicePath) {
        return findDisplayIndex(devicePath) !== -1
    }

    function getDisplayField(devicePath, field, fallback) {
        const idx = findDisplayIndex(devicePath)
        if (idx === -1) return fallback
        const value = AppConfiguration.gamemodeDisplays[idx][field]
        return value === undefined ? fallback : value
    }

    function setDisplayEnabled(devicePath, enabled) {
        const list = AppConfiguration.gamemodeDisplays.slice()
        const idx = findDisplayIndex(devicePath)
        if (enabled && idx === -1) {
            const defaults = DisplayManager.getCurrentOrNativeMode(devicePath)
            list.push({
                "devicePath": devicePath,
                "width": defaults.width || 1920,
                "height": defaults.height || 1080,
                "refreshRate": defaults.refreshRate || 60
            })
        } else if (!enabled && idx !== -1) {
            list.splice(idx, 1)
        } else {
            return
        }
        AppConfiguration.gamemodeDisplays = list
    }

    function updateDisplayMode(devicePath, width, height, refreshRate) {
        const list = AppConfiguration.gamemodeDisplays.slice()
        const idx = findDisplayIndex(devicePath)
        if (idx === -1) return
        const entry = Object.assign({}, list[idx])
        if (entry.width === width && entry.height === height && entry.refreshRate === refreshRate) return
        entry.width = width
        entry.height = height
        entry.refreshRate = refreshRate
        list[idx] = entry
        AppConfiguration.gamemodeDisplays = list
    }

    function uniqueResolutions(modes) {
        const seen = {}
        const list = []
        for (let i = 0; i < modes.length; i++) {
            const key = modes[i].width + "x" + modes[i].height
            if (!seen[key]) {
                seen[key] = true
                list.push({ width: modes[i].width, height: modes[i].height, label: modes[i].width + " × " + modes[i].height })
            }
        }
        list.sort(function(a, b) {
            return (b.width * b.height) - (a.width * a.height)
        })
        return list
    }

    function refreshRatesFor(modes, width, height) {
        const rates = {}
        for (let i = 0; i < modes.length; i++) {
            if (modes[i].width === width && modes[i].height === height) {
                rates[modes[i].refreshRate] = true
            }
        }
        const list = Object.keys(rates).map(function(r) { return parseInt(r) })
        list.sort(function(a, b) { return b - a })
        return list
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 3

            Label {
                Layout.fillWidth: true
                Layout.topMargin: 10
                Layout.leftMargin: 3
                text: qsTr("Gamemode Displays")
                font.pixelSize: 18
                font.bold: true
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 3
                Layout.bottomMargin: 5
                text: qsTr("Select one or more displays to activate when entering gamemode. Other displays will be turned off and restored when exiting.")
                font.pixelSize: 13
                opacity: 0.7
                wrapMode: Text.WordWrap
            }

            Card {
                Layout.fillWidth: true
                Layout.preferredWidth: parent.width
                title: "Disable monitor switching"
                additionalControl: Switch {
                    checked: AppConfiguration.disableMonitorSwitch
                    onToggled: {
                        AppConfiguration.disableMonitorSwitch = checked
                    }
                }
            }

            Repeater {
                model: DisplayManager.displays

                ColumnLayout {
                    id: monitorEntry
                    required property var model
                    Layout.fillWidth: true
                    spacing: 3

                    readonly property bool entryEnabled: !AppConfiguration.disableMonitorSwitch &&
                                                        root.isDisplayEnabled(monitorEntry.model.devicePath)

                    property var supportedModes: []
                    property var resolutions: []
                    property var refreshRates: []

                    function reloadModes() {
                        supportedModes = DisplayManager.getSupportedModes(monitorEntry.model.devicePath)
                        resolutions = root.uniqueResolutions(supportedModes)
                        const w = root.getDisplayField(monitorEntry.model.devicePath, "width", 0)
                        const h = root.getDisplayField(monitorEntry.model.devicePath, "height", 0)
                        refreshRates = root.refreshRatesFor(supportedModes, w, h)
                    }

                    Component.onCompleted: reloadModes()

                    Connections {
                        target: AppConfiguration
                        function onGamemodeDisplaysChanged() {
                            monitorEntry.reloadModes()
                        }
                    }

                    Card {
                        Layout.fillWidth: true
                        title: monitorEntry.model.name + (monitorEntry.model.isActive ? qsTr(" (Currently Active)") : "")
                        additionalControl: Switch {
                            enabled: !AppConfiguration.disableMonitorSwitch
                            checked: root.isDisplayEnabled(monitorEntry.model.devicePath)
                            onToggled: {
                                root.setDisplayEnabled(monitorEntry.model.devicePath, checked)
                            }
                        }
                    }

                    Card {
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        show: monitorEntry.entryEnabled
                        visible: monitorEntry.entryEnabled
                        title: qsTr("Primary display")
                        description: qsTr("Position this display at (0,0); others are arranged relative to it")
                        additionalControl: RadioButton {
                            checked: AppConfiguration.gamemodePrimaryDisplay === monitorEntry.model.devicePath
                            onToggled: {
                                if (checked) {
                                    AppConfiguration.gamemodePrimaryDisplay = monitorEntry.model.devicePath
                                }
                            }
                        }
                    }

                    Card {
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        show: monitorEntry.entryEnabled
                        visible: monitorEntry.entryEnabled
                        title: qsTr("Resolution & refresh rate")
                        additionalControl: RowLayout {
                            spacing: 8

                            ComboBox {
                                id: resolutionCombo
                                Layout.preferredWidth: 140
                                textRole: "label"
                                valueRole: "label"
                                model: monitorEntry.resolutions

                                property bool internalUpdate: false

                                function syncFromConfig() {
                                    internalUpdate = true
                                    const w = root.getDisplayField(monitorEntry.model.devicePath, "width", 0)
                                    const h = root.getDisplayField(monitorEntry.model.devicePath, "height", 0)
                                    let idx = -1
                                    for (let i = 0; i < monitorEntry.resolutions.length; i++) {
                                        if (monitorEntry.resolutions[i].width === w &&
                                            monitorEntry.resolutions[i].height === h) {
                                            idx = i
                                            break
                                        }
                                    }
                                    currentIndex = idx
                                    internalUpdate = false
                                }

                                onModelChanged: syncFromConfig()
                                Component.onCompleted: syncFromConfig()

                                onActivated: {
                                    if (internalUpdate || currentIndex < 0) return
                                    const res = monitorEntry.resolutions[currentIndex]
                                    const rates = root.refreshRatesFor(monitorEntry.supportedModes, res.width, res.height)
                                    const currentRate = root.getDisplayField(monitorEntry.model.devicePath, "refreshRate", 60)
                                    const newRate = rates.indexOf(currentRate) !== -1 ? currentRate : (rates[0] || 60)
                                    root.updateDisplayMode(monitorEntry.model.devicePath, res.width, res.height, newRate)
                                }
                            }

                            ComboBox {
                                id: refreshCombo
                                Layout.preferredWidth: 100
                                model: monitorEntry.refreshRates
                                displayText: currentIndex >= 0 ? (monitorEntry.refreshRates[currentIndex] + " Hz") : ""

                                delegate: ItemDelegate {
                                    required property int index
                                    required property var modelData
                                    width: refreshCombo.width
                                    text: modelData + " Hz"
                                    highlighted: refreshCombo.highlightedIndex === index
                                }

                                property bool internalUpdate: false

                                function syncFromConfig() {
                                    internalUpdate = true
                                    const r = root.getDisplayField(monitorEntry.model.devicePath, "refreshRate", 0)
                                    currentIndex = monitorEntry.refreshRates.indexOf(r)
                                    internalUpdate = false
                                }

                                onModelChanged: syncFromConfig()
                                Component.onCompleted: syncFromConfig()

                                onActivated: {
                                    if (internalUpdate || currentIndex < 0) return
                                    const w = root.getDisplayField(monitorEntry.model.devicePath, "width", 0)
                                    const h = root.getDisplayField(monitorEntry.model.devicePath, "height", 0)
                                    root.updateDisplayMode(monitorEntry.model.devicePath, w, h, monitorEntry.refreshRates[currentIndex])
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
