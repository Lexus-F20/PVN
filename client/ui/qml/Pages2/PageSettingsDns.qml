import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Config"
import "../Controls2/TextTypes"
import "../Components"

PageType {
    id: root

    property bool recommendationsExpanded: false

    readonly property var dnsPresets: [
        {
            name: "Cloudflare",
            primary: "1.1.1.1",
            secondary: "1.0.0.1",
            description: qsTr("Fastest, privacy-focused, no logging. Recommended for most users.")
        },
        {
            name: "Google",
            primary: "8.8.8.8",
            secondary: "8.8.4.4",
            description: qsTr("Stable and widely used worldwide. Good if Cloudflare is slow.")
        },
        {
            name: "AdGuard",
            primary: "94.140.14.14",
            secondary: "94.140.15.15",
            description: qsTr("Blocks ads and trackers at DNS level. Good for clean browsing.")
        },
        {
            name: "AdGuard Family",
            primary: "94.140.14.15",
            secondary: "94.140.15.16",
            description: qsTr("Blocks ads, trackers, and adult content. Good for child devices.")
        },
        {
            name: "Quad9",
            primary: "9.9.9.9",
            secondary: "149.112.112.112",
            description: qsTr("Blocks known malicious domains (phishing, malware). Safety-first.")
        },
        {
            name: "Yandex Safe",
            primary: "77.88.8.88",
            secondary: "77.88.8.2",
            description: qsTr("Russia-based, blocks malicious sites. Fast in CIS region.")
        }
    ]

    BackButtonType {
        id: backButton

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 20 + PageController.safeAreaTopMargin

        onFocusChanged: {
            if (this.activeFocus) {
                listView.positionViewAtBeginning()
            }
        }
    }

    ListViewType {
        id: listView

        anchors.top: backButton.bottom
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.left: parent.left

        property var isServerFromApi: ServersUiController.isDefaultServerFromApi

        enabled: !isServerFromApi

        Component.onCompleted: {
            if (isServerFromApi) {
                PageController.showNotificationMessage(qsTr("Default server does not support custom DNS"))
            }
        }

        header: ColumnLayout {
            width: listView.width
            spacing: 16

            BaseHeaderType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                headerText: qsTr("DNS servers")
            }

            ParagraphTextType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                
                text: qsTr("If PVN DNS is not used or installed")
            }
        }

        model: 1 // fake model to force the ListView to be created without a model

        delegate: ColumnLayout {
            width: listView.width
            spacing: 16

            TextFieldWithHeaderType {
                id: primaryDns

                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                headerText: qsTr("Primary DNS")

                textField.text: SettingsController.primaryDns
                textField.validator: RegularExpressionValidator {
                    regularExpression: InstallController.ipAddressRegExp()
                }
            }

            TextFieldWithHeaderType {
                id: secondaryDns

                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                headerText: qsTr("Secondary DNS")

                textField.text: SettingsController.secondaryDns
                textField.validator: RegularExpressionValidator {
                    regularExpression: InstallController.ipAddressRegExp()
                }
            }

            BasicButtonType {
                id: recommendationsButton

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                defaultColor: PvnStyle.color.transparent
                hoveredColor: PvnStyle.color.translucentWhite
                pressedColor: PvnStyle.color.sheerWhite
                textColor: PvnStyle.color.paleGray
                borderWidth: 1

                text: root.recommendationsExpanded
                      ? qsTr("Hide recommendations")
                      : qsTr("Recommended DNS servers")

                clickedFunc: function() {
                    root.recommendationsExpanded = !root.recommendationsExpanded
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                spacing: 8

                visible: root.recommendationsExpanded

                Repeater {
                    model: root.dnsPresets

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        radius: 12
                        color: presetMouse.containsMouse
                               ? PvnStyle.color.translucentWhite
                               : PvnStyle.color.onyxBlack
                        border.color: PvnStyle.color.slateGray
                        border.width: 1
                        implicitHeight: presetContent.implicitHeight + 24

                        ColumnLayout {
                            id: presetContent
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Header2TextType {
                                    text: modelData.name
                                    color: PvnStyle.color.paleGray
                                }

                                Item { Layout.fillWidth: true }

                                CaptionTextType {
                                    text: modelData.primary + " / " + modelData.secondary
                                    color: PvnStyle.color.mutedGray
                                }
                            }

                            CaptionTextType {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: PvnStyle.color.mutedGray
                                text: modelData.description
                            }
                        }

                        MouseArea {
                            id: presetMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                primaryDns.textField.text = modelData.primary
                                secondaryDns.textField.text = modelData.secondary
                                PageController.showNotificationMessage(
                                    qsTr("%1 DNS applied. Press Save to keep it.").arg(modelData.name))
                            }
                        }
                    }
                }
            }

            BasicButtonType {
                id: restoreDefaultButton

                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                defaultColor: PvnStyle.color.transparent
                hoveredColor: PvnStyle.color.translucentWhite
                pressedColor: PvnStyle.color.sheerWhite
                disabledColor: PvnStyle.color.mutedGray
                textColor: PvnStyle.color.paleGray
                borderWidth: 1

                text: qsTr("Restore default")

                clickedFunc: function() {
                    var headerText = qsTr("Restore default DNS settings?")
                    var yesButtonText = qsTr("Continue")
                    var noButtonText = qsTr("Cancel")

                    var yesButtonFunction = function() {
                        SettingsController.primaryDns = "1.1.1.1"
                        primaryDns.textField.text = SettingsController.primaryDns
                        SettingsController.secondaryDns = "1.0.0.1"
                        secondaryDns.textField.text = SettingsController.secondaryDns
                        PageController.showNotificationMessage(qsTr("Settings have been reset"))
                    }
                    var noButtonFunction = function() {
                    }

                    showQuestionDrawer(headerText, "", yesButtonText, noButtonText, yesButtonFunction, noButtonFunction)
                }
            }

            BasicButtonType {
                id: saveButton

                Layout.fillWidth: true
                Layout.margins: 16

                text: qsTr("Save")

                clickedFunc: function() {
                    if (primaryDns.textField.text === "") {
                        primaryDns.errorText = qsTr("Primary DNS cannot be empty")
                        return
                    }
                    primaryDns.errorText = ""
                    secondaryDns.errorText = ""

                    if (primaryDns.textField.text !== SettingsController.primaryDns) {
                        SettingsController.primaryDns = primaryDns.textField.text
                    }
                    if (secondaryDns.textField.text !== SettingsController.secondaryDns) {
                        SettingsController.secondaryDns = secondaryDns.textField.text
                    }
                    PageController.showNotificationMessage(qsTr("Settings saved"))
                }
            }
        }
    }
}

