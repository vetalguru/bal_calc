import QtQuick
import QtQuick.Controls
import QtTest
import BalCalc

// End-to-end through the screens' own code paths: create a profile in the
// editor, get a solution and a range table, add a library bullet with BC
// bands, switch the profile to it, export and import the profile. Test
// functions run in alphabetical order and share the scratch database.
TestCase {
    id: testCase
    name: "Flow"
    when: windowShown
    visible: true // children must be visible to load and take input
    width: 900
    height: 700

    Component { id: editorComponent; ProfileEditor {} }
    Component { id: bulletEditorComponent; BulletEditor {} }
    Component { id: tableComponent; TablePage {} }
    Component { id: mainComponent; Main {} }

    function test_1_createProfileInEditor() {
        compare(Backend.profiles.length, 0)
        compare(Backend.solution.ok, false)

        var editor = createTemporaryObject(editorComponent, testCase,
                                           { form: Backend.profileForm(0), width: 900, height: 700 })
        var f = editor.form
        f.name = "Test .308"
        f.caliber = ".308 Win"
        f.bulletName = "SMK 175"
        f.dragTable = "G7"
        f.bc = 0.243
        f.massGr = 175
        f.diameterIn = 0.308
        f.lengthIn = 1.24
        f.muzzleVelocity = 790
        f.clickUnits = "mrad"
        f.clickValue = 0.1
        var doneSpy = createTemporaryObject(spyComponent, testCase, { target: editor, signalName: "done" })
        editor.save()
        compare(doneSpy.count, 1)
        compare(Backend.profiles.length, 1)
        compare(Backend.profiles[0].name, "Test .308")
        compare(Backend.currentProfileId, Backend.profiles[0].id)
    }

    function test_2_solutionForTheTarget() {
        Backend.targetRangeM = 600
        tryVerify(function() { return Backend.solution.ok && Backend.solution.rangeM === 600 })
        verify(Backend.solution.elevation > 3 && Backend.solution.elevation < 8)
        compare(Backend.solution.hasScope, true)
        verify(Math.abs(Backend.solution.elevationClicks - Backend.solution.elevation * 10) <= 1)
    }

    function test_3_rangeTable() {
        Backend.tableFromM = 0
        Backend.tableToM = 1000
        Backend.tableStepM = 100
        var t = Backend.rangeTable()
        verify(t.ok)
        compare(t.rows.length, 11)
        fuzzyCompare(t.rows[6].elevation, Backend.solution.elevation, 1e-9)

        var page = createTemporaryObject(tableComponent, testCase, { width: 900, height: 700 })
        tryVerify(function() { return page.table.ok && page.table.rows.length === 11 })
        verify(page.curve.rows.length > 100)
    }

    function test_4_libraryBulletWithBands() {
        var editor = createTemporaryObject(bulletEditorComponent, testCase,
                                           { form: Backend.bulletForm(0), width: 900, height: 700 })
        editor.form.name = "MatchKing 175 HPBT"
        editor.form.manufacturer = "Sierra"
        editor.form.caliber = ".308"
        editor.form.massGr = 175
        editor.form.diameterIn = 0.308
        editor.form.lengthIn = 1.24
        editor.form.dragTable = "G1"
        editor.banded = true
        editor.bands = [{ velocity: 869, bc: 0.505 }, { velocity: 701, bc: 0.496 }, { velocity: 457, bc: 0.485 }]
        editor.save()
        var list = Backend.libraryBullets("matchking")
        compare(list.length, 1)
        compare(list[0].bcBands, 3)
        compare(list[0].dragKind, "multi_bc")
    }

    function test_5_profileUsesTheLibraryBullet() {
        var before = Backend.solution.elevation
        var bullet = Backend.libraryBullets("matchking")[0]
        var form = Backend.profileFormWithBullet(Backend.profileForm(Backend.currentProfileId), bullet.id)
        compare(form.libraryBulletId, bullet.id)
        compare(Backend.saveProfile(form), "")
        compare(Backend.profileForm(Backend.currentProfileId).libraryBulletId, bullet.id)
        tryVerify(function() { return Backend.solution.ok && Backend.solution.elevation !== before })
        // The library bullet cannot go while the profile uses it.
        verify(Backend.deleteBullet(bullet.id).length > 0)
    }

    function test_6_exportImportViaClipboard() {
        var id = Backend.currentProfileId
        compare(Backend.copyProfileToClipboard(id), "")
        compare(Backend.importProfileFromClipboard(), "")
        compare(Backend.profiles.length, 2)
        verify(Backend.currentProfileId !== id)
        // The identical library bullet is reused, not duplicated.
        compare(Backend.libraryBullets("").length, 1)
        verify(Backend.profileFileName(id).endsWith(".balcalc.json"))
    }

    function test_7_mainWindowLoadsAllPages() {
        var win = createTemporaryObject(mainComponent, testCase)
        verify(win)
        for (var p = 0; p < 5; ++p) {
            win.page = p
            wait(50)
        }
        win.tableTab = 1
        wait(50)
        win.close()
    }

    Component { id: spyComponent; SignalSpy {} }
}
