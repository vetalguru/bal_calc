pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BalCalc

// Everything about the shot that is not the rifle: air, powder, wind,
// angles and Earth rotation.
Page {
    id: page

    // Sized by the parent layout; a fixed implicit size avoids a binding
    // loop through the scrolled content's width.
    implicitWidth: 360
    implicitHeight: 640

    // Pressure can be typed as station pressure or as sea-level QNH.
    property bool qnhMode: false
    property real qnh: 1013.25

    header: ToolBar {
        Label {
            anchors.centerIn: parent
            text: qsTr("Conditions")
            font.pixelSize: 18
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 12

            Item { Layout.preferredHeight: 4 }

            Section {
                title: qsTr("Atmosphere")
                GridLayout {
                    Layout.fillWidth: true
                    columns: page.width >= 600 ? 2 : 1
                    columnSpacing: 16

                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Temperature")
                        unit: qsTr("°C")
                        value: Backend.temperatureC
                        from: -60
                        to: 60
                        onEdited: v => Backend.temperatureC = v
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Altitude above sea level")
                        unit: qsTr("m")
                        decimals: 0
                        value: Backend.altitudeM
                        from: -500
                        to: 6000
                        onEdited: v => {
                            Backend.altitudeM = v
                            if (page.qnhMode)
                                Backend.pressureHpa = Backend.stationPressure(page.qnh, v)
                        }
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: page.qnhMode ? qsTr("Pressure at sea level (QNH)")
                                            : qsTr("Station pressure (absolute)")
                        unit: qsTr("hPa")
                        value: page.qnhMode ? page.qnh : Backend.pressureHpa
                        from: 300
                        to: 1200
                        onEdited: v => {
                            if (page.qnhMode) {
                                page.qnh = v
                                Backend.pressureHpa = Backend.stationPressure(v, Backend.altitudeM)
                            } else {
                                Backend.pressureHpa = v
                            }
                        }
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Relative humidity")
                        unit: "%"
                        decimals: 0
                        value: Backend.humidityPct
                        from: 0
                        to: 100
                        onEdited: v => Backend.humidityPct = v
                    }
                }
                SwitchRow {
                    text: qsTr("I enter sea-level pressure (QNH, weather report)")
                    checked: page.qnhMode
                    onToggled: page.qnhMode = checked
                }
                Label {
                    visible: page.qnhMode
                    text: qsTr("Station pressure: %1 hPa").arg(Backend.pressureHpa.toFixed(1))
                    opacity: 0.7
                }
            }

            Section {
                title: qsTr("Powder")
                SwitchRow {
                    text: qsTr("Powder temperature = air temperature")
                    checked: Backend.powderFollowsAir
                    onToggled: Backend.powderFollowsAir = checked
                }
                NumberField {
                    visible: !Backend.powderFollowsAir
                    Layout.fillWidth: true
                    label: qsTr("Powder temperature")
                    unit: qsTr("°C")
                    value: Backend.powderC
                    from: -60
                    to: 80
                    onEdited: v => Backend.powderC = v
                }
            }

            Section {
                title: qsTr("Wind")
                GridLayout {
                    Layout.fillWidth: true
                    columns: page.width >= 600 ? 2 : 1
                    columnSpacing: 16
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Speed")
                        unit: qsTr("m/s")
                        value: Backend.windSpeed
                        from: 0
                        to: 40
                        onEdited: v => Backend.windSpeed = v
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Blowing from (0 = from the target, 90 = from the right)")
                        unit: "°"
                        decimals: 0
                        value: Backend.windFromDeg
                        from: 0
                        to: 360
                        onEdited: v => Backend.windFromDeg = v % 360
                    }
                }
            }

            Section {
                title: qsTr("Angles")
                GridLayout {
                    Layout.fillWidth: true
                    columns: page.width >= 600 ? 2 : 1
                    columnSpacing: 16
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Shot angle (uphill +, downhill −)")
                        unit: "°"
                        value: Backend.lookAngleDeg
                        from: -60
                        to: 60
                        onEdited: v => Backend.lookAngleDeg = v
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Cant (clockwise +)")
                        unit: "°"
                        value: Backend.cantDeg
                        from: -45
                        to: 45
                        onEdited: v => Backend.cantDeg = v
                    }
                }
            }

            Section {
                title: qsTr("Earth rotation (Coriolis)")
                SwitchRow {
                    text: qsTr("Account for Earth rotation")
                    checked: Backend.coriolis
                    onToggled: Backend.coriolis = checked
                }
                GridLayout {
                    visible: Backend.coriolis
                    Layout.fillWidth: true
                    columns: page.width >= 600 ? 2 : 1
                    columnSpacing: 16
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Latitude (north +)")
                        unit: "°"
                        value: Backend.latitudeDeg
                        from: -90
                        to: 90
                        onEdited: v => Backend.latitudeDeg = v
                    }
                    ColumnLayout {
                        SwitchRow {
                            text: qsTr("Known shot direction")
                            checked: Backend.useAzimuth
                            onToggled: Backend.useAzimuth = checked
                        }
                        NumberField {
                            visible: Backend.useAzimuth
                            Layout.fillWidth: true
                            label: qsTr("Azimuth (from north, clockwise)")
                            unit: "°"
                            decimals: 0
                            value: Backend.azimuthDeg
                            from: 0
                            to: 360
                            onEdited: v => Backend.azimuthDeg = v % 360
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }
        }
    }

    component Section: Pane {
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
}
