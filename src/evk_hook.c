/**
 * evk_hook.c - Keyboard Hook (WH_KEYBOARD_LL) (Rewritten)
 *
 * Installs a system-wide low-level keyboard hook that intercepts
 * keystrokes and routes them through the Vietnamese input engine.
 *
 * The hook is simple and clean:
 *   1. Check if Vietnamese mode is enabled and app is not blacklisted
 *   2. Forward key events to the input engine
 *   3. If the engine consumed the key (returned 1), suppress the keystroke
 *   4. Otherwise, let the keystroke pass through normally
 */

#include "evk.h"
#include "evk_table.h"

/* Forward declaration */
LRESULT CALLBACK hook_callback(int nCode, WPARAM wp, LPARAM lp);

/* ========================================================================
 * Hook Install / Uninstall
 * ======================================================================== */

void hook_install(void)
{
    EvkApp *app = &g_app;

    if (app->hookKeyboard)
        return;

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
 * ======================================================================== */

static void update_active_window(void)
{
    EvkApp *app = &g_app;
    HWND hwnd = GetForegroundWindow();

    if (hwnd == app->hwndMain || hwnd == app->hwndTray)
        return;

    app->hwndActive = hwnd;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc) {
        WCHAR path[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProc, 0, path, &size)) {
            WCHAR *fname = wcsrchr(path, L'\\');
            if (fname) fname++; else fname = path;
            wcscpy_s(app->active_app, MAX_APP_PATH, fname);

            for (WCHAR *p = app->active_app; *p; p++)
                *p = (WCHAR)CharLowerW((LPWSTR)(ULONG_PTR)*p);
        }
        CloseHandle(hProc);
    }

    app->is_blacklisted = settings_is_blacklisted(&app->settings, app->active_app);
}

/* ========================================================================
 * Should this application receive Vietnamese input?
 * ======================================================================== */

static BOOL should_process(HWND hwndForeground)
{
    EvkApp *app = &g_app;

    /* Vietnamese mode must be enabled */
    if (!app->settings.viet_enabled)
        return FALSE;

    /* Must not be blacklisted */
    if (app->is_blacklisted)
        return FALSE;

    /* Must not be our own window */
    if (hwndForeground == app->hwndMain || hwndForeground == app->hwndTray)
        return FALSE;

    /* Must not have Ctrl or Alt held (shortcuts pass through) */
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
        return FALSE;
    if (GetAsyncKeyState(VK_MENU) & 0x8000)
        return FALSE;

    return TRUE;
}

/* ========================================================================
 * Keyboard Hook Callback
 * ======================================================================== */

LRESULT CALLBACK hook_callback(int nCode, WPARAM wp, LPARAM lp)
{
    EvkApp *app = &g_app;

    if (nCode < 0 || nCode != HC_ACTION)
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);

    KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *)lp;
    BOOL keyDown = (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN);

    if (!keyDown) {
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);
    }

    /* ---- Update active window tracking ---- */
    HWND hwndForeground = GetForegroundWindow();
    if (hwndForeground != app->hwndActive) {
        update_active_window();
    }

    /* ---- Check if we should process this keystroke ---- */
    if (!should_process(hwndForeground)) {
        /* Reset composition when switching to a non-processed app */
        if (app->compose.active) {
            input_init(&app->compose);
        }
        return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);
    }

    /* ---- Forward to input engine ---- */
    int consumed = input_process_key(
        &app->compose,
        kb->vkCode,
        kb->scanCode,
        keyDown,
        0  /* modifiers checked internally */
    );

    if (consumed) {
        return 1; /* Suppress the keystroke */
    }

    /* Pass through */
    return CallNextHookEx(app->hookKeyboard, nCode, wp, lp);
}
