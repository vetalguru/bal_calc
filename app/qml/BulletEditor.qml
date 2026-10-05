pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BalCalc

// One library bullet: identity, size and drag (one BC or BC bands).
Page {
    id: page

    implicitWidth: 360
    implicitHeight: 640

    property var form: ({})
    signal done()

    readonly property bool wide: width >= 640
    // Bands are edited as a list model so rows can be added and removed.
    property var bands: (form.bands || []).slice()
    property bool banded: bands.length > 0

    function save() {
        page.forceActiveFocus()
        var f = page.form
        f.bands = page.banded ? page.bands : []
        var err = Backend.saveBullet(f)
        errorLabel.text = err
        if (err.length === 0)
            page.done()
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            Button { text: qsTr("Cancel"); flat: true; onClicked: page.done() }
            Label {
                text: page.form.id > 0 ? qsTr("Edit bullet") : qsTr("New bullet")
                font.pixelSize: 18
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
            Button { text: qsTr("Save"); highlighted: true; onClicked: page.save() }
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

            Pane {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Material.elevation: 1
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    TextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("Name, e.g. MatchKing 175 gr HPBT")
                        text: page.form.name || ""
                        onTextEdited: page.form.name = text
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: page.wide ? 2 : 1
                        columnSpacing: 16
                        TextField {
                            Layout.fillWidth: true
                            placeholderText: qsTr("Manufacturer")
                            text: page.form.manufacturer || ""
                            onTextEdited: page.form.manufacturer = text
                        }
                        TextField {
                            Layout.fillWidth: true
                            placeholderText: qsTr("Caliber, e.g. .308 Win")
                            text: page.form.caliber || ""
                            onTextEdited: page.form.caliber = text
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
            }

            Pane {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Material.elevation: 1
                enabled: !page.form.hasCustomCurve
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    Label {
                        text: qsTr("Drag")
                        font.pixelSize: 16
                        font.bold: true
                        color: Material.accent
                    }
                    Label {
                        visible: page.form.hasCustomCurve === true
                        text: qsTr("This bullet uses its own measured drag curve.")
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["G7", "G1", "G2", "G5", "G6", "G8", "GI", "GS", "RA4"]
                        Component.onCompleted: currentIndex = Math.max(0, find(page.form.dragTable))
                        onActivated: page.form.dragTable = currentText
                    }
                    SwitchRow {
                        text: qsTr("Different BCs by velocity (as published by Sierra)")
                        checked: page.banded
                        onToggled: {
                            page.banded = checked
                            if (checked && page.bands.length === 0)
                                page.bands = [{ velocity: 800, bc: page.form.bc || 0.3 }]
                        }
                    }
                    NumberField {
                        visible: !page.banded
                        Layout.fillWidth: true
                        label: qsTr("Ballistic coefficient")
                        decimals: 3
                        value: page.form.bc
                        from: 0; to: 2
                        onEdited: v => page.form.bc = v
                    }
                    Repeater {
                        model: page.banded ? page.bands.length : 0
                        delegate: RowLayout {
                            id: band
                            required property int index
                            Layout.fillWidth: true
                            spacing: 12
                            NumberField {
                                Layout.fillWidth: true
                                label: qsTr("Above velocity")
                                unit: qsTr("m/s")
                                decimals: 0
                                value: page.bands[band.index].velocity
                                from: 0; to: 2000
                                onEdited: v => page.bands[band.index].velocity = v
                            }
                            NumberField {
                                Layout.fillWidth: true
                                label: qsTr("BC")
                                decimals: 3
                                value: page.bands[band.index].bc
                                from: 0; to: 2
                                onEdited: v => page.bands[band.index].bc = v
                            }
                            Button {
                                text: "✕"
                                flat: true
                                onClicked: {
                                    var b = page.bands.slice()
                                    b.splice(band.index, 1)
                                    page.bands = b
                                    if (b.length === 0)
                                        page.banded = false
                                }
                            }
                        }
                    }
                    Button {
                        visible: page.banded
                        text: qsTr("Add band")
                        flat: true
                        onClicked: {
                            var b = page.bands.slice()
                            var last = b.length > 0 ? b[b.length - 1] : { velocity: 800, bc: 0.3 }
                            b.push({ velocity: Math.max(100, last.velocity - 200), bc: last.bc })
                            page.bands = b
                        }
                    }
                }
            }

            Pane {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Material.elevation: 1
                TextArea {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    placeholderText: qsTr("Notes, source of the data")
                    wrapMode: Text.Wrap
                    text: page.form.notes || ""
                    onTextChanged: page.form.notes = text
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 12
                Button {
                    visible: page.form.id > 0
                    text: qsTr("Delete")
                    flat: true
                    Material.foreground: Material.Red
                    onClicked: {
                        var err = Backend.deleteBullet(page.form.id)
                        errorLabel.text = err
                        if (err.length === 0)
                            page.done()
                    }
                }
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Save"); highlighted: true; onClicked: page.save() }
            }
        }
    }
}
