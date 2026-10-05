pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import BalCalc

// Range card and trajectory chart for the current profile and conditions.
Page {
    id: page

    implicitWidth: 360
    implicitHeight: 640

    // Emitted when a table row is tapped (its range becomes the target).
    signal rangeChosen()

    property alias tab: tabs.currentIndex
    property var table: ({ ok: false, rows: [] })
    property var curve: ({ ok: false, rows: [] })
    readonly property bool wide: width >= 720
    readonly property string unitLabel: Backend.angleUnit === "moa" ? qsTr("MOA") : qsTr("MRAD")

    function reload() {
        if (!visible)
            return
        table = Backend.rangeTable()
        curve = Backend.trajectoryCurve(Math.max(Backend.tableToM, Backend.targetRangeM), 250)
        chart.requestPaint()
    }

    onVisibleChanged: reload()
    Component.onCompleted: reload()
    Connections {
        target: Backend
        function onSolutionChanged() { page.reload() }
        function onTableSpecChanged() { page.reload() }
    }

    // Fixed decimals without "-0.00".
    function fmt(v, d) {
        var s = Number(v).toFixed(d)
        return Number(s) === 0 ? Number(0).toFixed(d) : s
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            TabBar {
                id: tabs
                Layout.fillWidth: true
                TabButton { text: qsTr("Table") }
                TabButton { text: qsTr("Chart") }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // --- Span ------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 12
            NumberField {
                Layout.fillWidth: true
                label: qsTr("From")
                unit: qsTr("m")
                decimals: 0
                value: Backend.tableFromM
                from: 0; to: 3000
                onEdited: v => Backend.tableFromM = v
            }
            NumberField {
                Layout.fillWidth: true
                label: qsTr("To")
                unit: qsTr("m")
                decimals: 0
                value: Backend.tableToM
                from: 10; to: 3000
                onEdited: v => Backend.tableToM = v
            }
            NumberField {
                Layout.fillWidth: true
                label: qsTr("Step")
                unit: qsTr("m")
                decimals: 0
                value: Backend.tableStepM
                from: 5; to: 500
                onEdited: v => Backend.tableStepM = v
            }
        }

        Label {
            visible: !page.table.ok
            Layout.fillWidth: true
            Layout.margins: 12
            wrapMode: Text.Wrap
            color: Material.color(Material.Red)
            text: page.table.error || ""
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            // --- Table -------------------------------------------------------
            ListView {
                id: list
                clip: true
                model: page.table.ok ? page.table.rows : []
                headerPositioning: ListView.OverlayHeader
                ScrollBar.vertical: ScrollBar {}

                readonly property var angleKeys: ["elevation", "windage", "elevationClicks", "windageClicks"]
                readonly property var columns: {
                    var c = [
                        { title: qsTr("Range\nm"), key: "rangeM", d: 0 },
                        { title: qsTr("Elev\n%1").arg(page.unitLabel), key: "elevation", d: 2 }
                    ]
                    if (page.table.hasScope)
                        c.push({ title: qsTr("Elev\nclicks"), key: "elevationClicks", d: 0 })
                    c.push({ title: qsTr("Wind\n%1").arg(page.unitLabel), key: "windage", d: 2 })
                    if (page.table.hasScope)
                        c.push({ title: qsTr("Wind\nclicks"), key: "windageClicks", d: 0 })
                    c.push({ title: qsTr("V\nm/s"), key: "velocity", d: 0 })
                    if (page.wide) {
                        c.push({ title: qsTr("Drop\ncm"), key: "dropCm", d: 1 })
                        c.push({ title: qsTr("Drift\ncm"), key: "windageCm", d: 1 })
                        c.push({ title: qsTr("Mach"), key: "mach", d: 2 })
                        c.push({ title: qsTr("Energy\nJ"), key: "energy", d: 0 })
                        c.push({ title: qsTr("Time\ns"), key: "time", d: 3 })
                    }
                    return c
                }

                header: Rectangle {
                    z: 2
                    width: list.width
                    height: 44
                    color: Material.backgroundColor
                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: list.columns
                            delegate: Label {
                                required property var modelData
                                width: list.width / list.columns.length
                                height: 44
                                horizontalAlignment: Text.AlignRight
                                verticalAlignment: Text.AlignVCenter
                                rightPadding: 8
                                text: modelData.title
                                font.pixelSize: 12
                                opacity: 0.7
                            }
                        }
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Material.dividerColor
                    }
                }

                delegate: ItemDelegate {
                    id: row
                    required property var modelData
                    required property int index
                    width: list.width
                    height: 36
                    padding: 0
                    readonly property bool isTarget: Math.abs(modelData.rangeM - Backend.targetRangeM) < 0.5
                    background: Rectangle {
                        color: row.isTarget ? Qt.alpha(Material.accent, 0.25)
                                            : (row.index % 2 ? Qt.alpha(Material.foreground, 0.04) : "transparent")
                    }
                    contentItem: Row {
                        Repeater {
                            model: list.columns
                            delegate: Label {
                                required property var modelData
                                width: list.width / list.columns.length
                                height: 36
                                horizontalAlignment: Text.AlignRight
                                verticalAlignment: Text.AlignVCenter
                                rightPadding: 8
                                font.pixelSize: 15
                                font.bold: row.isTarget
                                // No hold at the muzzle.
                                text: row.modelData.rangeM === 0 && list.angleKeys.indexOf(modelData.key) >= 0
                                      ? "—" : page.fmt(row.modelData[modelData.key], modelData.d)
                                color: row.modelData.mach < 1.0 ? Material.color(Material.Red)
                                       : row.modelData.mach < 1.2 ? Material.color(Material.Orange)
                                       : Material.foreground
                            }
                        }
                    }
                    onClicked: {
                        Backend.targetRangeM = row.modelData.rangeM
                        page.rangeChosen()
                    }
                }

                footer: Label {
                    width: list.width
                    padding: 12
                    wrapMode: Text.Wrap
                    font.pixelSize: 12
                    opacity: 0.7
                    text: qsTr("Corrections: up and right are positive. Orange: transonic (below Mach 1.2). Red: subsonic. Tap a row to aim at it.")
                }
            }

            // --- Chart -------------------------------------------------------
            Item {
                Canvas {
                    id: chart
                    anchors.fill: parent
                    anchors.margins: 8
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        var rows = page.curve.ok ? page.curve.rows : []
                        if (rows.length < 2)
                            return

                        var maxR = rows[rows.length - 1].rangeM
                        var minY = 0, maxY = 0
                        for (var i = 0; i < rows.length; ++i) {
                            minY = Math.min(minY, rows[i].dropCm, rows[i].windageCm)
                            maxY = Math.max(maxY, rows[i].dropCm, rows[i].windageCm)
                        }
                        var meters = (maxY - minY) > 300
                        var scale = meters ? 0.01 : 1
                        var pad = (maxY - minY) * 0.08 + 1
                        minY -= pad; maxY += pad

                        var left = 56, right = width - 12, top = 12, bottom = height - 36
                        function X(r) { return left + (right - left) * r / maxR }
                        function Y(v) { return top + (bottom - top) * (maxY - v) / (maxY - minY) }

                        var fg = Material.foreground
                        ctx.font = "12px sans-serif"
                        ctx.fillStyle = fg
                        ctx.strokeStyle = Qt.alpha(fg, 0.15)
                        ctx.lineWidth = 1

                        // Grid and axis labels.
                        var stepR = maxR > 1500 ? 250 : (maxR > 600 ? 100 : 50)
                        for (var r = 0; r <= maxR + 1e-6; r += stepR) {
                            ctx.beginPath(); ctx.moveTo(X(r), top); ctx.lineTo(X(r), bottom); ctx.stroke()
                            var label = String(Math.round(r))
                            ctx.fillText(label, Math.min(X(r) - 3 * label.length, right - 7 * label.length), bottom + 16)
                        }
                        var spanY = (maxY - minY) * scale
                        var stepY = Math.pow(10, Math.floor(Math.log(spanY / 5) / Math.LN10))
                        if (spanY / stepY > 10) stepY *= 2
                        for (var y = Math.ceil(minY * scale / stepY) * stepY; y <= maxY * scale; y += stepY) {
                            ctx.beginPath(); ctx.moveTo(left, Y(y / scale)); ctx.lineTo(right, Y(y / scale)); ctx.stroke()
                            ctx.fillText(Number(y).toFixed(stepY < 1 ? 1 : 0), 4, Y(y / scale) + 4)
                        }
                        ctx.fillText((meters ? qsTr("m") : qsTr("cm")), 4, top - 2)
                        ctx.fillText(qsTr("range, m"), right - 60, bottom + 30)

                        // Line of sight.
                        ctx.strokeStyle = Qt.alpha(fg, 0.6)
                        ctx.setLineDash([6, 4])
                        ctx.beginPath(); ctx.moveTo(X(0), Y(0)); ctx.lineTo(X(maxR), Y(0)); ctx.stroke()
                        ctx.setLineDash([])

                        // Target.
                        if (Backend.targetRangeM <= maxR) {
                            ctx.strokeStyle = Material.accent
                            ctx.beginPath(); ctx.moveTo(X(Backend.targetRangeM), top)
                            ctx.lineTo(X(Backend.targetRangeM), bottom); ctx.stroke()
                        }

                        function curveOf(key, color) {
                            ctx.strokeStyle = color
                            ctx.lineWidth = 2.5
                            ctx.beginPath()
                            for (var k = 0; k < rows.length; ++k) {
                                var px = X(rows[k].rangeM), py = Y(rows[k][key])
                                if (k === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py)
                            }
                            ctx.stroke()
                        }
                        curveOf("windageCm", Material.color(Material.Teal))
                        curveOf("dropCm", Material.color(Material.Orange))

                        // Legend.
                        ctx.fillStyle = Material.color(Material.Orange)
                        ctx.fillText("● " + qsTr("height over line of sight"), left + 8, top + 14)
                        ctx.fillStyle = Material.color(Material.Teal)
                        ctx.fillText("● " + qsTr("drift (right +)"), left + 8, top + 30)
                    }
                }
            }
        }
    }
}
