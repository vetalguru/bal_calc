pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BalCalc

// A rifle: barrel, scope and zero.
Page {
    id: page

    // Sized by the parent layout; a fixed implicit size avoids a binding
    // loop through the scrolled content's width.
    implicitWidth: 360
    implicitHeight: 640

    // Field values, keys as Backend.rifleForm() returns them.
    property var form: ({})
    signal done()

    readonly property bool wide: width >= 640
    readonly property var clickUnits: [
        { value: "mrad", text: qsTr("MRAD") },
        { value: "moa", text: qsTr("MOA") },
        { value: "smoa", text: qsTr("inch / 100 yd") },
        { value: "cm100m", text: qsTr("cm / 100 m") }
    ]

    function save() {
        // Push edits from a field that still has focus.
        page.forceActiveFocus()
        var err = Backend.saveRifle(page.form)
        errorLabel.text = err
        if (err.length === 0)
            page.done()
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            Button {
                text: qsTr("Cancel")
                flat: true
                onClicked: page.done()
            }
            Label {
                text: page.form.rifleId > 0 ? qsTr("Edit rifle") : qsTr("New rifle")
                font.pixelSize: 18
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Save")
                highlighted: true
                onClicked: page.save()
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 12

            Label {
                id: errorLabel
                visible: text.length > 0
                Layout.fillWidth: true
                Layout.margins: 12
                wrapMode: Text.Wrap
                color: Material.color(Material.Red)
            }

            FormGroup {
                title: qsTr("Rifle")
                Layout.topMargin: 12
                TextField {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Name, e.g. Tikka T3x")
                    text: page.form.name || ""
                    onTextEdited: page.form.name = text
                }
                TextField {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Caliber, e.g. .308 Win")
                    text: page.form.caliber || ""
                    onTextEdited: page.form.caliber = text
                }
                Grid2 {
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Sight height over bore")
                        unit: qsTr("cm")
                        value: page.form.sightHeightCm
                        from: 0; to: 20
                        onEdited: v => page.form.sightHeightCm = v
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Twist rate, 1 turn in (0 = unknown)")
                        unit: qsTr("in")
                        decimals: 2
                        value: page.form.twistIn
                        from: 0; to: 60
                        onEdited: v => page.form.twistIn = v
                    }
                }
                SwitchRow {
                    text: qsTr("Left-hand twist")
                    checked: page.form.twistLeft || false
                    onToggled: page.form.twistLeft = checked
                }
            }

            FormGroup {
                title: qsTr("Scope")
                Grid2 {
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label { text: qsTr("Turret units"); font.pixelSize: 12; opacity: 0.7 }
                        ComboBox {
                            Layout.fillWidth: true
                            model: page.clickUnits
                            textRole: "text"
                            valueRole: "value"
                            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(page.form.clickUnits))
                            onActivated: page.form.clickUnits = currentValue
                        }
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("One click")
                        decimals: 4
                        value: page.form.clickValue
                        from: 0; to: 10
                        onEdited: v => page.form.clickValue = v
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label { text: qsTr("Reticle"); font.pixelSize: 12; opacity: 0.7 }
                        ComboBox {
                            Layout.fillWidth: true
                            model: [{ id: 0, name: qsTr("None (plain crosshair)") }].concat(Backend.reticles())
                            textRole: "name"
                            valueRole: "id"
                            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(page.form.reticleId || 0))
                            onActivated: page.form.reticleId = currentValue
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label { text: qsTr("Focal plane"); font.pixelSize: 12; opacity: 0.7 }
                        ComboBox {
                            id: focalPlaneBox
                            Layout.fillWidth: true
                            model: [{ value: "ffp", text: qsTr("First (FFP)") },
                                    { value: "sfp", text: qsTr("Second (SFP)") }]
                            textRole: "text"
                            valueRole: "value"
                            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(page.form.focalPlane || "ffp"))
                            onActivated: page.form.focalPlane = currentValue
                        }
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Magnification from")
                        unit: "×"
                        value: page.form.minMagnification
                        from: 0; to: 100
                        onEdited: v => page.form.minMagnification = v
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Magnification to")
                        unit: "×"
                        value: page.form.maxMagnification
                        from: 0; to: 100
                        onEdited: v => page.form.maxMagnification = v
                    }
                    NumberField {
                        visible: focalPlaneBox.currentValue === "sfp"
                        Layout.fillWidth: true
                        label: qsTr("SFP: marks are true at")
                        unit: "×"
                        value: page.form.sfpReferenceMagnification
                        from: 0; to: 100
                        onEdited: v => page.form.sfpReferenceMagnification = v
                    }
                }
            }

            FormGroup {
                title: qsTr("Zero")
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.7
                    text: qsTr("Where and in what air the rifle was zeroed. A cartridge that hits elsewhere at the zero distance gets its own shift in its shot log.")
                }
                Grid2 {
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Zero distance")
                        unit: qsTr("m")
                        decimals: 0
                        value: page.form.zeroRangeM
                        from: 10; to: 1000
                        onEdited: v => page.form.zeroRangeM = v
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Temperature when zeroed")
                        unit: qsTr("°C")
                        value: page.form.zeroTemperatureC
                        from: -60; to: 60
                        onEdited: v => { page.form.zeroTemperatureC = v; page.form.zeroPowderC = v }
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Station pressure when zeroed")
                        unit: qsTr("hPa")
                        value: page.form.zeroPressureHpa
                        from: 300; to: 1200
                        onEdited: v => page.form.zeroPressureHpa = v
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Altitude when zeroed")
                        unit: qsTr("m")
                        decimals: 0
                        value: page.form.zeroAltitudeM
                        from: -500; to: 6000
                        onEdited: v => page.form.zeroAltitudeM = v
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Humidity when zeroed")
                        unit: "%"
                        decimals: 0
                        value: page.form.zeroHumidityPct
                        from: 0; to: 100
                        onEdited: v => page.form.zeroHumidityPct = v
                    }
                }
            }

            Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 24
                text: qsTr("Save")
                highlighted: true
                onClicked: page.save()
            }
        }
    }

    component Grid2: GridLayout {
        Layout.fillWidth: true
        columns: page.wide ? 2 : 1
        columnSpacing: 16
        rowSpacing: 8
    }
}
