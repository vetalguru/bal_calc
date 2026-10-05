pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import BalCalc

// Rifle + ammunition profiles: list, choose, create, edit, delete, share;
// entry point to the bullet library.
Page {
    id: page

    // Sized by the parent layout; a fixed implicit size avoids a binding
    // loop through the scrolled content's width.
    implicitWidth: 360
    implicitHeight: 640

    signal profileChosen()

    // Pops an inner page (editor, library, log); false when at the list.
    function back() {
        if (stack.depth > 1) {
            stack.pop()
            return true
        }
        return false
    }

    function edit(id) {
        stack.push(editorComponent, { form: Backend.profileForm(id), stack: stack })
    }

    function notify(error, success) {
        var text = error.length > 0 ? error : success
        if (text.length === 0)
            return
        toast.text = text
        toast.error = error.length > 0
        toast.open()
    }

    StackView {
        id: stack
        anchors.fill: parent

        initialItem: Page {
            header: ToolBar {
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 8
                    Label {
                        text: qsTr("Profiles")
                        font.pixelSize: 18
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Button {
                        text: qsTr("Bullets")
                        flat: true
                        onClicked: stack.push(libraryComponent, { stack: stack })
                    }
                    Button {
                        id: importButton
                        text: qsTr("Import")
                        flat: true
                        onClicked: importMenu.open()
                        Menu {
                            id: importMenu
                            y: importButton.height
                            MenuItem {
                                text: qsTr("From file…")
                                onTriggered: importDialog.open()
                            }
                            MenuItem {
                                text: qsTr("From clipboard")
                                onTriggered: page.notify(Backend.importProfileFromClipboard(),
                                                         qsTr("Profile imported."))
                            }
                        }
                    }
                    Button {
                        text: qsTr("New")
                        highlighted: true
                        onClicked: page.edit(0)
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: Backend.profiles.length === 0
                width: parent.width - 48
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("No profiles yet. A profile is a rifle, its scope and the cartridge it shoots.")
                opacity: 0.7
            }

            ListView {
                anchors.fill: parent
                model: Backend.profiles
                delegate: ItemDelegate {
                    id: row
                    required property var modelData
                    width: ListView.view.width
                    highlighted: row.modelData.id === Backend.currentProfileId
                    contentItem: RowLayout {
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                text: row.modelData.name
                                font.pixelSize: 16
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: qsTr("Zero %1 m").arg(Math.round(row.modelData.zeroRangeM))
                                opacity: 0.6
                            }
                        }
                        Button {
                            text: qsTr("Edit")
                            flat: true
                            onClicked: page.edit(row.modelData.id)
                        }
                        Button {
                            id: moreButton
                            text: "⋮"
                            flat: true
                            onClicked: rowMenu.open()
                            Menu {
                                id: rowMenu
                                y: moreButton.height
                                MenuItem {
                                    text: qsTr("Shot log and truing")
                                    onTriggered: {
                                        Backend.currentProfileId = row.modelData.id
                                        stack.push(truingComponent)
                                    }
                                }
                                MenuItem {
                                    text: qsTr("Export to file…")
                                    onTriggered: {
                                        exportDialog.profileId = row.modelData.id
                                        exportDialog.selectedFile = Backend.profileFileName(row.modelData.id)
                                        exportDialog.open()
                                    }
                                }
                                MenuItem {
                                    text: qsTr("Copy to clipboard")
                                    onTriggered: page.notify(Backend.copyProfileToClipboard(row.modelData.id),
                                                             qsTr("Profile copied. Paste it into a message or a file."))
                                }
                                MenuItem {
                                    text: qsTr("Delete")
                                    onTriggered: {
                                        confirmDelete.profileId = row.modelData.id
                                        confirmDelete.profileName = row.modelData.name
                                        confirmDelete.open()
                                    }
                                }
                            }
                        }
                    }
                    onClicked: {
                        Backend.currentProfileId = row.modelData.id
                        page.profileChosen()
                    }
                }
            }
        }
    }

    Component {
        id: editorComponent
        ProfileEditor {
            onDone: stack.pop()
        }
    }

    Component {
        id: truingComponent
        TruingPage {
            onClosed: stack.pop()
        }
    }

    Component {
        id: libraryComponent
        LibraryPage {
            onClosed: stack.pop()
        }
    }

    FileDialog {
        id: importDialog
        title: qsTr("Import profile")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("BalCalc profiles (*.json)"), qsTr("All files (*)")]
        onAccepted: page.notify(Backend.importProfile(selectedFile), qsTr("Profile imported."))
    }

    FileDialog {
        id: exportDialog
        property int profileId: 0
        title: qsTr("Export profile")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("BalCalc profiles (*.json)")]
        onAccepted: page.notify(Backend.exportProfile(profileId, selectedFile), qsTr("Profile saved."))
    }

    Dialog {
        id: confirmDelete
        property int profileId: 0
        property string profileName
        anchors.centerIn: parent
        modal: true
        title: qsTr("Delete profile?")
        standardButtons: Dialog.Yes | Dialog.No
        Label { text: confirmDelete.profileName }
        onAccepted: page.notify(Backend.deleteProfile(confirmDelete.profileId), "")
    }

    Popup {
        id: toast
        property string text
        property bool error: false
        x: (page.width - width) / 2
        y: page.height - height - 24
        width: Math.min(page.width - 32, 480)
        padding: 12
        closePolicy: Popup.CloseOnPressOutside
        onOpened: hideTimer.restart()
        Timer {
            id: hideTimer
            interval: 3500
            onTriggered: toast.close()
        }
        background: Rectangle {
            radius: 6
            color: toast.error ? Material.color(Material.Red, Material.Shade700) : "#323232"
        }
        contentItem: Label {
            text: toast.text
            color: "white"
            wrapMode: Text.Wrap
        }
    }
}
