pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BalCalc

// One form for a rifle, its scope, its cartridge and the zero.
Page {
    id: page

    // Sized by the parent layout; a fixed implicit size avoids a binding
    // loop through the scrolled content's width.
    implicitWidth: 360
    implicitHeight: 640

    // Field values, keys as Backend.profileForm() returns them.
    property var form: ({})
    // The stack this editor was pushed on (to open the bullet library).
    property StackView stack
    signal done()

    readonly property bool fromLibrary: (form.libraryBulletId || 0) > 0

    function chooseBullet() {
        page.forceActiveFocus()
        var library = page.stack.push(libraryComponent, { stack: page.stack, picker: true })
        library.picked.connect(function(id) {
            page.form = Backend.profileFormWithBullet(page.form, id)
            page.stack.pop()
        })
    }

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
        var err = Backend.saveProfile(page.form)
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
                text: page.form.profileId > 0 ? qsTr("Edit profile") : qsTr("New profile")
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

            Group {
                title: qsTr("Profile")
                TextField {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Name, e.g. Tikka T3x .308 / SMK 175")
                    text: page.form.name || ""
                    onTextEdited: page.form.name = text
                }
                TextField {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Caliber, e.g. .308 Win")
                    text: page.form.caliber || ""
                    onTextEdited: page.form.caliber = text
                }
            }

            Group {
                title: qsTr("Rifle and scope")
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
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("One click")
                        decimals: 4
                        value: page.form.clickValue
                        from: 0; to: 10
                        onEdited: v => page.form.clickValue = v
                    }
                }
            }

            Group {
                title: qsTr("Bullet")
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        opacity: 0.8
                        text: page.fromLibrary ? qsTr("From the library: %1").arg(page.form.bulletName)
                                               : qsTr("Own bullet of this profile")
                    }
                    Button {
                        text: page.fromLibrary ? qsTr("Change") : qsTr("From library")
                        flat: true
                        onClicked: page.chooseBullet()
                    }
                    Button {
                        visible: page.fromLibrary
                        text: qsTr("Edit as own")
                        flat: true
                        onClicked: {
                            var f = page.form
                            f.libraryBulletId = 0
                            page.form = f
                            page.formChanged()
                        }
                    }
                }
                TextField {
                    Layout.fillWidth: true
                    enabled: !page.fromLibrary
                    placeholderText: qsTr("Bullet, e.g. Sierra MatchKing 175 gr")
                    text: page.form.bulletName || ""
                    onTextEdited: page.form.bulletName = text
                }
                Grid2 {
                    enabled: !page.fromLibrary
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label { text: qsTr("Drag model"); font.pixelSize: 12; opacity: 0.7 }
                        ComboBox {
                            Layout.fillWidth: true
                            model: ["G7", "G1", "G2", "G5", "G6", "G8", "GI", "GS", "RA4"]
                            Component.onCompleted: currentIndex = Math.max(0, find(page.form.dragTable))
                            onActivated: page.form.dragTable = currentText
                        }
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Ballistic coefficient")
                        decimals: 3
                        value: page.form.bc
                        from: 0; to: 2
                        onEdited: v => page.form.bc = v
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Weight")
                        unit: qsTr("gr")
                        value: page.form.massGr
                        from: 0; to: 2000
                        onEdited: v => page.form.massGr = v
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Diameter")
                        unit: qsTr("in")
                        decimals: 3
                        value: page.form.diameterIn
                        from: 0; to: 1
                        onEdited: v => page.form.diameterIn = v
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Length (for spin drift)")
                        unit: qsTr("in")
                        decimals: 3
                        value: page.form.lengthIn
                        from: 0; to: 4
                        onEdited: v => page.form.lengthIn = v
                    }
                }
            }

            Group {
                title: qsTr("Cartridge")
                Grid2 {
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Muzzle velocity")
                        unit: qsTr("m/s")
                        value: page.form.muzzleVelocity
                        from: 0; to: 2000
                        onEdited: v => page.form.muzzleVelocity = v
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Measured at powder temperature")
                        unit: qsTr("°C")
                        value: page.form.powderReferenceC
                        from: -60; to: 80
                        onEdited: v => page.form.powderReferenceC = v
                    }
                    NumberField {
                        Layout.fillWidth: true
                        label: qsTr("Powder sensitivity")
                        unit: qsTr("%/°C")
                        decimals: 3
                        value: page.form.powderSensitivity
                        from: -2; to: 2
                        onEdited: v => page.form.powderSensitivity = v
                    }
                }
            }

            Group {
                title: qsTr("Zero")
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
                        label: qsTr("Impact above aim at zero")
                        unit: qsTr("cm")
                        value: page.form.zeroOffsetUpCm
                        from: -50; to: 50
                        onEdited: v => page.form.zeroOffsetUpCm = v
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

    Component {
        id: libraryComponent
        LibraryPage {
            onClosed: page.stack.pop()
        }
    }

    component Group: Pane {
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

    component Grid2: GridLayout {
        Layout.fillWidth: true
        columns: page.wide ? 2 : 1
        columnSpacing: 16
        rowSpacing: 8
    }
}
