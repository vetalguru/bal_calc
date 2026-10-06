pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import BalCalc

// Rifles and cartridges: two lists to choose from, create, edit, delete and
// share; entry point to the bullet library and the shot log.
Page {
    id: page

    // Sized by the parent layout; a fixed implicit size avoids a binding
    // loop through the scrolled content's width.
    implicitWidth: 360
    implicitHeight: 640

    // A cartridge was chosen: the solution can be shown.
    signal chosen()

    property alias tab: tabs.currentIndex
    readonly property bool rifleTab: tabs.currentIndex === 0

    // Pops an inner page (editor, library, log); false when at the lists.
    function back() {
        if (stack.depth > 1) {
            stack.pop()
            return true
        }
        return false
    }

    function editRifle(id) {
        stack.push(rifleEditorComponent, { form: Backend.rifleForm(id) })
    }
    function editCartridge(form) {
        stack.push(cartridgeEditorComponent, { form: form, stack: stack })
    }
    function copyFactoryCartridge() {
        var library = stack.push(cartridgeLibraryComponent)
        library.picked.connect(function(id) {
            stack.pop()
            page.editCartridge(Backend.cartridgeFormFromLibrary(id))
        })
    }

    function notify(error, success) {
        var text = error.length > 0 ? error : success
        if (text.length === 0)
            return
        toast.text = text
        toast.error = error.length > 0
        toast.open()
    }

    StackView {
        id: stack
        anchors.fill: parent

        initialItem: Page {
            header: ColumnLayout {
                spacing: 0
                ToolBar {
                    Layout.fillWidth: true
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 8
                        // The tabs below already say it on a phone.
                        Label {
                            text: page.width >= 520 ? qsTr("Rifles and cartridges") : ""
                            font.pixelSize: 18
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Button {
                            text: qsTr("Bullets")
                            flat: true
                            onClicked: stack.push(libraryComponent, { stack: stack })
                        }
                        Button {
                            id: importButton
                            text: qsTr("Import")
                            flat: true
                            onClicked: importMenu.open()
                            Menu {
                                id: importMenu
                                y: importButton.height
                                MenuItem {
                                    text: qsTr("From file…")
                                    onTriggered: importDialog.open()
                                }
                                MenuItem {
                                    text: qsTr("From clipboard")
                                    onTriggered: page.notify(Backend.importSharedFromClipboard(),
                                                             qsTr("Imported."))
                                }
                            }
                        }
                        Button {
                            id: newButton
                            text: qsTr("New")
                            highlighted: true
                            onClicked: page.rifleTab ? page.editRifle(0) : newMenu.open()
                            Menu {
                                id: newMenu
                                y: newButton.height
                                MenuItem {
                                    text: qsTr("Empty cartridge")
                                    onTriggered: page.editCartridge(Backend.cartridgeForm(0))
                                }
                                MenuItem {
                                    text: qsTr("Copy a factory cartridge…")
                                    onTriggered: page.copyFactoryCartridge()
                                }
                            }
                        }
                    }
                }
                TabBar {
                    id: tabs
                    Layout.fillWidth: true
                    TabButton { text: qsTr("Rifles (%1)").arg(Backend.rifles.length) }
                    TabButton { text: qsTr("Cartridges (%1)").arg(Backend.cartridges.length) }
                }
            }

            StackLayout {
                anchors.fill: parent
                currentIndex: tabs.currentIndex

                // Rifles
                Item {
                    Label {
                        anchors.centerIn: parent
                        visible: Backend.rifles.length === 0
                        width: parent.width - 48
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        text: qsTr("No rifles yet. A rifle holds its scope and its zero; tap New to add one.")
                        opacity: 0.7
                    }
                    ListView {
                        anchors.fill: parent
                        clip: true
                        model: Backend.rifles
                        delegate: ItemDelegate {
                            id: rifleRow
                            required property var modelData
                            width: ListView.view.width
                            highlighted: rifleRow.modelData.id === Backend.currentRifleId
                            contentItem: RowLayout {
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Label {
                                        text: rifleRow.modelData.name
                                        font.pixelSize: 16
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: rifleRow.modelData.caliber
                                        visible: text.length > 0
                                        opacity: 0.6
                                    }
                                }
                                Button {
                                    text: qsTr("Edit")
                                    flat: true
                                    onClicked: page.editRifle(rifleRow.modelData.id)
                                }
                                ItemMenu {
                                    kind: "rifle"
                                    itemId: rifleRow.modelData.id
                                    itemName: rifleRow.modelData.name
                                }
                            }
                            onClicked: {
                                Backend.currentRifleId = rifleRow.modelData.id
                                tabs.currentIndex = 1 // now the cartridge
                            }
                        }
                    }
                }

                // Cartridges
                Item {
                    Label {
                        anchors.centerIn: parent
                        visible: Backend.cartridges.length === 0
                        width: parent.width - 48
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        text: qsTr("No cartridges yet. Tap New to describe your load or copy a factory one.")
                        opacity: 0.7
                    }
                    ListView {
                        anchors.fill: parent
                        clip: true
                        model: Backend.cartridges
                        delegate: ItemDelegate {
                            id: cartRow
                            required property var modelData
                            width: ListView.view.width
                            highlighted: cartRow.modelData.id === Backend.currentCartridgeId
                            contentItem: RowLayout {
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    // Another calibre than the current rifle's: dimmed.
                                    opacity: cartRow.modelData.matches || Backend.rifles.length === 0 ? 1.0 : 0.55
                                    Label {
                                        text: cartRow.modelData.name
                                        font.pixelSize: 16
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        opacity: 0.6
                                        text: [cartRow.modelData.caliber, cartRow.modelData.bulletName,
                                               qsTr("%1 m/s").arg(Math.round(cartRow.modelData.muzzleVelocity))]
                                              .filter(s => s.length > 0).join(" · ")
                                    }
                                }
                                Button {
                                    text: qsTr("Edit")
                                    flat: true
                                    onClicked: page.editCartridge(Backend.cartridgeForm(cartRow.modelData.id))
                                }
                                ItemMenu {
                                    kind: "cartridge"
                                    itemId: cartRow.modelData.id
                                    itemName: cartRow.modelData.name
                                }
                            }
                            onClicked: {
                                Backend.currentCartridgeId = cartRow.modelData.id
                                page.chosen()
                            }
                        }
                    }
                }
            }
        }
    }

    // The ⋮ menu of a rifle or cartridge row.
    component ItemMenu: Button {
        id: more
        property string kind
        property int itemId
        property string itemName
        text: "⋮"
        flat: true
        onClicked: menu.open()
        Menu {
            id: menu
            y: more.height
            MenuItem {
                visible: more.kind === "cartridge"
                height: visible ? implicitHeight : 0
                text: qsTr("Shot log and truing")
                onTriggered: {
                    Backend.currentCartridgeId = more.itemId
                    stack.push(truingComponent)
                }
            }
            MenuItem {
                text: qsTr("Export to file…")
                onTriggered: {
                    exportDialog.kind = more.kind
                    exportDialog.itemId = more.itemId
                    exportDialog.selectedFile = Backend.exportFileName(more.kind, more.itemId)
                    exportDialog.open()
                }
            }
            MenuItem {
                text: qsTr("Copy to clipboard")
                onTriggered: page.notify(Backend.copyItemToClipboard(more.kind, more.itemId),
                                         qsTr("Copied. Paste it into a message or a file."))
            }
            MenuItem {
                text: qsTr("Delete")
                onTriggered: {
                    confirmDelete.kind = more.kind
                    confirmDelete.itemId = more.itemId
                    confirmDelete.itemName = more.itemName
                    confirmDelete.open()
                }
            }
        }
    }

    Component {
        id: rifleEditorComponent
        RifleEditor {
            onDone: stack.pop()
        }
    }

    Component {
        id: cartridgeEditorComponent
        CartridgeEditor {
            onDone: stack.pop()
        }
    }

    Component {
        id: cartridgeLibraryComponent
        CartridgeLibraryPage {
            onClosed: stack.pop()
        }
    }

    Component {
        id: truingComponent
        TruingPage {
            onClosed: stack.pop()
        }
    }

    Component {
        id: libraryComponent
        LibraryPage {
            onClosed: stack.pop()
        }
    }

    FileDialog {
        id: importDialog
        title: qsTr("Import a rifle or a cartridge")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("BalCalc files (*.json)"), qsTr("All files (*)")]
        onAccepted: page.notify(Backend.importShared(selectedFile), qsTr("Imported."))
    }

    FileDialog {
        id: exportDialog
        property string kind
        property int itemId: 0
        title: kind === "rifle" ? qsTr("Export rifle") : qsTr("Export cartridge")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("BalCalc files (*.json)")]
        onAccepted: page.notify(Backend.exportItem(kind, itemId, selectedFile), qsTr("Saved."))
    }

    Dialog {
        id: confirmDelete
        property string kind
        property int itemId: 0
        property string itemName
        anchors.centerIn: parent
        width: Math.min(page.width - 32, 420)
        modal: true
        title: kind === "rifle" ? qsTr("Delete rifle?") : qsTr("Delete cartridge?")
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            // From the page, not the dialog: no loop through its implicit size.
            width: Math.min(page.width - 32, 420) - confirmDelete.leftPadding - confirmDelete.rightPadding
            wrapMode: Text.Wrap
            text: confirmDelete.itemName + "\n\n" +
                  (confirmDelete.kind === "rifle"
                   ? qsTr("Its shot logs and truing with every cartridge are deleted too.")
                   : qsTr("Its shot logs and truing with every rifle are deleted too."))
        }
        onAccepted: page.notify(kind === "rifle" ? Backend.deleteRifle(itemId)
                                                 : Backend.deleteCartridge(itemId), "")
    }

    Popup {
        id: toast
        property string text
        property bool error: false
        x: (page.width - width) / 2
        y: page.height - height - 24
        width: Math.min(page.width - 32, 480)
        padding: 12
        closePolicy: Popup.CloseOnPressOutside
        onOpened: hideTimer.restart()
        Timer {
            id: hideTimer
            interval: 3500
            onTriggered: toast.close()
        }
        background: Rectangle {
            radius: 6
            color: toast.error ? Material.color(Material.Red, Material.Shade700) : "#323232"
        }
        contentItem: Label {
            text: toast.text
            color: "white"
            wrapMode: Text.Wrap
        }
    }
}
