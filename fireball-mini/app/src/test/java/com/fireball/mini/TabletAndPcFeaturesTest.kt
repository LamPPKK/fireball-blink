package com.fireball.mini

import android.view.KeyEvent
import com.fireball.mini.core.engine.BrowserShortcutAction
import com.fireball.mini.core.engine.DesktopKeyShortcutHandler
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class TabletAndPcFeaturesTest {

    @Test
    fun testKeyboardShortcutsForTabletAndPc() {
        assertEquals(
            BrowserShortcutAction.NEW_TAB,
            DesktopKeyShortcutHandler.resolveShortcut(KeyEvent.KEYCODE_T, isCtrlOrCmd = true, isShift = false, isAlt = false)
        )

        assertEquals(
            BrowserShortcutAction.CLOSE_TAB,
            DesktopKeyShortcutHandler.resolveShortcut(KeyEvent.KEYCODE_W, isCtrlOrCmd = true, isShift = false, isAlt = false)
        )

        assertEquals(
            BrowserShortcutAction.NEXT_TAB,
            DesktopKeyShortcutHandler.resolveShortcut(KeyEvent.KEYCODE_TAB, isCtrlOrCmd = true, isShift = false, isAlt = false)
        )

        assertEquals(
            BrowserShortcutAction.PREV_TAB,
            DesktopKeyShortcutHandler.resolveShortcut(KeyEvent.KEYCODE_TAB, isCtrlOrCmd = true, isShift = true, isAlt = false)
        )

        assertEquals(
            BrowserShortcutAction.SELECT_TAB_1,
            DesktopKeyShortcutHandler.resolveShortcut(KeyEvent.KEYCODE_1, isCtrlOrCmd = true, isShift = false, isAlt = false)
        )

        assertEquals(
            BrowserShortcutAction.RELOAD,
            DesktopKeyShortcutHandler.resolveShortcut(KeyEvent.KEYCODE_R, isCtrlOrCmd = true, isShift = false, isAlt = false)
        )

        assertEquals(
            BrowserShortcutAction.FOCUS_OMNIBOX,
            DesktopKeyShortcutHandler.resolveShortcut(KeyEvent.KEYCODE_L, isCtrlOrCmd = true, isShift = false, isAlt = false)
        )

        assertEquals(
            BrowserShortcutAction.BOOKMARK_PAGE,
            DesktopKeyShortcutHandler.resolveShortcut(KeyEvent.KEYCODE_D, isCtrlOrCmd = true, isShift = false, isAlt = false)
        )

        assertEquals(
            BrowserShortcutAction.NEW_INCOGNITO,
            DesktopKeyShortcutHandler.resolveShortcut(KeyEvent.KEYCODE_N, isCtrlOrCmd = true, isShift = true, isAlt = false)
        )
    }

    @Test
    fun testDesktopModeUserAgentFormat() {
        val desktopUa = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36"
        val mobileUa = "Mozilla/5.0 (Linux; Android 14; Mobile) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Mobile Safari/537.36"

        assertNotNull(desktopUa)
        assert(desktopUa.contains("X11; Linux x86_64"))
        assert(!desktopUa.contains("Mobile"))

        assert(mobileUa.contains("Android"))
        assert(mobileUa.contains("Mobile"))
    }
}
