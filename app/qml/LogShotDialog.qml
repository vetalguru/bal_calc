import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BalCalc

// Records the correction that actually hit at the current conditions.
Dialog {
    id: dialog

    property real rangeM: 300
    property real elevation: 0
    property real windage: 0
    property bool withWindage: false

    function openFor(range, elevationGuess) {
        rangeM = range
        elevation = Math.round(elevationGuess * 100) / 100
        windage = 0
        withWindage = false
        notes.text = ""
        errorLabel.text = ""
        open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 400, 460)
    modal: true
    title: qsTr("Log a hit")
    standardButtons: Dialog.Save | Dialog.Cancel

    onAccepted: {
        var err = Backend.logShot(rangeM, elevation, withWindage, windage, notes.text)
        if (err.length > 0) {
            errorLabel.text = err
            open()
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: 8
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            opacity: 0.8
            text: qsTr("Current air, powder temperature and shot angle are saved with it.")
        }
        NumberField {
            Layout.fillWidth: true
            label: qsTr("Distance")
            unit: qsTr("m")
            decimals: 0
            value: dialog.rangeM
            from: 10; to: 3000
            onEdited: v => dialog.rangeM = v
        }
        NumberField {
            Layout.fillWidth: true
            label: qsTr("Elevation that hit (up +)")
            unit: Backend.angleUnit === "moa" ? qsTr("MOA") : qsTr("MRAD")
            decimals: 2
            value: dialog.elevation
            from: -100; to: 200
            onEdited: v => dialog.elevation = v
        }
        SwitchRow {
            text: qsTr("Also log the windage that hit")
            checked: dialog.withWindage
            onToggled: dialog.withWindage = checked
        }
        NumberField {
            visible: dialog.withWindage
            Layout.fillWidth: true
            label: qsTr("Windage that hit (right +)")
            unit: Backend.angleUnit === "moa" ? qsTr("MOA") : qsTr("MRAD")
            decimals: 2
            value: dialog.windage
            from: -100; to: 100
            onEdited: v => dialog.windage = v
        }
        TextField {
            id: notes
            Layout.fillWidth: true
            placeholderText: qsTr("Notes (group size, light, wind)")
        }
        Label {
            id: errorLabel
            visible: text.length > 0
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Material.color(Material.Red)
        }
    }
}
