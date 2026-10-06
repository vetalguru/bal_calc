pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BalCalc

// Factory loads (starter library, imported .ammo files): pick one to copy
// into the user's cartridges.
Page {
    id: page

    implicitWidth: 360
    implicitHeight: 640

    signal picked(int cartridgeId)
    signal closed()

    property var items: []
    function reload() { items = Backend.libraryCartridges(search.text) }
    Component.onCompleted: reload()

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
                text: qsTr("Factory cartridges")
                font.pixelSize: 18
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TextField {
            id: search
            Layout.fillWidth: true
            Layout.margins: 12
            placeholderText: qsTr("Search by name or caliber")
            onTextChanged: page.reload()
        }

        Label {
            visible: page.items.length === 0
            Layout.fillWidth: true
            Layout.margins: 24
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            opacity: 0.7
            text: search.text.length > 0 ? qsTr("Nothing found.")
                                         : qsTr("No factory cartridges. Import .ammo files in the bullet library.")
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: page.items
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: row
                required property var modelData
                width: ListView.view.width
                contentItem: ColumnLayout {
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
                        text: [row.modelData.caliber, row.modelData.bulletName,
                               qsTr("%1 m/s").arg(Math.round(row.modelData.muzzleVelocity))]
                              .filter(s => s.length > 0).join(" · ")
                    }
                }
                onClicked: page.picked(row.modelData.id)
            }
        }
    }
}
