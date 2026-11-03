package org.vetalguru.balcalc

import java.awt.datatransfer.Clipboard as AwtClipboard
import java.io.File
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlinx.coroutines.runBlocking
import org.vetalguru.balcalc.core.desktopDatabasePath
import org.vetalguru.balcalc.core.resourcesDir

/** The desktop (Linux and Windows) platform: data folders, files through the dialog, the clipboard. */
class DesktopPlatformTest {
    private val dir = File.createTempFile("holdmark-desktop", "").apply { delete(); mkdirs() }

    @AfterTest
    fun cleanUp() {
        dir.deleteRecursively()
    }

    private fun env(vararg pairs: Pair<String, String>): (String) -> String? = mapOf(*pairs)::get

    private fun linux(home: File) = env("os.name" to "Linux", "user.home" to home.path)

    // ---- Where the data lives ----------------------------------------------

    /** Linux: XDG_DATA_HOME when it is an absolute path, else ~/.local/share. */
    @Test
    fun linuxDataFolderFollowsXdg() {
        val home = File(dir, "home")
        val xdg = File(dir, "xdg")
        assertEquals(File(xdg, "vetalguru/Holdmark/holdmark.db").path,
            desktopDatabasePath(env("XDG_DATA_HOME" to xdg.path), linux(home)))
        assertTrue(File(xdg, "vetalguru/Holdmark").isDirectory) // made for the database
        val default = File(home, ".local/share/vetalguru/Holdmark/holdmark.db").path
        assertEquals(default, desktopDatabasePath(env(), linux(home)))
        // The XDG spec: empty or relative values are ignored, not taken from the working directory.
        assertEquals(default, desktopDatabasePath(env("XDG_DATA_HOME" to ""), linux(home)))
        assertEquals(default, desktopDatabasePath(env("XDG_DATA_HOME" to "relative/data"), linux(home)))
    }

    /** Windows: %APPDATA%, whatever XDG says. */
    @Test
    fun windowsDataFolderIsAppData() {
        val appData = File(dir, "AppData/Roaming")
        val windows = env("os.name" to "Windows 11", "user.home" to dir.path)
        assertEquals(File(appData, "vetalguru/Holdmark/holdmark.db").path,
            desktopDatabasePath(env("APPDATA" to appData.path, "XDG_DATA_HOME" to dir.path), windows))
    }

    /** HOLDMARK_DB, or the BalCalc-era BALCALC_DB, names the file outright; nothing is made. */
    @Test
    fun databaseOverrides() {
        val home = File(dir, "home")
        assertEquals("/data/a.db", desktopDatabasePath(env("HOLDMARK_DB" to "/data/a.db", "BALCALC_DB" to "/b.db"), linux(home)))
        assertEquals("/b.db", desktopDatabasePath(env("BALCALC_DB" to "/b.db"), linux(home)))
        assertFalse(home.exists())
    }

    /** The resources: BALCALC_RESOURCES first, then Compose's property; neither is an error. */
    @Test
    fun resourcesFolder() {
        val prop = env("compose.application.resources.dir" to "/opt/holdmark/lib/app/resources")
        assertEquals(File("/tests/res"), resourcesDir(env("BALCALC_RESOURCES" to "/tests/res"), prop))
        assertEquals(File("/opt/holdmark/lib/app/resources"), resourcesDir(env(), prop))
        assertFailsWith<IllegalStateException> { resourcesDir(env(), env()) }
    }

    // ---- Files through the dialog -------------------------------------------

    /** Stands in for the native dialog: what the user would pick, and what was asked. */
    private inner class Picks(var save: File? = null, var open: List<File> = emptyList()) : FileChooser {
        var suggested: String? = null
        var multiple: Boolean? = null
        var accept: ((String) -> Boolean)? = null

        override fun save(suggestedName: String): File? {
            suggested = suggestedName
            return save
        }

        override fun open(multiple: Boolean, accept: ((String) -> Boolean)?): List<File> {
            this.multiple = multiple
            this.accept = accept
            return open
        }
    }

    private fun platform(picks: Picks, clipboard: AwtClipboard? = null) = DesktopPlatform({ null }, picks, clipboard)

    /** Save: the suggested name goes to the dialog, the text or bytes to the chosen file (replaced). */
    @Test
    fun savesWhereTheUserChose() = runBlocking {
        val target = File(dir, "out.json").apply { writeText("an older and longer file") }
        val picks = Picks(save = target)
        assertTrue(platform(picks).saveText("rifle.holdmark.json", "{}"))
        assertEquals("rifle.holdmark.json", picks.suggested)
        assertEquals("{}", target.readText())

        val png = byteArrayOf(-119, 80, 78, 71)
        assertTrue(platform(picks).saveBytes("table.png", "image/png", png))
        assertContentEquals(png, target.readBytes())
    }

    /** Cancelled save dialogs write nothing. */
    @Test
    fun cancelledSaveWritesNothing() = runBlocking {
        val picks = Picks(save = null)
        assertFalse(platform(picks).saveText("a.csv", "x"))
        assertFalse(platform(picks).saveBytes("a.png", "image/png", byteArrayOf(1)))
        assertTrue(dir.listFiles().orEmpty().isEmpty())
    }

    /** Open: one or several files, read with their names; the dialog filters by extension, any case. */
    @Test
    fun opensTextFiles() = runBlocking {
        val a = File(dir, "a.ammo").apply { writeText("<ammo/>") }
        val b = File(dir, "b.DRG").apply { writeText("drag") }
        val picks = Picks(open = listOf(a, b))
        val read = platform(picks).openTexts(listOf("ammo", "drg"), multiple = true)
        assertEquals(listOf("a.ammo" to "<ammo/>", "b.DRG" to "drag"), read.map { it.name to it.content })
        assertEquals(true, picks.multiple)
        val accept = picks.accept!!
        assertTrue(accept("x.ammo") && accept("X.AMMO") && accept("y.drg"))
        assertFalse(accept("x.reticle") || accept("ammo") || accept("x.ammo.txt"))

        // No extensions: no filter; one file; cancelled: nothing.
        platform(picks).openTexts(emptyList(), multiple = false)
        assertNull(picks.accept)
        assertEquals(false, picks.multiple)
        assertEquals(emptyList(), platform(Picks()).openTexts(listOf("json"), multiple = false))
    }

    /** A picture: only image files offered; its bytes come back as they are. */
    @Test
    fun opensAPicture() = runBlocking {
        val photo = File(dir, "group.JPG").apply { writeBytes(byteArrayOf(-1, -40, -1)) }
        val picks = Picks(open = listOf(photo))
        assertContentEquals(byteArrayOf(-1, -40, -1), platform(picks).openImage())
        assertEquals(false, picks.multiple)
        val accept = picks.accept!!
        for (name in listOf("a.jpg", "a.jpeg", "a.png", "a.webp", "a.bmp", "A.PNG")) assertTrue(accept(name), name)
        for (name in listOf("a.gif", "a.json", "png")) assertFalse(accept(name), name)
        assertNull(platform(Picks()).openImage())
    }

    /** The clipboard: copy then paste; something other than text pastes as nothing. */
    @Test
    fun clipboard() = runBlocking {
        val board = AwtClipboard("test")
        val p = platform(Picks(), board)
        assertNull(p.pasteText()) // empty
        p.copyText("holdmark:1/1:abc")
        assertEquals("holdmark:1/1:abc", p.pasteText())
        board.setContents(object : java.awt.datatransfer.Transferable {
            override fun getTransferDataFlavors() = arrayOf(java.awt.datatransfer.DataFlavor.imageFlavor)
            override fun isDataFlavorSupported(f: java.awt.datatransfer.DataFlavor) = f == java.awt.datatransfer.DataFlavor.imageFlavor
            override fun getTransferData(f: java.awt.datatransfer.DataFlavor): Any =
                throw java.awt.datatransfer.UnsupportedFlavorException(f)
        }, null)
        assertNull(p.pasteText())
    }
}
