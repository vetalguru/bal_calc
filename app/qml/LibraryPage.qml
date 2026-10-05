pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import BalCalc

// Bullet library: search, add, edit, delete; or pick one for a profile.
Page {
    id: page

    implicitWidth: 360
    implicitHeight: 640

    // Picker mode: tapping a bullet emits picked(id) instead of editing it.
    property bool picker: false
    property StackView stack
    signal picked(int bulletId)
    signal closed()

    property var bullets: []
    function reload() { bullets = Backend.libraryBullets(search.text) }
    Component.onCompleted: reload()
    Connections {
        target: Backend
        function onLibraryChanged() { page.reload() }
    }

    function edit(id) {
        page.stack.push(editorComponent, { form: Backend.bulletForm(id) })
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            Button {
                text: qsTr("Back")
                flat: true
                onClicked: page.closed()
            }
            Label {
                text: page.picker ? qsTr("Choose a bullet") : qsTr("Bullet library")
                font.pixelSize: 18
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Import")
                flat: true
                onClicked: importDialog.open()
            }
            Button {
                text: qsTr("New")
                highlighted: true
                onClicked: page.edit(0)
            }
        }
    }

    FileDialog {
        id: importDialog
        title: qsTr("Import bullets, drag curves or reticles")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Ballistic data (*.ammo *.drg *.reticle *.json)"), qsTr("All files (*)")]
        onAccepted: {
            importResult.text = Backend.importFiles(selectedFiles)
            importResult.open()
        }
    }

    Dialog {
        id: importResult
        property alias text: resultLabel.text
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 520)
        modal: true
        title: qsTr("Import")
        standardButtons: Dialog.Ok
        Label {
            id: resultLabel
            width: parent.width
            wrapMode: Text.Wrap
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TextField {
            id: search
            Layout.fillWidth: true
            Layout.margins: 12
            placeholderText: qsTr("Search by name, maker or caliber")
            onTextChanged: page.reload()
        }

        Label {
            visible: page.bullets.length === 0
            Layout.fillWidth: true
            Layout.margins: 24
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            opacity: 0.7
            text: search.text.length > 0 ? qsTr("Nothing found.")
                                         : qsTr("The library is empty. Add bullets here to reuse them in several profiles.")
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: page.bullets
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: row
                required property var modelData
                width: ListView.view.width
                contentItem: RowLayout {
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.name
                            font.pixelSize: 16
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            opacity: 0.65
                            text: [row.modelData.manufacturer, row.modelData.caliber,
                                   qsTr("%1 gr").arg(Number(row.modelData.massGr).toFixed(1)),
                                   row.modelData.dragKind === "curve" ? qsTr("own drag curve")
                                   : row.modelData.dragTable + " " + Number(row.modelData.bc).toFixed(3)
                                     + (row.modelData.bcBands > 1 ? " " + qsTr("(%1 bands)").arg(row.modelData.bcBands) : "")]
                                  .filter(s => s.length > 0).join("  ·  ")
                        }
                    }
                    Button {
                        visible: page.picker
                        text: qsTr("Edit")
                        flat: true
                        onClicked: page.edit(row.modelData.id)
                    }
                }
                onClicked: page.picker ? page.picked(row.modelData.id) : page.edit(row.modelData.id)
            }
        }
    }

    Component {
        id: editorComponent
        BulletEditor {
            onDone: page.stack.pop()
        }
    }
}
