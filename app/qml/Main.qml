pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BalCalc

ApplicationWindow {
    id: win

    width: 1100
    height: 760
    minimumWidth: 360
    minimumHeight: 560
    visible: true
    title: qsTr("BalCalc")

    Material.theme: Material.System
    Material.accent: Material.Orange
    Material.primary: Material.BlueGrey

    // Desktop and tablets get a side rail, phones a bottom tab bar.
    readonly property bool wide: width >= 840
    property int page: 0
    property alias tableTab: tablePage.tab

    readonly property var pages: [
        { title: qsTr("Solution"), short: qsTr("Solve") },
        { title: qsTr("Range table"), short: qsTr("Table") },
        { title: qsTr("Conditions"), short: qsTr("Air") },
        { title: qsTr("Profiles"), short: qsTr("Rifles") },
        { title: qsTr("Settings"), short: qsTr("More") }
    ]

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Pane {
            visible: win.wide
            Layout.fillHeight: true
            Layout.preferredWidth: 200
            padding: 0
            Material.elevation: 2

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Label {
                    text: qsTr("BalCalc")
                    font.pixelSize: 22
                    font.bold: true
                    Layout.margins: 16
                }

                Repeater {
                    model: win.pages
                    delegate: ItemDelegate {
                        id: navItem
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        text: navItem.modelData.title
                        highlighted: win.page === navItem.index
                        onClicked: win.page = navItem.index
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: win.page

            SolutionPage {
                onEditProfiles: win.page = 3
            }
            TablePage {
                id: tablePage
                onRangeChosen: win.page = 0
            }
            ConditionsPage {}
            ProfilesPage {
                onProfileChosen: win.page = 0
            }
            SettingsPage {}
        }
    }

    footer: TabBar {
        visible: !win.wide
        height: visible ? implicitHeight : 0
        currentIndex: win.page
        onCurrentIndexChanged: win.page = currentIndex

        Repeater {
            model: win.pages
            delegate: TabButton {
                id: tab
                required property var modelData
                text: tab.modelData.short
            }
        }
    }

    Dialog {
        id: dbErrorDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Database error")
        standardButtons: Dialog.Ok
        Label {
            text: Backend.databaseError + "\n" + Backend.databasePath
            wrapMode: Text.Wrap
            width: Math.min(win.width - 80, 480)
        }
        Component.onCompleted: if (Backend.databaseError.length > 0) open()
    }
}
