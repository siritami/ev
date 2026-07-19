/**
 * evk_main.c - Main Entry Point and Window Procedure
 *
 * EVKey Vietnamese Keyboard Input Method Editor
 *
 * This is the main file that initializes the application, creates
 * the hidden main window, installs the keyboard hook, and runs
 * the message loop.
 *
 * Architecture:
 *   1. Single-instance check (mutex + FindWindow)
 *   2. Create hidden main window (receives hook callbacks)
 *   3. Install WH_KEYBOARD_LL global hook
 *   4. Create system tray icon
 *   5. Run message loop
 *
 * Open-source reimplementation based on reverse engineering of EVKey64.exe.
 * Original EVKey by National Center for High-performance Computing (NCHC), Vietnam.
 */

#include "evk.h"
#include "evk_table.h"

/* ========================================================================
 * Global Application Instance
 * ======================================================================== */

EvkApp g_app;

/* ========================================================================
 * Single-Instance Check
 *
 * Uses a named mutex and FindWindow to ensure only one instance runs.
 * If another instance is found, signals it to quit and takes over.
 * ======================================================================== */

static BOOL check_single_instance(void)
{
    /* Create named mutex */
    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"EVKey_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        /* Another instance is running - tell it to quit */
        HWND hOld = FindWindowW(APP_CLASS, NULL);
        if (hOld) {
            PostMessageW(hOld, WM_EVK_QUIT, 1000, 0);
            Sleep(300);
        }
        return FALSE;
    }
    /* Store mutex handle in app context */
    g_app.hEvent = hMutex;
    return TRUE;
}

/* ========================================================================
 * Helper Thread
 *
 * Runs in background, periodically checks the active window and
 * updates the compose state if the foreground app changes.
 * ======================================================================== */

static DWORD WINAPI helper_thread(LPVOID param)
{
    (void)param;
    EvkApp *app = &g_app;

    while (app->settings.viet_enabled || 1) {
        /* Wait for shutdown signal or timeout */
        DWORD result = WaitForSingleObject(app->hEvent, 100);
        if (result == WAIT_OBJECT_0)
            break; /* Shutdown signaled */

        /* Check if Vietnamese should be auto-disabled for this app */
        HWND hwndForeground = GetForegroundWindow();
        if (hwndForeground && hwndForeground != app->hwndMain) {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwndForeground, &pid);

            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                WCHAR path[MAX_PATH];
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, path, &size)) {
                    WCHAR *fname = wcsrchr(path, L'\\');
                    if (fname) fname++; else fname = path;

                    WCHAR lower[MAX_PATH];
                    wcscpy_s(lower, MAX_PATH, fname);
                    for (WCHAR *p = lower; *p; p++)
                        *p = (WCHAR)CharLowerW((LPWSTR)(ULONG_PTR)*p);

                    /* Check per-app overrides */
                    BOOL found_on = FALSE, found_off = FALSE;
                    for (int i = 0; i < app->settings.on_vn_count; i++) {
                        if (_wcsicmp(app->settings.on_vn_apps[i], lower) == 0) {
                            found_on = TRUE;
                            break;
                        }
                    }
                    for (int i = 0; i < app->settings.off_vn_count; i++) {
                        if (_wcsicmp(app->settings.off_vn_apps[i], lower) == 0) {
                            found_off = TRUE;
                            break;
                        }
                    }

                    if (found_off && !app->settings.always_viet) {
                        app->settings.viet_enabled = FALSE;
                        tray_update_icon();
                    } else if (found_on) {
                        app->settings.viet_enabled = TRUE;
                        tray_update_icon();
                    }
                }
                CloseHandle(hProc);
            }
        }
    }

    return 0;
}

/* ========================================================================
 * Register Window Class
 * ======================================================================== */

static ATOM register_main_class(HINSTANCE hInst)
{
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = APP_CLASS;
    wc.lpszMenuName = APP_FULLNAME;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hIconSm = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

    return RegisterClassExW(&wc);
}

/* ========================================================================
 * Main Window Procedure
 *
 * Handles:
 *   - WM_CREATE: Initialize tray icon
 *   - WM_TRAYICON: Tray icon events
 *   - WM_COMMAND: Menu commands
 *   - WM_EVK_QUIT: Shutdown
 *   - Shell hook notifications
 * ======================================================================== */

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    EvkApp *app = &g_app;

    switch (msg) {
    case WM_CREATE:
        /* Create system tray icon */
        tray_create(hwnd);

        /* Install keyboard hook */
        hook_install();

        /* Start helper thread */
        app->hThread = CreateThread(NULL, 0, helper_thread, NULL, 0, NULL);

        /* Update icon */
        tray_update_icon();
        return 0;

    case WM_TRAYICON:
        return tray_handle_message(hwnd, wp, lp);

    case WM_COMMAND:
        tray_handle_command(hwnd, LOWORD(wp));
        return 0;

    case WM_EVK_QUIT:
        /* Shutdown request */
        if (wp == 1000) {
            /* Signal from another instance - just quit */
        }
        PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        /* Cleanup */
        hook_uninstall();
        tray_destroy(hwnd);

        if (app->hThread) {
            /* Signal helper thread to exit */
            SetEvent(app->hEvent);
            WaitForSingleObject(app->hThread, 1000);
            CloseHandle(app->hThread);
        }

        if (app->hEvent) {
            CloseHandle(app->hEvent);
        }

        PostQuitMessage(0);
        return 0;

    default:
        /* Handle TaskbarCreated to re-add tray icon */
        if (app->shellHookMsg && msg == app->shellHookMsg) {
            /* Shell hook notification */
            return 0;
        }
        break;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ========================================================================
 * WinMain - Application Entry Point
 *
 * Initialization sequence:
 *   1. Check single instance
 *   2. Initialize common controls
 *   3. Initialize settings (load from INI)
 *   4. Initialize input engine
 *   5. Initialize macro system
 *   6. Register window class
 *   7. Create hidden main window
 *   8. Enter message loop
 * ======================================================================== */

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

    /* ---- Initialize application context ---- */
    memset(&g_app, 0, sizeof(g_app));
    g_app.hInstance = hInstance;

    /* ---- Single instance check ---- */
    if (!check_single_instance()) {
        return 0;
    }

    /* ---- Initialize common controls ---- */
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    /* ---- Initialize settings ---- */
    settings_init(&g_app.settings);
    settings_load(&g_app.settings);

    /* ---- Initialize input engine ---- */
    input_init(&g_app.compose);

    /* ---- Load macros ---- */
    macro_init();
    if (g_app.settings.macro_path[0]) {
        macro_load(g_app.settings.macro_path);
    }

    /* ---- Register window class ---- */
    register_main_class(hInstance);

    /* ---- Create hidden main window ---- */
    g_app.hwndMain = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        APP_CLASS,
        APP_FULLNAME,
        WS_POPUP,
        -32000, -32000, 1, 1,  /* Off-screen */
        NULL, NULL, hInstance, NULL
    );

    if (!g_app.hwndMain) {
        MessageBoxW(NULL, L"Failed to create main window!", APP_NAME, MB_ICONERROR);
        macro_free();
        return 1;
    }

    /* ---- Register shell hook for taskbar restart ---- */
    g_app.shellHookMsg = RegisterWindowMessageW(L"TaskbarCreated");

    /* ---- Show tray icon (minimized) ---- */
    if (!g_app.settings.start_minimized) {
        /* Show a brief balloon notification */
        g_app.nid.uFlags |= NIF_INFO;
        wcscpy_s(g_app.nid.szInfoTitle, 64, APP_NAME);
        wcscpy_s(g_app.nid.szInfo, 256,
            L"EVKey \u0111\u00E3 kh\u1EDFi \u0111\u1ED9ng.\n"
            L"Nh\u1EA5n chu\u1ED9t ph\u1EA3i l\u00EAn bi\u1EA3u tu\u01B0\u1EE3ng \u1EDF khay h\u1EC7 th\u1ED1ng \u0111\u1EC3 c\u00E0i \u0111\u1EB7t.");
        Shell_NotifyIconW(NIM_MODIFY, &g_app.nid);
    }

    /* ---- Auto-start registration ---- */
    if (g_app.settings.auto_start) {
        settings_set_autostart(TRUE);
    }

    /* ---- Enter message loop ---- */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    /* ---- Cleanup ---- */
    macro_free();

    return (int)msg.wParam;
}
