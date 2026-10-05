import QtQuick

// Draws a reticle (Backend's JSON drawing in nominal mrad, y up) and marks
// where the target goes. Zooms so the target and its surroundings fit.
Item {
    id: root

    // Reticle drawing JSON (see applogic/importers.h); empty = plain crosshair.
    property string definition
    // Target position in drawing units (mrad), y up.
    property real targetX: 0
    property real targetY: 0

    readonly property var drawing: {
        try {
            return definition.length > 0 ? JSON.parse(definition) : null
        } catch (e) {
            return null
        }
    }

    // Half-width of the visible field in drawing units.
    readonly property real span: {
        var reach = Math.max(Math.abs(targetX), Math.abs(targetY))
        var half = drawing && drawing.size ? Math.max(drawing.size[0], drawing.size[1]) / 2 : 10
        // The reticle around the centre, widened when the target needs room.
        return Math.max(2.5, Math.min(half, 6), reach * 1.25 + 1)
    }

    onDrawingChanged: canvas.requestPaint()
    onTargetXChanged: canvas.requestPaint()
    onTargetYChanged: canvas.requestPaint()
    onSpanChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var size = Math.min(width, height)
            var cx = width / 2, cy = height / 2
            var k = size / 2 / root.span // px per mrad
            function X(x) { return cx + x * k }
            function Y(y) { return cy - y * k }
            function W(w) { return Math.max(1, (w || 0) * k) }

            // Field of view.
            ctx.save()
            ctx.beginPath()
            ctx.arc(cx, cy, size / 2 - 1, 0, 2 * Math.PI)
            ctx.fillStyle = "#f4f1e8"
            ctx.fill()
            ctx.clip()

            var ink = "#111111"
            ctx.strokeStyle = ink
            ctx.fillStyle = ink
            var els = root.drawing ? root.drawing.elements : null
            if (!els) {
                // No reticle: a fine crosshair with 1 mrad ticks.
                els = [{ t: "line", x1: -root.span, y1: 0, x2: root.span, y2: 0, w: 0.02 },
                       { t: "line", x1: 0, y1: -root.span, x2: 0, y2: root.span, w: 0.02 }]
                for (var m = -Math.floor(root.span); m <= root.span; ++m) {
                    if (m === 0) continue
                    els.push({ t: "line", x1: m, y1: -0.15, x2: m, y2: 0.15, w: 0.02 })
                    els.push({ t: "line", x1: -0.15, y1: m, x2: 0.15, y2: m, w: 0.02 })
                }
            }
            for (var i = 0; i < els.length; ++i) {
                var e = els[i]
                if (e.t === "line") {
                    ctx.lineWidth = W(e.w)
                    ctx.beginPath()
                    ctx.moveTo(X(e.x1), Y(e.y1))
                    ctx.lineTo(X(e.x2), Y(e.y2))
                    ctx.stroke()
                } else if (e.t === "circle") {
                    ctx.lineWidth = W(e.w)
                    ctx.beginPath()
                    ctx.arc(X(e.x), Y(e.y), Math.max(0.8, e.r * k), 0, 2 * Math.PI)
                    if (e.fill) ctx.fill(); else ctx.stroke()
                } else if (e.t === "text") {
                    ctx.font = Math.max(8, e.h * k) + "px sans-serif"
                    ctx.fillText(e.s, X(e.x), Y(e.y))
                } else if (e.t === "path") {
                    ctx.lineWidth = W(e.w)
                    ctx.beginPath()
                    var px = 0, py = 0
                    for (var j = 0; j < e.d.length; ++j) {
                        var c = e.d[j]
                        if (c[0] === "M") {
                            ctx.moveTo(X(c[1]), Y(c[2]))
                        } else if (c[0] === "L") {
                            ctx.lineTo(X(c[1]), Y(c[2]))
                        } else if (c[0] === "A") {
                            arcTo(ctx, px, py, c[1], c[2], c[3], c[4], c[5], X, Y, k)
                        }
                        px = c[1]; py = c[2]
                    }
                    if (e.fill) ctx.fill(); else ctx.stroke()
                }
            }
            ctx.restore()

            // Field edge.
            ctx.strokeStyle = "#555555"
            ctx.lineWidth = 2
            ctx.beginPath()
            ctx.arc(cx, cy, size / 2 - 1, 0, 2 * Math.PI)
            ctx.stroke()

            // Target.
            var tx = X(root.targetX), ty = Y(root.targetY)
            ctx.strokeStyle = "#e53935"
            ctx.fillStyle = "rgba(229, 57, 53, 0.25)"
            ctx.lineWidth = 2.5
            ctx.beginPath()
            ctx.arc(tx, ty, 9, 0, 2 * Math.PI)
            ctx.fill()
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(tx - 15, ty); ctx.lineTo(tx - 5, ty)
            ctx.moveTo(tx + 5, ty); ctx.lineTo(tx + 15, ty)
            ctx.moveTo(tx, ty - 15); ctx.lineTo(tx, ty - 5)
            ctx.moveTo(tx, ty + 5); ctx.lineTo(tx, ty + 15)
            ctx.stroke()
        }

        // SVG-style arc from (x0, y0) to (x1, y1) with radius r (drawing units).
        function arcTo(ctx, x0, y0, x1, y1, r, clockwise, major, X, Y, k) {
            var dx = (x1 - x0) / 2, dy = (y1 - y0) / 2
            var d2 = dx * dx + dy * dy
            if (d2 === 0) return
            var rr = Math.max(r, Math.sqrt(d2))
            var h = Math.sqrt(Math.max(0, rr * rr - d2))
            var mx = x0 + dx, my = y0 + dy
            var len = Math.sqrt(d2)
            // Two candidate centres either side of the chord.
            var sign = (clockwise === major) ? 1 : -1
            var ccx = mx + sign * h * (-dy / len)
            var ccy = my + sign * h * (dx / len)
            // Canvas angles are measured in screen space (y down).
            var a0 = Math.atan2(-(y0 - ccy), x0 - ccx)
            var a1 = Math.atan2(-(y1 - ccy), x1 - ccx)
            ctx.arc(X(ccx), Y(ccy), rr * k, a0, a1, !clockwise)
        }
    }
}
