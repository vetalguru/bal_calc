import QtQuick
import QtQuick.Controls
import QtTest
import BalCalc

// End-to-end through the screens' own code paths: create a rifle and a
// cartridge in their editors, get a solution and a range table, add a
// library bullet with BC bands, switch the cartridge to it, share both,
// use the cartridge in a second rifle. Test functions run in alphabetical
// order and share the scratch database.
TestCase {
    id: testCase
    name: "Flow"
    when: windowShown
    visible: true // children must be visible to load and take input
    width: 900
    height: 700

    Component { id: rifleEditorComponent; RifleEditor {} }
    Component { id: cartridgeEditorComponent; CartridgeEditor {} }
    Component { id: bulletEditorComponent; BulletEditor {} }
    Component { id: tableComponent; TablePage {} }
    Component { id: mainComponent; Main {} }
    Component { id: truingComponent; TruingPage {} }
    Component { id: armoryComponent; ArmoryPage {} }

    function test_0_starterLibraryIsSeeded() {
        // 69 cartridges + 55 radar curves + 38 published bullets.
        compare(Backend.libraryBullets("").length, 162)
        verify(Backend.libraryBullets("Lapua").length >= 50)
        verify(Backend.seedReport.length > 0)
        // Factory cartridges stay in the library, the user's list is empty.
        compare(Backend.libraryCartridges("").length, 69)
        compare(Backend.cartridges.length, 0)
    }

    function test_1_createRifleAndCartridgeInEditors() {
        compare(Backend.rifles.length, 0)
        compare(Backend.solution.ok, false)

        var rifle = createTemporaryObject(rifleEditorComponent, testCase,
                                          { form: Backend.rifleForm(0), width: 900, height: 700 })
        var r = rifle.form
        r.name = "Test rifle"
        r.caliber = ".308 Win"
        r.clickUnits = "mrad"
        r.clickValue = 0.1
        var rifleDone = createTemporaryObject(spyComponent, testCase, { target: rifle, signalName: "done" })
        rifle.save()
        compare(rifleDone.count, 1)
        compare(Backend.rifles.length, 1)
        compare(Backend.currentRifleId, Backend.rifles[0].id)
        compare(Backend.currentProfileId, 0) // no cartridge yet

        var cartridge = createTemporaryObject(cartridgeEditorComponent, testCase,
                                              { form: Backend.cartridgeForm(0), width: 900, height: 700 })
        var c = cartridge.form
        c.name = "Test load"
        c.caliber = ".308 Win"
        c.bulletName = "SMK 175"
        c.dragTable = "G7"
        c.bc = 0.243
        c.massGr = 175
        c.diameterIn = 0.308
        c.lengthIn = 1.24
        c.muzzleVelocity = 790
        var cartDone = createTemporaryObject(spyComponent, testCase, { target: cartridge, signalName: "done" })
        cartridge.save()
        compare(cartDone.count, 1)
        compare(Backend.cartridges.length, 1)
        compare(Backend.cartridges[0].matches, true)
        compare(Backend.currentCartridgeId, Backend.cartridges[0].id)
        verify(Backend.currentProfileId > 0)
        compare(Backend.currentPair.rifleName, "Test rifle")
        compare(Backend.currentPair.cartridgeName, "Test load")
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
        editor.form.name = "Test bullet 175 HPBT"
        editor.form.manufacturer = "Sierra"
        editor.form.caliber = ".308"
        editor.form.massGr = 175
        editor.form.diameterIn = 0.308
        editor.form.lengthIn = 1.24
        editor.form.dragTable = "G1"
        editor.banded = true
        editor.bands = [{ velocity: 869, bc: 0.505 }, { velocity: 701, bc: 0.496 }, { velocity: 457, bc: 0.485 }]
        editor.save()
        var list = Backend.libraryBullets("Test bullet")
        compare(list.length, 1)
        compare(list[0].bcBands, 3)
        compare(list[0].dragKind, "multi_bc")
    }


    function test_5_cartridgeUsesTheLibraryBullet() {
        var before = Backend.solution.elevation
        var bullet = Backend.libraryBullets("Test bullet")[0]
        var form = Backend.cartridgeFormWithBullet(Backend.cartridgeForm(Backend.currentCartridgeId), bullet.id)
        compare(form.libraryBulletId, bullet.id)
        compare(Backend.saveCartridge(form), "")
        compare(Backend.cartridgeForm(Backend.currentCartridgeId).libraryBulletId, bullet.id)
        tryVerify(function() { return Backend.solution.ok && Backend.solution.elevation !== before })
        // The library bullet cannot go while the cartridge uses it.
        verify(Backend.deleteBullet(bullet.id).length > 0)
    }

    function test_6_shareRifleAndCartridgeViaClipboard() {
        var rifle = Backend.currentRifleId
        var cartridge = Backend.currentCartridgeId
        var bulletsBefore = Backend.libraryBullets("").length
        compare(Backend.copyItemToClipboard("cartridge", cartridge), "")
        compare(Backend.importSharedFromClipboard(), "")
        compare(Backend.cartridges.length, 2)
        verify(Backend.currentCartridgeId !== cartridge)
        // The identical library bullet is reused, not duplicated.
        compare(Backend.libraryBullets("").length, bulletsBefore)

        compare(Backend.copyItemToClipboard("rifle", rifle), "")
        compare(Backend.importSharedFromClipboard(), "")
        compare(Backend.rifles.length, 2)
        verify(Backend.currentRifleId !== rifle)
        verify(Backend.exportFileName("rifle", rifle).endsWith(".balcalc.json"))

        // Back to the first pair; drop the copies.
        Backend.currentRifleId = rifle
        Backend.currentCartridgeId = cartridge
        compare(Backend.deleteRifle(Backend.rifles.filter(r => r.id !== rifle)[0].id), "")
        compare(Backend.deleteCartridge(Backend.cartridges.filter(c => c.id !== cartridge)[0].id), "")
        compare(Backend.currentRifleId, rifle)
        compare(Backend.currentCartridgeId, cartridge)
    }

    function test_6b_oneCartridgeInTwoRifles() {
        var first = Backend.currentRifleId
        var firstPair = Backend.currentProfileId
        var form = Backend.rifleForm(0)
        form.name = "Second rifle"
        form.caliber = ".308"
        form.zeroRangeM = 300
        compare(Backend.saveRifle(form), "")
        compare(Backend.rifles.length, 2)
        var pair = Backend.currentProfileId
        verify(pair > 0 && pair !== firstPair)
        compare(Backend.currentPair.zeroRangeM, 300)
        // The cartridge kept: the same load, now in the second rifle.
        compare(Backend.currentPair.cartridgeName, "Test load")
        Backend.currentRifleId = first
        compare(Backend.currentProfileId, firstPair)
    }

    function test_6c_pointOfImpactShift() {
        Backend.targetRangeM = 100
        tryVerify(function() { return Backend.solution.ok && Backend.solution.rangeM === 100 })
        var before = Backend.solution.elevation
        var page = createTemporaryObject(truingComponent, testCase, { width: 900, height: 700 })
        var up = findChild(page, "offsetUp")
        verify(up)
        up.edited(3) // hits 3 cm high at the 100 m zero
        compare(page.offsetError, "")
        compare(Backend.currentPair.offsetUpCm, 3)
        tryVerify(function() { return Math.abs(Backend.solution.elevation - (before - 0.3)) < 0.02 })
        compare(Backend.setZeroOffset(0, 0), "")
    }

    function test_6d_armoryLists() {
        var page = createTemporaryObject(armoryComponent, testCase, { width: 900, height: 700 })
        verify(page)
        page.tab = 1
        wait(50)
        page.copyFactoryCartridge()
        wait(50)
        verify(page.back()) // closes the factory list
        verify(!page.back())
    }


    function test_7_holdModes() {
        Backend.targetRangeM = 700
        Backend.holdMode = "hold"
        tryVerify(function() { return Backend.solution.ok && Backend.solution.holdMode === "hold" })
        var s = Backend.solution
        compare(s.dialElevationClicks, 0)
        fuzzyCompare(s.targetY, -s.elevation, 1e-9) // MRAD, FFP
        fuzzyCompare(s.targetX, -s.windage, 1e-9)

        Backend.holdMode = "dial_elevation"
        tryVerify(function() { return Backend.solution.holdMode === "dial_elevation" })
        s = Backend.solution
        compare(s.dialElevationClicks, s.elevationClicks)
        verify(Math.abs(s.targetY) <= 0.05) // only the click remainder is held
        Backend.holdMode = "hold"
    }

    function test_7b_logHitsAndTrue() {
        Backend.holdMode = "dial_elevation"
        // Pretend the rifle needs 4 % more elevation than predicted at two ranges.
        var ranges = [500, 900]
        for (var i = 0; i < ranges.length; ++i) {
            Backend.targetRangeM = ranges[i]
            tryVerify(function() { return Backend.solution.ok && Backend.solution.rangeM === ranges[i] })
            compare(Backend.logShot(ranges[i], Backend.solution.elevation * 1.04, false, 0, "test"), "")
        }
        compare(Backend.shots().length, 2)
        var page = createTemporaryObject(truingComponent, testCase, { width: 900, height: 700 })
        compare(page.shots.length, 2)
        var r = Backend.computeTruing()
        verify(r.ok, r.error)
        verify(r.rmsAfter < r.rmsBefore / 3)
        verify(r.velocityAfter < r.velocityBefore) // more drop = slower bullet
        compare(Backend.applyTruing(), "")
        tryVerify(function() { return Backend.solution.ok && Backend.solution.velocityScale < 1 })
        compare(Backend.resetTruing(), "")
        tryVerify(function() { return Backend.solution.velocityScale === 1 })
    }

    function test_8_mainWindowLoadsAllPages() {
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
