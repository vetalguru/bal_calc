pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BalCalc

// Shot log of the current profile and truing (fitting muzzle velocity and
// drag to the corrections that hit).
Page {
    id: page

    implicitWidth: 360
    implicitHeight: 640

    signal closed()

    property var shots: []
    property var result: null
    readonly property string unitLabel: Backend.angleUnit === "moa" ? qsTr("MOA") : qsTr("MRAD")

    function reload() {
        shots = Backend.shots()
        result = null
    }
    function fmt(v, d) {
        var s = Number(v).toFixed(d)
        return Number(s) === 0 ? Number(0).toFixed(d) : s
    }

    Component.onCompleted: reload()
    Connections {
        target: Backend
        function onShotsChanged() { page.reload() }
        function onCurrentProfileIdChanged() { page.reload() }
        function onAngleUnitChanged() { page.reload() }
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            Button { text: qsTr("Back"); flat: true; onClicked: page.closed() }
            Label {
                text: qsTr("Shot log and truing")
                font.pixelSize: 18
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Log a hit")
                highlighted: true
                onClicked: logDialog.openFor(Backend.targetRangeM, Backend.solution.elevation || 0)
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
                Layout.fillWidth: true
                Layout.margins: 12
                wrapMode: Text.Wrap
                opacity: 0.8
                text: qsTr("Log the elevation that actually hit, at several distances (ideally one close, one where the bullet is still fast, one far). Truing then adjusts the muzzle velocity and the drag of this profile so the calculator agrees with your rifle.")
            }

            Label {
                visible: page.shots.length === 0
                Layout.fillWidth: true
                Layout.margins: 12
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.6
                text: qsTr("No shots logged for this profile yet.")
            }

            // --- Shots -------------------------------------------------------
            Repeater {
                model: page.shots
                delegate: ItemDelegate {
                    id: shot
                    required property var modelData
                    Layout.fillWidth: true
                    contentItem: RowLayout {
                        CheckBox {
                            checked: shot.modelData.used
                            onToggled: Backend.setShotUsed(shot.modelData.id, checked)
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                font.pixelSize: 16
                                text: qsTr("%1 m: hit at %2 %3").arg(Math.round(shot.modelData.rangeM))
                                      .arg(page.fmt(shot.modelData.observed, 2)).arg(page.unitLabel)
                            }
                            Label {
                                opacity: 0.6
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: (shot.modelData.predicted !== undefined
                                       ? qsTr("predicted %1").arg(page.fmt(shot.modelData.predicted, 2)) + "  ·  " : "")
                                      + page.fmt(shot.modelData.temperatureC, 0) + " °C  ·  "
                                      + shot.modelData.shotAt.replace("T", " ").replace("Z", "")
                                      + (shot.modelData.notes.length > 0 ? "  ·  " + shot.modelData.notes : "")
                            }
                        }
                        Button {
                            text: "✕"
                            flat: true
                            onClicked: Backend.deleteShot(shot.modelData.id)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 12
                Button {
                    text: qsTr("Calculate truing")
                    highlighted: true
                    enabled: page.shots.length > 0
                    onClicked: page.result = Backend.computeTruing()
                }
                Button {
                    text: qsTr("Reset truing")
                    flat: true
                    enabled: Backend.solution.ok === true &&
                             (Backend.solution.velocityScale !== 1 || Backend.solution.dragScale !== 1)
                    onClicked: Backend.resetTruing()
                }
            }

            Label {
                visible: Backend.solution.ok === true &&
                         (Backend.solution.velocityScale !== 1 || Backend.solution.dragScale !== 1)
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                wrapMode: Text.Wrap
                text: qsTr("The profile is trued: velocity ×%1, drag ×%2.")
                      .arg(page.fmt(Backend.solution.velocityScale, 4))
                      .arg(page.fmt(Backend.solution.dragScale, 3))
            }

            // --- Result ------------------------------------------------------
            Pane {
                visible: page.result !== null
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Material.elevation: 2

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6
                    Label {
                        visible: page.result !== null && !page.result.ok
                        text: page.result ? page.result.error : ""
                        color: Material.color(Material.Red)
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    ColumnLayout {
                        visible: page.result !== null && page.result.ok
                        Layout.fillWidth: true
                        spacing: 4
                        Label {
                            font.pixelSize: 16
                            font.bold: true
                            text: page.result && page.result.ok
                                  ? qsTr("Muzzle velocity %1 → %2 m/s").arg(page.fmt(page.result.velocityBefore, 1))
                                        .arg(page.fmt(page.result.velocityAfter, 1)) : ""
                        }
                        Label {
                            text: page.result && page.result.ok
                                  ? (page.result.dragFitted
                                     ? qsTr("Drag ×%1").arg(page.fmt(page.result.dragScale, 3))
                                     : qsTr("Drag unchanged: log shots at more different distances to fit it."))
                                  : ""
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        Label {
                            text: page.result && page.result.ok
                                  ? qsTr("Average miss %1 → %2 %3").arg(page.fmt(page.result.rmsBefore, 2))
                                        .arg(page.fmt(page.result.rmsAfter, 2)).arg(page.unitLabel) : ""
                        }
                        Repeater {
                            model: page.result && page.result.ok ? page.result.points : []
                            delegate: Label {
                                required property var modelData
                                opacity: 0.75
                                text: qsTr("%1 m: hit %2, was %3, now %4").arg(Math.round(modelData.rangeM))
                                      .arg(page.fmt(modelData.observed, 2)).arg(page.fmt(modelData.before, 2))
                                      .arg(page.fmt(modelData.after, 2))
                            }
                        }
                        Button {
                            text: qsTr("Apply to the profile")
                            highlighted: true
                            onClicked: {
                                var err = Backend.applyTruing()
                                if (err.length > 0)
                                    page.result = { ok: false, error: err }
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 16 }
        }
    }

    LogShotDialog { id: logDialog }
}
