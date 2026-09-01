package com.fireball.mini.core.engine

import android.view.KeyEvent

enum class BrowserShortcutAction {
    NEW_TAB,
    CLOSE_TAB,
    REOPEN_CLOSED_TAB,
    NEXT_TAB,
    PREV_TAB,
    SELECT_TAB_1,
    SELECT_TAB_2,
    SELECT_TAB_3,
    SELECT_TAB_4,
    SELECT_TAB_5,
    SELECT_TAB_6,
    SELECT_TAB_7,
    SELECT_TAB_8,
    SELECT_TAB_9,
    RELOAD,
    FORCE_RELOAD,
    FOCUS_OMNIBOX,
    BOOKMARK_PAGE,
    OPEN_HISTORY,
    OPEN_DOWNLOADS,
    OPEN_FIND_IN_PAGE,
    NEW_INCOGNITO,
    NAVIGATE_BACK,
    NAVIGATE_FORWARD,
    ZOOM_IN,
    ZOOM_OUT,
    ZOOM_RESET
}

object DesktopKeyShortcutHandler {

    /**
     * Translates an Android KeyEvent into a high-level browser action.
     * Returns null if no shortcut was matched.
     */
    fun handleKeyEvent(event: KeyEvent): BrowserShortcutAction? {
        val meta = event.metaState
        val isCtrlOrCmd = event.isCtrlPressed || event.isMetaPressed || (meta and (KeyEvent.META_CTRL_ON or KeyEvent.META_META_ON)) != 0
        val isShift = event.isShiftPressed || (meta and KeyEvent.META_SHIFT_ON) != 0
        val isAlt = event.isAltPressed || (meta and KeyEvent.META_ALT_ON) != 0
        val keyCode = event.keyCode

        return resolveShortcut(keyCode, isCtrlOrCmd, isShift, isAlt)
    }

    fun resolveShortcut(keyCode: Int, isCtrlOrCmd: Boolean, isShift: Boolean, isAlt: Boolean): BrowserShortcutAction? {
        // Ctrl / Cmd combinations
        if (isCtrlOrCmd) {
            return when (keyCode) {
                KeyEvent.KEYCODE_T -> if (isShift) BrowserShortcutAction.REOPEN_CLOSED_TAB else BrowserShortcutAction.NEW_TAB
                KeyEvent.KEYCODE_W -> BrowserShortcutAction.CLOSE_TAB
                KeyEvent.KEYCODE_TAB -> if (isShift) BrowserShortcutAction.PREV_TAB else BrowserShortcutAction.NEXT_TAB
                KeyEvent.KEYCODE_PAGE_DOWN -> BrowserShortcutAction.NEXT_TAB
                KeyEvent.KEYCODE_PAGE_UP -> BrowserShortcutAction.PREV_TAB
                KeyEvent.KEYCODE_R -> if (isShift) BrowserShortcutAction.FORCE_RELOAD else BrowserShortcutAction.RELOAD
                KeyEvent.KEYCODE_L -> BrowserShortcutAction.FOCUS_OMNIBOX
                KeyEvent.KEYCODE_D -> BrowserShortcutAction.BOOKMARK_PAGE
                KeyEvent.KEYCODE_H -> BrowserShortcutAction.OPEN_HISTORY
                KeyEvent.KEYCODE_J -> BrowserShortcutAction.OPEN_DOWNLOADS
                KeyEvent.KEYCODE_F -> BrowserShortcutAction.OPEN_FIND_IN_PAGE
                KeyEvent.KEYCODE_N -> if (isShift) BrowserShortcutAction.NEW_INCOGNITO else BrowserShortcutAction.NEW_TAB
                KeyEvent.KEYCODE_1 -> BrowserShortcutAction.SELECT_TAB_1
                KeyEvent.KEYCODE_2 -> BrowserShortcutAction.SELECT_TAB_2
                KeyEvent.KEYCODE_3 -> BrowserShortcutAction.SELECT_TAB_3
                KeyEvent.KEYCODE_4 -> BrowserShortcutAction.SELECT_TAB_4
                KeyEvent.KEYCODE_5 -> BrowserShortcutAction.SELECT_TAB_5
                KeyEvent.KEYCODE_6 -> BrowserShortcutAction.SELECT_TAB_6
                KeyEvent.KEYCODE_7 -> BrowserShortcutAction.SELECT_TAB_7
                KeyEvent.KEYCODE_8 -> BrowserShortcutAction.SELECT_TAB_8
                KeyEvent.KEYCODE_9 -> BrowserShortcutAction.SELECT_TAB_9
                KeyEvent.KEYCODE_PLUS, KeyEvent.KEYCODE_EQUALS -> BrowserShortcutAction.ZOOM_IN
                KeyEvent.KEYCODE_MINUS -> BrowserShortcutAction.ZOOM_OUT
                KeyEvent.KEYCODE_0 -> BrowserShortcutAction.ZOOM_RESET
                else -> null
            }
        }

        // Alt combinations
        if (isAlt) {
            return when (keyCode) {
                KeyEvent.KEYCODE_D -> BrowserShortcutAction.FOCUS_OMNIBOX
                KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_LEFT_BRACKET -> BrowserShortcutAction.NAVIGATE_BACK
                KeyEvent.KEYCODE_DPAD_RIGHT, KeyEvent.KEYCODE_RIGHT_BRACKET -> BrowserShortcutAction.NAVIGATE_FORWARD
                else -> null
            }
        }

        // Function keys
        return when (keyCode) {
            KeyEvent.KEYCODE_F5 -> if (isCtrlOrCmd) BrowserShortcutAction.FORCE_RELOAD else BrowserShortcutAction.RELOAD
            KeyEvent.KEYCODE_FORWARD -> BrowserShortcutAction.NAVIGATE_FORWARD
            else -> null
        }
    }
}
