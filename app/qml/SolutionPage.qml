pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BalCalc

// Firing solution for one target: distance in, dial/hold corrections out,
// large enough to read at arm's length.
Page {
    id: page

    // Sized by the parent layout; a fixed implicit size avoids a binding
    // loop through the scrolled content's width.
    implicitWidth: 360
    implicitHeight: 640

    signal editProfiles()

    readonly property var sol: Backend.solution
    readonly property string unitLabel: Backend.angleUnit === "moa" ? qsTr("MOA") : qsTr("MRAD")

    function fmt(v, d) { return Number(v).toFixed(d) }

    function clockLabel(deg) {
        var h = Math.round(((deg % 360) + 360) % 360 / 30)
        if (h === 0)
            h = 12
        return qsTr("%1 o'clock").arg(h)
    }

    function setRange(r) {
        Backend.targetRangeM = Math.max(10, Math.min(3000, Math.round(r)))
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12

            Label {
                text: qsTr("Profile")
                opacity: 0.8
            }
            ComboBox {
                id: profileBox
                Layout.fillWidth: true
                model: Backend.profiles
                textRole: "name"
                valueRole: "id"
                enabled: count > 0
                displayText: count > 0 ? currentText : qsTr("No profiles")
                Component.onCompleted: currentIndex = indexOfValue(Backend.currentProfileId)
                Connections {
                    target: Backend
                    function onCurrentProfileIdChanged() {
                        profileBox.currentIndex = profileBox.indexOfValue(Backend.currentProfileId)
                    }
                    function onProfilesChanged() {
                        profileBox.currentIndex = profileBox.indexOfValue(Backend.currentProfileId)
                    }
                }
                onActivated: Backend.currentProfileId = currentValue
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 16

            Item { Layout.preferredHeight: 4 }

            // --- Distance -------------------------------------------------
            Pane {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Material.elevation: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    Label {
                        text: qsTr("Distance")
                        opacity: 0.7
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        TextField {
                            id: rangeField
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 34
                            font.bold: true
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: IntValidator { bottom: 0; top: 3000 }
                            text: Math.round(Backend.targetRangeM)
                            onEditingFinished: {
                                var v = parseInt(text)
                                text = Qt.binding(function() { return Math.round(Backend.targetRangeM) })
                                if (!isNaN(v))
                                    page.setRange(v)
                            }
                        }
                        Label { text: qsTr("m"); font.pixelSize: 20 }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Repeater {
                            model: [-100, -10, 10, 100]
                            delegate: Button {
                                id: stepButton
                                required property int modelData
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                flat: true
                                text: (modelData > 0 ? "+" : "−") + Math.abs(modelData)
                                onClicked: page.setRange(Backend.targetRangeM + stepButton.modelData)
                            }
                        }
                    }
                    Slider {
                        Layout.fillWidth: true
                        from: 50
                        to: 2500
                        stepSize: 5
                        value: Backend.targetRangeM
                        onMoved: page.setRange(value)
                    }
                }
            }

            // --- Corrections ---------------------------------------------
            Label {
                visible: !page.sol.ok
                Layout.fillWidth: true
                Layout.margins: 12
                text: page.sol.error || ""
                wrapMode: Text.Wrap
                color: Material.color(Material.Red)
                font.pixelSize: 16
            }
            Button {
                visible: Backend.profiles.length === 0
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Create a profile")
                highlighted: true
                onClicked: page.editProfiles()
            }

            GridLayout {
                visible: page.sol.ok === true
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                columns: page.width >= 600 ? 2 : 1
                columnSpacing: 12
                rowSpacing: 12

                CorrectionTile {
                    Layout.fillWidth: true
                    title: qsTr("Elevation")
                    direction: page.sol.elevation >= 0 ? qsTr("UP") : qsTr("DOWN")
                    value: page.fmt(Math.abs(page.sol.elevation || 0), 2)
                    unit: page.unitLabel
                    clicks: page.sol.hasScope
                            ? qsTr("%1 clicks").arg(Math.abs(page.sol.elevationClicks || 0)) : ""
                }
                CorrectionTile {
                    Layout.fillWidth: true
                    title: qsTr("Windage")
                    direction: Math.abs(page.sol.windage || 0) < 0.005 ? ""
                               : (page.sol.windage > 0 ? qsTr("RIGHT") : qsTr("LEFT"))
                    value: page.fmt(Math.abs(page.sol.windage || 0), 2)
                    unit: page.unitLabel
                    clicks: page.sol.hasScope
                            ? qsTr("%1 clicks").arg(Math.abs(page.sol.windageClicks || 0)) : ""
                }
            }

            // --- Quick wind ----------------------------------------------
            Pane {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Material.elevation: 1

                GridLayout {
                    anchors.fill: parent
                    columns: page.width >= 480 ? 3 : 2
                    columnSpacing: 16
                    rowSpacing: 8

                    NumberField {
                        Layout.preferredWidth: 130
                        label: qsTr("Wind speed")
                        unit: qsTr("m/s")
                        value: Backend.windSpeed
                        from: 0
                        to: 40
                        onEdited: v => Backend.windSpeed = v
                    }
                    Dial {
                        id: windDial
                        Layout.preferredWidth: 96
                        Layout.preferredHeight: 96
                        Layout.alignment: Qt.AlignHCenter
                        from: 0
                        to: 360
                        stepSize: 15
                        snapMode: Dial.SnapAlways
                        wrap: true
                        startAngle: 0
                        endAngle: 360
                        value: Backend.windFromDeg
                        onMoved: Backend.windFromDeg = value % 360
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.columnSpan: page.width >= 480 ? 1 : 2
                        Label { text: qsTr("Wind from"); opacity: 0.7 }
                        Label {
                            text: page.clockLabel(Backend.windFromDeg) + "  (" +
                                  Math.round(Backend.windFromDeg) + "°)"
                            font.pixelSize: 18
                        }
                        Label {
                            text: qsTr("12 = from the target, 3 = from the right")
                            opacity: 0.6
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            // --- Details ---------------------------------------------------
            Pane {
                visible: page.sol.ok === true
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Material.elevation: 1

                GridLayout {
                    anchors.fill: parent
                    columns: page.width >= 600 ? 4 : 2
                    columnSpacing: 16
                    rowSpacing: 10

                    Detail { label: qsTr("Velocity"); value: page.fmt(page.sol.velocity || 0, 0) + " " + qsTr("m/s") }
                    Detail { label: qsTr("Energy"); value: page.fmt(page.sol.energy || 0, 0) + " " + qsTr("J") }
                    Detail { label: qsTr("Time of flight"); value: page.fmt(page.sol.time || 0, 3) + " " + qsTr("s") }
                    Detail { label: qsTr("Mach"); value: page.fmt(page.sol.mach || 0, 2) }
                    Detail { label: qsTr("Drop"); value: page.fmt(page.sol.dropCm || 0, 1) + " " + qsTr("cm") }
                    Detail { label: qsTr("Drift"); value: page.fmt(page.sol.windageCm || 0, 1) + " " + qsTr("cm") }
                    Detail { label: qsTr("Spin drift"); value: page.fmt(page.sol.spinDriftCm || 0, 1) + " " + qsTr("cm") }
                    Detail { label: qsTr("Muzzle velocity"); value: page.fmt(page.sol.muzzleVelocity || 0, 1) + " " + qsTr("m/s") }
                    Detail {
                        label: qsTr("Stability Sg")
                        value: page.sol.stability > 0 ? page.fmt(page.sol.stability, 2) : "—"
                        warn: page.sol.stability > 0 && page.sol.stability < 1.4
                    }
                }
            }

            Label {
                visible: page.sol.ok === true && (page.sol.subsonic || page.sol.transonicRangeM > 0)
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                wrapMode: Text.Wrap
                color: Material.color(Material.Orange)
                text: page.sol.subsonic
                      ? qsTr("Subsonic at the target: expect larger dispersion.")
                      : qsTr("Transonic from about %1 m.").arg(Math.round(page.sol.transonicRangeM))
            }

            Item { Layout.preferredHeight: 16 }
        }
    }

    component CorrectionTile: Pane {
        id: tile
        property string title
        property string direction
        property string value
        property string unit
        property string clicks

        Material.elevation: 3
        padding: 16

        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            RowLayout {
                Label { text: tile.title; opacity: 0.7; font.pixelSize: 15 }
                Item { Layout.fillWidth: true }
                Label {
                    text: tile.direction
                    font.pixelSize: 18
                    font.bold: true
                    color: Material.accent
                }
            }
            RowLayout {
                spacing: 8
                Label {
                    text: tile.value
                    font.pixelSize: 56
                    font.bold: true
                }
                Label {
                    text: tile.unit
                    font.pixelSize: 18
                    opacity: 0.8
                    Layout.alignment: Qt.AlignBaseline
                }
            }
            Label {
                visible: tile.clicks.length > 0
                text: tile.clicks
                font.pixelSize: 18
            }
        }
    }

    component Detail: ColumnLayout {
        id: detail
        property string label
        property string value
        property bool warn: false
        spacing: 0
        Label { text: detail.label; opacity: 0.6; font.pixelSize: 12 }
        Label {
            text: detail.value
            font.pixelSize: 17
            color: detail.warn ? Material.color(Material.Red) : Material.foreground
        }
    }
}
