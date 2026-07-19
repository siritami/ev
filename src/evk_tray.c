/**
 * evk_tray.c - System Tray Icon
 *
 * Manages the EVKey system tray icon, popup menu, and notifications.
 * The tray icon provides quick access to Vietnamese ON/OFF toggle,
 * input method selection, charset selection, and settings.
 */

#include "evk.h"
#include "evk_table.h"

/* ========================================================================
 * Tray Icon Creation
 * ======================================================================== */

BOOL tray_create(HWND hwnd)
{
    EvkApp *app = &g_app;

    /* Load icons from the exe resources */
    app->hIconBig = LoadIconW(app->hInstance, MAKEINTRESOURCEW(1));
    app->hIconSmall = LoadIconW(app->hInstance, MAKEINTRESOURCEW(1));

    /* If no resource icon, use a system icon as fallback */
    if (!app->hIconBig) {
        app->hIconBig = LoadIconW(NULL, IDI_APPLICATION);
        app->hIconSmall = LoadIconW(NULL, IDI_APPLICATION);
    }

    /* Setup NOTIFYICONDATA */
    memset(&app->nid, 0, sizeof(app->nid));
    app->nid.cbSize = sizeof(NOTIFYICONDATAW);
    app->nid.hWnd = hwnd;
    app->nid.uID = 1;
    app->nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    app->nid.uCallbackMessage = WM_TRAYICON;
    app->nid.hIcon = app->hIconBig;
    wcscpy_s(app->nid.szTip, 128, APP_FULLNAME);

    /* Add to system tray */
    Shell_NotifyIconW(NIM_ADD, &app->nid);

    app->hwndTray = hwnd;
    return TRUE;
}

void tray_destroy(HWND hwnd)
{
    EvkApp *app = &g_app;
    Shell_NotifyIconW(NIM_DELETE, &app->nid);
    if (app->hIconBig) DestroyIcon(app->hIconBig);
    if (app->hIconSmall) DestroyIcon(app->hIconSmall);
    app->hwndTray = NULL;
}

/* ========================================================================
 * Tray Context Menu
 * ======================================================================== */

void tray_show_menu(HWND hwnd)
{
    EvkApp *app = &g_app;
    POINT pt;
    GetCursorPos(&pt);

    /* Create popup menu */
    HMENU hMenu = CreatePopupMenu();

    /* ---- Vietnamese ON/OFF toggle ---- */
    AppendMenuW(hMenu, MF_STRING | (app->settings.viet_enabled ? MF_CHECKED : 0),
        IDM_TOGGLE_VIET, L"B\u1EADt ti\u1EBFng Vi\u1EC7&t (F12)");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    /* ---- Input Method submenu ---- */
    HMENU hMethod = CreatePopupMenu();
    AppendMenuW(hMethod, MF_STRING |
        (app->settings.input_method == METHOD_TELEX ? MF_CHECKED : 0),
        IDM_METHOD_TELEX, L"Telex");
    AppendMenuW(hMethod, MF_STRING |
        (app->settings.input_method == METHOD_VNI ? MF_CHECKED : 0),
        IDM_METHOD_VNI, L"VNI Windows");
    AppendMenuW(hMethod, MF_STRING |
        (app->settings.input_method == METHOD_VIQR ? MF_CHECKED : 0),
        IDM_METHOD_VIQR, L"VIQR");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hMethod, L"C\u00E1c ph\u01B0\u01A1ng ph\u00E1p nh\u1EADp");

    /* ---- Output Charset submenu ---- */
    HMENU hCharset = CreatePopupMenu();
    static const wchar_t *charset_names[] = {
        L"Unicode", L"UTF-8", L"Unicode Ref", L"Unicode Hex",
        L"Unicode Composite", L"Win CP1258", L"C String",
        L"VNI Internal", NULL, NULL, NULL, L"UTF-8 VIQR",
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        L"TCVN3 (ABC)", L"TCVN5", L"VISCII", L"VPS",
        L"Vietware X", L"Vietware F",
        NULL, NULL, NULL, NULL,
        NULL, NULL, L"Word", L"Word (Extended)"
    };

    for (int i = 0; i < 32; i++) {
        if (charset_names[i]) {
            AppendMenuW(hCharset, MF_STRING |
                (app->settings.output_charset == i ? MF_CHECKED : 0),
                IDM_CHARSET_BASE + i, charset_names[i]);
        }
    }
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hCharset, L"B\u1EA3ng m\u00E3 xu\u1EA5t");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    /* ---- Converter ---- */
    AppendMenuW(hMenu, MF_STRING, IDM_OPEN_CONVERTER, L"Chuy\u1EC3n \u0111\u1ED5i...");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    /* ---- Options ---- */
    AppendMenuW(hMenu, MF_STRING |
        (app->settings.auto_switch ? MF_CHECKED : 0),
        IDM_AUTO_SWITCH, L"T\u1EF1 \u0111\u1ED5ng chuy\u1EC3n");
    AppendMenuW(hMenu, MF_STRING |
        (app->settings.always_viet ? MF_CHECKED : 0),
        IDM_ALWAYS_VIET, L"Lu\u00F4n ti\u1EBFng Vi\u1EC7&t");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    /* ---- Settings ---- */
    AppendMenuW(hMenu, MF_STRING, IDM_OPEN_CONFIG, L"C\u00E0i \u0111\u1EB7t...");
    AppendMenuW(hMenu, MF_STRING, IDM_ABOUT, L"V\u1EC1...");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    /* ---- Exit ---- */
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Tho\u00E1t");

    /* Show the menu */
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
        pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

/* ========================================================================
 * Update Tray Icon Tooltip
 * ======================================================================== */

void tray_update_icon(void)
{
    EvkApp *app = &g_app;
    const wchar_t *method = L"Telex";
    switch (app->settings.input_method) {
    case METHOD_VNI:  method = L"VNI"; break;
    case METHOD_VIQR: method = L"VIQR"; break;
    }

    WCHAR tip[128];
    _swprintf(tip, L"%s [%s] %s",
        APP_FULLNAME,
        method,
        app->settings.viet_enabled ? L"ON" : L"OFF");

    wcscpy_s(app->nid.szTip, 128, tip);
    Shell_NotifyIconW(NIM_MODIFY, &app->nid);
}

/* ========================================================================
 * Tray Message Handler
 * ======================================================================== */

LRESULT tray_handle_message(HWND hwnd, WPARAM wp, LPARAM lp)
{
    EvkApp *app = &g_app;

    if ((UINT)lp == WM_RBUTTONUP) {
        tray_show_menu(hwnd);
        return 0;
    }

    if ((UINT)lp == WM_LBUTTONDBLCLK) {
        /* Double-click: toggle Vietnamese */
        app->settings.viet_enabled = !app->settings.viet_enabled;
        tray_update_icon();
        settings_save(&app->settings);
        return 0;
    }

    if ((UINT)lp == WM_LBUTTONUP) {
        /* Single click: show/hide settings info */
        tray_show_menu(hwnd);
        return 0;
    }

    return 0;
}

/* ========================================================================
 * Tray Menu Command Handler
 * ======================================================================== */

void tray_handle_command(HWND hwnd, UINT cmd)
{
    EvkApp *app = &g_app;

    switch (cmd) {
    case IDM_TOGGLE_VIET:
        app->settings.viet_enabled = !app->settings.viet_enabled;
        tray_update_icon();
        break;

    case IDM_METHOD_TELEX:
        app->settings.input_method = METHOD_TELEX;
        tray_update_icon();
        break;
    case IDM_METHOD_VNI:
        app->settings.input_method = METHOD_VNI;
        tray_update_icon();
        break;
    case IDM_METHOD_VIQR:
        app->settings.input_method = METHOD_VIQR;
        tray_update_icon();
        break;

    case IDM_OPEN_CONVERTER:
        dialog_show_converter(hwnd);
        break;

    case IDM_AUTO_SWITCH:
        app->settings.auto_switch = !app->settings.auto_switch;
        break;
    case IDM_ALWAYS_VIET:
        app->settings.always_viet = !app->settings.always_viet;
        break;

    case IDM_OPEN_CONFIG:
        dialog_show_config(hwnd);
        break;

    case IDM_ABOUT:
        dialog_show_about(hwnd);
        break;

    case IDM_EXIT:
        PostMessage(hwnd, WM_EVK_QUIT, 0, 0);
        break;

    default:
        /* Handle charset selection */
        if (cmd >= IDM_CHARSET_BASE && cmd < IDM_CHARSET_BASE + 32) {
            app->settings.output_charset = (EvkCharset)(cmd - IDM_CHARSET_BASE);
        }
        break;
    }

    settings_save(&app->settings);
}
