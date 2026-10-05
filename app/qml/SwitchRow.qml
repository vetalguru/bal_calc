import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A switch whose label wraps instead of widening the layout.
RowLayout {
    id: root

    property alias checked: toggle.checked
    property alias text: label.text

    signal toggled()

    Layout.fillWidth: true
    spacing: 4

    Switch {
        id: toggle
        onToggled: root.toggled()
    }
    Label {
        id: label
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        MouseArea {
            anchors.fill: parent
            onClicked: {
                toggle.toggle()
                root.toggled()
            }
        }
    }
}
