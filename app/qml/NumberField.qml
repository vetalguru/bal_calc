import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Labelled numeric input with a unit. Accepts "," or "." as the decimal
// separator; emits edited(value) when the user finishes typing.
ColumnLayout {
    id: root

    property string label
    property string unit
    property real value
    property int decimals: 1
    property real from: -1e9
    property real to: 1e9

    signal edited(real value)

    spacing: 2

    function format(v) {
        if (isNaN(v))
            return ""
        var s = Number(v).toFixed(root.decimals)
        if (s.indexOf(".") >= 0)
            s = s.replace(/0+$/, "").replace(/\.$/, "")
        return s
    }

    Label {
        text: root.label
        font.pixelSize: 12
        opacity: 0.7
        Layout.fillWidth: true
        elide: Text.ElideRight
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        TextField {
            id: field
            Layout.fillWidth: true
            Layout.minimumWidth: 52
            text: root.format(root.value)
            selectByMouse: true
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: RegularExpressionValidator { regularExpression: /^-?[0-9]*[.,]?[0-9]*$/ }
            onEditingFinished: {
                var v = parseFloat(text.replace(",", "."))
                // Re-bind: typing replaced the text, show the model value again.
                text = Qt.binding(function() { return root.format(root.value) })
                if (isNaN(v))
                    return
                v = Math.min(root.to, Math.max(root.from, v))
                if (v !== root.value) {
                    root.edited(v)
                    // Models without change signals (plain JS objects) do
                    // not push the value back; keep what was typed.
                    if (root.value !== v)
                        root.value = v
                }
            }
            onActiveFocusChanged: if (activeFocus) selectAll()
        }

        Label {
            visible: root.unit.length > 0
            text: root.unit
            opacity: 0.8
            Layout.minimumWidth: implicitWidth
        }
    }
}
