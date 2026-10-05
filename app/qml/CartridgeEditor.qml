pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BalCalc

// A cartridge: its bullet and muzzle velocity.
Page {
    id: page

    // Sized by the parent layout; a fixed implicit size avoids a binding
    // loop through the scrolled content's width.
    implicitWidth: 360
    implicitHeight: 640

    // Field values, keys as Backend.cartridgeForm() returns them.
    property var form: ({})
    // The stack this editor was pushed on (to open the bullet library).
    property StackView stack
    signal done()

    readonly property bool fromLibrary: (form.libraryBulletId || 0) > 0
    readonly property bool wide: width >= 640

    function chooseBullet() {
        page.forceActiveFocus()
        var library = page.stack.push(libraryComponent, { stack: page.stack, picker: true })
        library.picked.connect(function(id) {
            page.form = Backend.cartridgeFormWithBullet(page.form, id)
            page.stack.pop()
        })
    }

    function save() {
        // Push edits from a field that still has focus.
        page.forceActiveFocus()
        var err = Backend.saveCartridge(page.form)
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
                text: page.form.cartridgeId > 0 ? qsTr("Edit cartridge") : qsTr("New cartridge")
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
                title: qsTr("Cartridge")
                Layout.topMargin: 12
                TextField {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Name, e.g. Handload SMK 175 / 43.5 gr Varget")
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

            FormGroup {
                title: qsTr("Bullet")
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        opacity: 0.8
                        text: page.fromLibrary ? qsTr("From the library: %1").arg(page.form.bulletName)
                                               : qsTr("Own bullet of this cartridge")
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

            FormGroup {
                title: qsTr("Velocity")
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

    component Grid2: GridLayout {
        Layout.fillWidth: true
        columns: page.wide ? 2 : 1
        columnSpacing: 16
        rowSpacing: 8
    }
}
