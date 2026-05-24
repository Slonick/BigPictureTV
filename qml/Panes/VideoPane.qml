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
            list.push({
                "devicePath": devicePath,
                "width": 3840,
                "height": 2160,
                "refreshRate": 60
            })
        } else if (!enabled && idx !== -1) {
            list.splice(idx, 1)
        } else {
            return
        }
        AppConfiguration.gamemodeDisplays = list
    }

    function updateDisplayField(devicePath, field, value) {
        const list = AppConfiguration.gamemodeDisplays.slice()
        const idx = findDisplayIndex(devicePath)
        if (idx === -1) return
        const entry = Object.assign({}, list[idx])
        if (entry[field] === value) return
        entry[field] = value
        list[idx] = entry
        AppConfiguration.gamemodeDisplays = list
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

                    readonly property bool enabled: !AppConfiguration.disableMonitorSwitch &&
                                                    root.isDisplayEnabled(monitorEntry.model.devicePath)

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
                        show: monitorEntry.enabled
                        visible: monitorEntry.enabled
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
                        show: monitorEntry.enabled
                        visible: monitorEntry.enabled
                        title: qsTr("Width")
                        additionalControl: SpinBox {
                            from: 640
                            to: 7680
                            stepSize: 1
                            editable: true
                            value: root.getDisplayField(monitorEntry.model.devicePath, "width", 3840)
                            onValueModified: {
                                root.updateDisplayField(monitorEntry.model.devicePath, "width", value)
                            }
                        }
                    }

                    Card {
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        show: monitorEntry.enabled
                        visible: monitorEntry.enabled
                        title: qsTr("Height")
                        additionalControl: SpinBox {
                            from: 480
                            to: 4320
                            stepSize: 1
                            editable: true
                            value: root.getDisplayField(monitorEntry.model.devicePath, "height", 2160)
                            onValueModified: {
                                root.updateDisplayField(monitorEntry.model.devicePath, "height", value)
                            }
                        }
                    }

                    Card {
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        show: monitorEntry.enabled
                        visible: monitorEntry.enabled
                        title: qsTr("Refresh Rate (Hz)")
                        additionalControl: SpinBox {
                            from: 24
                            to: 360
                            stepSize: 1
                            editable: true
                            value: root.getDisplayField(monitorEntry.model.devicePath, "refreshRate", 60)
                            onValueModified: {
                                root.updateDisplayField(monitorEntry.model.devicePath, "refreshRate", value)
                            }
                        }
                    }
                }
            }
        }
    }
}
