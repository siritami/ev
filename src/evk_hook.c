/**
 * evk_hook.c - Keyboard Hook (WH_KEYBOARD_LL)
 *
 * Installs a system-wide low-level keyboard hook that intercepts
 * keystrokes and routes them through the Vietnamese input engine.
 *
 * When Vietnamese mode is active and the foreground app is not
 * blacklisted, keystrokes are processed by the compose engine.
 * If the engine produces output, the original keystroke is suppressed
 * and replaced with the Vietnamese character.
 */

#include "evk.h"
#include "evk_table.h"

/* ========================================================================
 * Hook Install / Uninstall
 * ======================================================================== */

void hook_install(void)
{
    EvkApp *app = &g_app;

    if (app->hookKeyboard)
        return; /* Already installed */

    HMODULE hMod = GetModuleHandleW(NULL);
    app->hookKeyboard = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        hook_callback,
        hMod,
        0  /* Global hook */
    );
}

void hook_uninstall(void)
{
    EvkApp *app = &g_app;

    if (app->hookKeyboard) {
        UnhookWindowsHookEx(app->hookKeyboard);
        app->hookKeyboard = NULL;
    }
}

/* ========================================================================
 * Active Window Tracking
 *
 * Tracks which application currently has focus so we can apply
 * per-application blacklist overrides.
 * ======================================================================== */

static void update_active_window(void)
{
    EvkApp *app = &g_app;
    HWND hwnd = GetForegroundWindow();

    if (hwnd == app->hwndMain || hwnd == app->hwndTray)
        return;

    app->hwndActive = hwnd;

    /* Get the process name of the active window */
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc) {
        WCHAR path[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProc, 0, path, &size)) {
            /* Extract just the filename */
            WCHAR *fname = wcsrchr(path, L'\\');
            if (fname) fname++; else fname = path;
            wcscpy_s(app->active_app, MAX_APP_PATH, fname);

            /* Convert to lowercase for comparison */
            for (WCHAR *p = app->active_app; *p; p++)
                *p = (WCHAR)CharLowerW((LPWSTR)(ULONG_PTR)*p);
        }
        CloseHandle(hProc);
    }

    /* Check blacklist */
    app->is_blacklisted = settings_is_blacklisted(&app->settings, app->active_app);
}

/* ========================================================================
 * Keyboard Hook Callback
 *
 * This is the core function that intercepts all keystrokes.
 *
 * Flow:
 *   1. If Vietnamese is disabled or app is blacklisted → pass through
 *   2. Handle modifier key state updates
 *   3. Forward to the input engine for processing
 *   4. If the engine consumed the key → send the output character
 *   5. If not consumed → pass the original key through
 * ======================================================================== */

LRESULT CALLBACK hook_callback(int nCode, WPARAM wp, LPARAM lp)
{
    EvkApp *app = &g_app;

    if (nCode < 0)
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);

    /* Only process key events, not GetMessage hook events */
    if (nCode != HC_ACTION)
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);

    KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *)lp;
    BOOL keyDown = (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN);
    BOOL keyUp = (wp == WM_KEYUP || wp == WM_SYSKEYUP);

    /* ---- Handle Shift key up → cycle tone in Telex mode ---- */
    if (keyUp && kb->vkCode == VK_SHIFT && app->settings.input_method == METHOD_TELEX) {
        if (app->compose.active && app->compose.tone_char == 0 && app->compose.base_char) {
            /* Shift released with no tone → cycle through tones */
            /* (simplified: just apply sắc as default) */
            app->compose.tone_char = vn_apply_tone(app->compose.base_char, TONE_ACUTE);

            /* Send backspaces to erase compose buffer, then send composed char */
            wchar_t composed = input_get_composed_char(&app->compose);
            if (composed) {
                input_send_backspace(app->compose.compose_len);
                input_send_char(composed);
            }
            input_init(&app->compose);
        }
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);
    }

    /* ---- Check prerequisites ---- */
    if (!keyDown) {
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);
    }

    /* Vietnamese mode must be enabled */
    if (!app->settings.viet_enabled)
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);

    /* Must not be blacklisted */
    if (app->is_blacklisted)
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);

    /* Must not be in our own window */
    HWND hwndForeground = GetForegroundWindow();
    if (hwndForeground == app->hwndMain || hwndForeground == app->hwndTray)
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);

    /* ---- Handle Backspace during composition ---- */
    if (kb->vkCode == VK_BACK && app->compose.active) {
        if (input_backspace(&app->compose)) {
            /* Erase one character from the compose buffer */
            input_send_backspace(1);
            /* Re-send the simplified compose buffer */
            if (app->compose.compose_len > 0) {
                /* Show remaining compose chars */
                wchar_t base = app->compose.buffer[0];
                wchar_t composed = app->compose.tone_char ? app->compose.tone_char : base;
                if (composed && app->compose.compose_len == 1) {
                    /* Single char remaining - send it then erase on next key */
                }
            }
            return 1; /* Suppress backspace */
        }
        /* Not in composition - let backspace through */
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);
    }

    /* ---- Escape key → flush composition ---- */
    if (kb->vkCode == VK_ESCAPE && app->compose.active) {
        input_reset(&app->compose);
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);
    }

    /* ---- Space/Enter → flush composition and let key through ---- */
    if ((kb->vkCode == VK_SPACE || kb->vkCode == VK_RETURN) && app->compose.active) {
        wchar_t composed = input_get_composed_char(&app->compose);
        if (composed && composed != app->compose.buffer[0]) {
            /* Send backspaces to erase compose buffer, then send final char */
            input_send_backspace(app->compose.compose_len);
            input_send_char(composed);
            input_init(&app->compose);
            return 1; /* Suppress the space/enter, engine handled it */
        }
        /* No composition result - flush and let key through */
        input_init(&app->compose);
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);
    }

    /* ---- Tab → flush and pass through ---- */
    if (kb->vkCode == VK_TAB && app->compose.active) {
        input_reset(&app->compose);
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);
    }

    /* ---- Get modifier state ---- */
    UINT modifiers = 0;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;

    /* Don't process if Ctrl or Alt is held (shortcuts) */
    if (modifiers & (MOD_CONTROL | MOD_ALT)) {
        if (app->compose.active) {
            input_reset(&app->compose);
        }
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);
    }

    /* ---- Update active window if changed ---- */
    if (hwndForeground != app->hwndActive) {
        update_active_window();
        if (app->is_blacklisted) {
            input_reset(&app->compose);
            return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);
        }
    }

    /* ---- Process key through input engine ---- */
    BOOL consumed = input_process_key(
        &app->compose,
        kb->vkCode,
        kb->scanCode,
        keyDown,
        modifiers
    );

    if (consumed) {
        /* Key was consumed by the compose engine */
        return 1; /* Suppress the original keystroke */
    }

    /* Not consumed - check if we have a pending composition to flush */
    if (app->compose.active && app->compose.compose_len > 0) {
        /* Check if this key should finalize the composition */
        if (kb->vkCode == VK_SPACE || kb->vkCode == VK_RETURN ||
            kb->vkCode == VK_TAB || kb->vkCode == VK_ESCAPE) {
            wchar_t composed = input_get_composed_char(&app->compose);
            if (composed) {
                input_send_backspace(app->compose.compose_len);
                input_send_char(composed);
            }
            input_init(&app->compose);
            return 1; /* Suppress and send composed char */
        }
    }

    /* Pass through to the next hook */
    return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);
}
