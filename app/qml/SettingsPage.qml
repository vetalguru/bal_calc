import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BalCalc

Page {
    id: page

    // Sized by the parent layout; a fixed implicit size avoids a binding
    // loop through the scrolled content's width.
    implicitWidth: 360
    implicitHeight: 640

    header: ToolBar {
        Label {
            anchors.centerIn: parent
            text: qsTr("Settings")
            font.pixelSize: 18
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 12

            Pane {
                Layout.fillWidth: true
                Layout.margins: 12
                Material.elevation: 1
                ColumnLayout {
                    anchors.fill: parent
                    Label {
                        text: qsTr("Angle units")
                        font.bold: true
                        color: Material.accent
                    }
                    RadioButton {
                        text: qsTr("MRAD (milliradians)")
                        checked: Backend.angleUnit === "mrad"
                        onToggled: if (checked) Backend.angleUnit = "mrad"
                    }
                    RadioButton {
                        text: qsTr("MOA (minutes of angle)")
                        checked: Backend.angleUnit === "moa"
                        onToggled: if (checked) Backend.angleUnit = "moa"
                    }

                    Label {
                        text: qsTr("Language")
                        font.bold: true
                        color: Material.accent
                        Layout.topMargin: 12
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        model: [
                            { value: "", text: qsTr("System") },
                            { value: "uk", text: "Українська" },
                            { value: "ru", text: "Русский" },
                            { value: "en", text: "English" }
                        ]
                        textRole: "text"
                        valueRole: "value"
                        Component.onCompleted: currentIndex = Math.max(0, indexOfValue(Backend.language))
                        onActivated: Backend.language = currentValue
                    }
                }
            }

            Pane {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Material.elevation: 1
                ColumnLayout {
                    anchors.fill: parent
                    Label {
                        text: qsTr("About")
                        font.bold: true
                        color: Material.accent
                    }
                    Label { text: qsTr("Engine %1, SQLite %2").arg(Backend.engineVersion).arg(Backend.sqliteVersion) }
                    Label {
                        text: qsTr("Point-mass trajectory model with Coriolis, spin drift and aerodynamic jump. Always confirm with live fire.")
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        opacity: 0.8
                    }
                    Label {
                        text: Backend.databasePath
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                        opacity: 0.5
                        font.pixelSize: 11
                    }
                }
            }
        }
    }
}
