import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// A titled card of form fields (rifle and cartridge editors).
Pane {
    id: section
    property string title
    default property alias content: box.data
    Layout.fillWidth: true
    Layout.leftMargin: 12
    Layout.rightMargin: 12
    Material.elevation: 1
    ColumnLayout {
        id: box
        anchors.fill: parent
        spacing: 8
        Label {
            text: section.title
            font.pixelSize: 16
            font.bold: true
            color: Material.accent
        }
    }
}
