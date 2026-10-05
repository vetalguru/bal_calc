import QtQuick
import QtTest
import BalCalc

// NumberField: typed values are reported and stay visible even when the
// model does not push them back (plain JS objects, as in the editors).
TestCase {
    id: testCase
    name: "NumberField"
    when: windowShown
    visible: true // children must be visible to load and take input
    width: 400
    height: 200

    Component {
        id: fieldComponent
        NumberField {
            width: 300
            label: "Weight"
            unit: "gr"
            from: 0
            to: 500
        }
    }

    SignalSpy { id: editedSpy; signalName: "edited" }

    function typeInto(field, text) {
        mouseClick(field, field.width / 2, field.height - 10)
        keySequence(StandardKey.SelectAll)
        for (var i = 0; i < text.length; ++i)
            keyClick(text[i])
        keyClick(Qt.Key_Return)
    }

    function test_typedValueIsReportedAndKept() {
        var model = { weight: 0 }
        var field = createTemporaryObject(fieldComponent, testCase, { value: model.weight })
        editedSpy.target = field
        editedSpy.clear()
        field.edited.connect(function(v) { model.weight = v })
        typeInto(field, "175.5")
        compare(editedSpy.count, 1)
        compare(model.weight, 175.5)
        compare(field.value, 175.5)
    }

    function test_commaIsADecimalSeparatorAndRangeIsClamped() {
        var field = createTemporaryObject(fieldComponent, testCase, { value: 1 })
        editedSpy.target = field
        editedSpy.clear()
        typeInto(field, "0,308")
        compare(field.value, 0.308)
        typeInto(field, "9999")
        compare(field.value, 500)
        compare(editedSpy.count, 2)
    }

    function test_garbageKeepsTheOldValue() {
        var field = createTemporaryObject(fieldComponent, testCase, { value: 42 })
        editedSpy.target = field
        editedSpy.clear()
        typeInto(field, "-")
        compare(editedSpy.count, 0)
        compare(field.value, 42)
    }
}
