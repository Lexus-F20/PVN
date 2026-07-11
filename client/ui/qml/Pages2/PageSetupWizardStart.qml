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
    enableTimer: (SettingsController.isOnTv()) ? false : true

    // ui mode: "login" | "register"
    property string authMode: "login"

    readonly property bool authBusy: AuthController.state === 1
                                  || AuthController.state === 2
                                  || AuthController.state === 3
                                  || AuthController.state === 4

    Flickable {
        id: scroll
        anchors.fill: parent
        anchors.topMargin: PageController.safeAreaTopMargin
        anchors.bottomMargin: PageController.safeAreaBottomMargin
        contentWidth: parent.width
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: content
            width: scroll.width
            spacing: 12

            Image {
                id: image
                source: "qrc:/images/amneziaBigLogo.png"
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 16
                Layout.preferredWidth: 200
                Layout.preferredHeight: 160
                fillMode: Image.PreserveAspectFit
            }

            // ---- Title -------------------------------------------------------
            Header1TextType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                horizontalAlignment: Text.AlignHCenter
                text: authMode === "login" ? qsTr("Welcome back") : qsTr("Create account")
            }

            ParagraphTextType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                horizontalAlignment: Text.AlignHCenter
                color: PvnStyle.color.mutedGray
                text: authMode === "login"
                      ? qsTr("Sign in to your PVN account")
                      : qsTr("Sign up for a new PVN account")
            }

            // ---- Name (register only) ---------------------------------------
            TextFieldWithHeaderType {
                id: nameField
                visible: authMode === "register"
                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                headerText: qsTr("Name (optional)")
                textField.text: ""
                textField.maximumLength: 40
                textFieldEditable: !authBusy
            }

            // ---- Email -------------------------------------------------------
            TextFieldWithHeaderType {
                id: emailField
                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                headerText: qsTr("Email")
                textField.text: ""
                textField.placeholderText: "you@example.com"
                textField.inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                textField.maximumLength: 100
                textFieldEditable: !authBusy
            }

            // ---- Password ----------------------------------------------------
            TextFieldWithHeaderType {
                id: passwordField
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                headerText: qsTr("Password")
                textField.text: ""
                textField.echoMode: TextInput.Password
                textField.maximumLength: 100
                textFieldEditable: !authBusy
                buttonImageSource: "qrc:/images/controls/eye.svg"
                clickedFunc: function() {
                    textField.echoMode = textField.echoMode === TextInput.Password
                                       ? TextInput.Normal : TextInput.Password
                }
            }

            // ---- Error -------------------------------------------------------
            CaptionTextType {
                id: errorLabel
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                color: PvnStyle.color.vibrantRed
                visible: text.length > 0
                wrapMode: Text.WordWrap
                text: ""
            }

            // ---- Primary action ---------------------------------------------
            BasicButtonType {
                id: primaryButton
                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                enabled: !authBusy
                text: authBusy
                      ? qsTr("Please wait…")
                      : (authMode === "login" ? qsTr("Sign in") : qsTr("Create account"))
                clickedFunc: function() {
                    errorLabel.text = ""
                    var email = emailField.textField.text.trim()
                    var password = passwordField.textField.text
                    if (email.length < 3 || email.indexOf("@") < 1) {
                        errorLabel.text = qsTr("Enter a valid email")
                        return
                    }
                    if (password.length < 8) {
                        errorLabel.text = qsTr("Password must be at least 8 characters")
                        return
                    }
                    if (authMode === "login") {
                        AuthController.signInWithEmail(email, password)
                    } else {
                        AuthController.registerWithEmail(email, password, nameField.textField.text.trim())
                    }
                }
            }

            // ---- Toggle login/register --------------------------------------
            Row {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4
                spacing: 6
                ParagraphTextType {
                    color: PvnStyle.color.mutedGray
                    text: authMode === "login"
                          ? qsTr("No account?")
                          : qsTr("Already have an account?")
                }
                ParagraphTextType {
                    color: PvnStyle.color.paleGray
                    text: authMode === "login" ? qsTr("Sign up") : qsTr("Sign in")
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            errorLabel.text = ""
                            authMode = authMode === "login" ? "register" : "login"
                        }
                    }
                }
            }

            // ---- "OR" divider ------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 8
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: PvnStyle.color.slateGray
                }
                CaptionTextType {
                    text: qsTr("OR")
                    color: PvnStyle.color.mutedGray
                }
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: PvnStyle.color.slateGray
                }
            }

            // ---- Google ------------------------------------------------------
            BasicButtonType {
                id: googleSignInButton
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                enabled: !authBusy
                text: authBusy ? qsTr("Signing in…") : qsTr("Continue with Google")
                clickedFunc: function() {
                    errorLabel.text = ""
                    AuthController.signInWithGoogle()
                }
            }

            // ---- I have a config file ---------------------------------------
            BasicButtonType {
                id: startButton
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.bottomMargin: 24
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                defaultColor: PvnStyle.color.transparent
                hoveredColor: PvnStyle.color.translucentWhite
                pressedColor: PvnStyle.color.sheerWhite
                disabledColor: PvnStyle.color.mutedGray
                textColor: PvnStyle.color.paleGray
                borderColor: PvnStyle.color.slateGray
                borderWidth: 1
                text: qsTr("I have a config file")
                clickedFunc: function() {
                    PageController.goToPage(PageEnum.PageSetupWizardConfigSource)
                }
            }
        }
    }

    Connections {
        target: AuthController
        function onSignInCompleted() {
            PageController.goToPageHome()
        }
        function onSignInFailed(msg) {
            errorLabel.text = msg
        }
    }

    Timer {
        interval: 250
        running: SettingsController.isOnTv()
        repeat: true
        onTriggered: {
            startButton.forceActiveFocus()
            if (startButton.activeFocus) {
                running = false
            }
        }
    }

    onVisibleChanged: {
        if (visible && SettingsController.isOnTv()) {
            startButton.forceActiveFocus()
        }
    }
}
