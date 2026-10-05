import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BalCalc

ApplicationWindow {
    width: 420
    height: 720
    visible: true
    title: qsTr("BalCalc")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        Label {
            text: qsTr("Ballistic calculator")
            font.pixelSize: 24
            font.bold: true
        }
        Label { text: qsTr("Engine: %1").arg(AppInfo.engineVersion) }
        Label { text: qsTr("SQLite: %1").arg(AppInfo.sqliteVersion) }
        Label { text: qsTr("Database: %1").arg(AppInfo.databaseStatus) }
        Label {
            Layout.fillWidth: true
            text: AppInfo.databasePath
            wrapMode: Text.WrapAnywhere
            opacity: 0.6
        }
        Item { Layout.fillHeight: true }
    }
}
