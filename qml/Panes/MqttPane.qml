import QtQuick
import QtQuick.Controls.FluentWinUI3
import QtQuick.Layouts
import Odizinne.BigPictureTV

Pane {
    ScrollView {
        anchors.fill: parent

        ColumnLayout {
            width: parent.width
            spacing: 3

            Card {
                Layout.fillWidth: true
                title: "Enable Home Assistant (MQTT)"
                description: "Publish Big Picture / desktop state so HA can switch your TV"
                additionalControl: Switch {
                    checked: AppConfiguration.mqttEnabled
                    onClicked: AppConfiguration.mqttEnabled = checked
                }
            }

            Card {
                Layout.fillWidth: true
                enabled: AppConfiguration.mqttEnabled
                title: "Broker address"
                description: "Hostname or IP of your MQTT broker"
                additionalControl: TextField {
                    placeholderText: qsTr("homeassistant.local")
                    text: AppConfiguration.mqttHost
                    onTextChanged: AppConfiguration.mqttHost = text
                }
            }

            Card {
                Layout.fillWidth: true
                enabled: AppConfiguration.mqttEnabled
                title: "Broker port"
                additionalControl: SpinBox {
                    from: 1
                    to: 65535
                    editable: true
                    value: AppConfiguration.mqttPort
                    onValueModified: AppConfiguration.mqttPort = value
                }
            }

            Card {
                Layout.fillWidth: true
                enabled: AppConfiguration.mqttEnabled
                title: "Username"
                description: "Leave empty for an anonymous broker"
                additionalControl: TextField {
                    placeholderText: qsTr("optional")
                    text: AppConfiguration.mqttUsername
                    onTextChanged: AppConfiguration.mqttUsername = text
                }
            }

            Card {
                Layout.fillWidth: true
                enabled: AppConfiguration.mqttEnabled
                title: "Password"
                additionalControl: TextField {
                    placeholderText: qsTr("optional")
                    echoMode: TextInput.Password
                    text: AppConfiguration.mqttPassword
                    onTextChanged: AppConfiguration.mqttPassword = text
                }
            }

            Card {
                Layout.fillWidth: true
                enabled: AppConfiguration.mqttEnabled
                title: "Entity name"
                description: "Name shown for the sensor in Home Assistant"
                additionalControl: TextField {
                    placeholderText: qsTr("Big Picture Mode")
                    text: AppConfiguration.mqttEntityName
                    onTextChanged: AppConfiguration.mqttEntityName = text
                }
            }

            Card {
                Layout.fillWidth: true
                enabled: AppConfiguration.mqttEnabled
                title: "Base topic"
                description: "State is published to <base>/state"
                additionalControl: TextField {
                    placeholderText: qsTr("bigpicturetv")
                    text: AppConfiguration.mqttBaseTopic
                    onTextChanged: AppConfiguration.mqttBaseTopic = text
                }
            }

            Card {
                Layout.fillWidth: true
                enabled: AppConfiguration.mqttEnabled
                title: "Discovery prefix"
                description: "Must match HA's MQTT discovery prefix (default homeassistant)"
                additionalControl: TextField {
                    placeholderText: qsTr("homeassistant")
                    text: AppConfiguration.mqttDiscoveryPrefix
                    onTextChanged: AppConfiguration.mqttDiscoveryPrefix = text
                }
            }

            Card {
                Layout.fillWidth: true
                enabled: AppConfiguration.mqttEnabled
                title: "Connection"
                description: MqttClient.statusText
                additionalControl: Button {
                    text: qsTr("Test")
                    onClicked: MqttClient.testConnection()
                }
            }

            Card {
                Layout.fillWidth: true
                enabled: AppConfiguration.mqttEnabled && MqttClient.connected
                title: "Simulate state"
                description: "Publish a state without launching Big Picture"
                additionalControl: RowLayout {
                    spacing: 6
                    Button {
                        text: qsTr("On")
                        onClicked: MqttClient.simulateState(true)
                    }
                    Button {
                        text: qsTr("Off")
                        onClicked: MqttClient.simulateState(false)
                    }
                }
            }
        }
    }
}
