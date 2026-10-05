pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BalCalc

// Rifle + ammunition profiles: list, choose, create, edit, delete.
Page {
    id: page

    // Sized by the parent layout; a fixed implicit size avoids a binding
    // loop through the scrolled content's width.
    implicitWidth: 360
    implicitHeight: 640

    signal profileChosen()

    function edit(id) {
        stack.push(editorComponent, { form: Backend.profileForm(id) })
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
                        Layout.fillWidth: true
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
                            text: qsTr("Delete")
                            flat: true
                            onClicked: {
                                confirmDelete.profileId = row.modelData.id
                                confirmDelete.profileName = row.modelData.name
                                confirmDelete.open()
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

    Dialog {
        id: confirmDelete
        property int profileId: 0
        property string profileName
        anchors.centerIn: parent
        modal: true
        title: qsTr("Delete profile?")
        standardButtons: Dialog.Yes | Dialog.No
        Label { text: confirmDelete.profileName }
        onAccepted: {
            var err = Backend.deleteProfile(confirmDelete.profileId)
            if (err.length > 0)
                console.warn(err)
        }
    }
}
