/**
 * evk_dialog.c - Dialog Windows
 *
 * Implements the EVKey settings dialog, text converter dialog,
 * and about dialog.
 */

#include "evk.h"
#include "evk_table.h"

/* ========================================================================
 * About Dialog
 * ======================================================================== */

void dialog_show_about(HWND hwndParent)
{
    WCHAR msg[512];
    _swprintf(msg,
        L"EVKey v" APP_VERSION L"\n"
        L"Vietnamese Keyboard Input Method\n\n"
        L"Original EVKey by NCHC Vietnam\n"
        L"Open-source reimplementation\n\n"
        L"Supported input methods:\n"
        L"  Telex, VNI, VIQR\n\n"
        L"Supported output charsets:\n"
        L"  Unicode, UTF-8, TCVN3, VISCII,\n"
        L"  VPS, WinCP1258, VIQR, and more.\n\n"
        L"This is an open-source build.\n"
        L"Source code available on GitHub.");

    MessageBoxW(hwndParent, msg, L"About EVKey", MB_OK | MB_ICONINFORMATION);
}

/* ========================================================================
 * Settings Dialog
 *
 * A modeless dialog with tabs for:
 *   - General settings (input method, output charset)
 *   - Blacklist management
 *   - Advanced options
 * ======================================================================== */

#define IDD_CONFIG      1001
#define IDC_METHOD      1010
#define IDC_CHARSET     1011
#define IDC_VIET_ON     1012
#define IDC_AUTO_SWITCH 1013
#define IDC_ALWAYS_VIET 1014
#define IDC_BACKSPACE   1015
#define IDC_START_MIN   1016
#define IDC_AUTO_START  1017
#define IDC_BLACKLIST   1020
#define IDC_ADD_BLACK   1021
#define IDC_REM_BLACK   1022
#define IDC_APPLY       1030
#define IDC_CLOSE       1031

static HWND s_hwndConfig = NULL;

static BOOL CALLBACK ConfigDialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    EvkApp *app = &g_app;

    switch (msg) {
    case WM_INITDIALOG: {
        /* Populate input method combo */
        HWND hMethod = GetDlgItem(hwnd, IDC_METHOD);
        SendMessageW(hMethod, CB_ADDSTRING, 0, (LPARAM)L"Telex");
        SendMessageW(hMethod, CB_ADDSTRING, 0, (LPARAM)L"VNI Windows");
        SendMessageW(hMethod, CB_ADDSTRING, 0, (LPARAM)L"VIQR");
        SendMessageW(hMethod, CB_ADDSTRING, 0, (LPARAM)L"TCVN3 (ABC)");
        SendMessageW(hMethod, CB_ADDSTRING, 0, (LPARAM)L"VISCII");
        SendMessageW(hMethod, CB_SETCURSEL, (WPARAM)app->settings.input_method, 0);

        /* Populate charset combo */
        HWND hCharset = GetDlgItem(hwnd, IDC_CHARSET);
        SendMessageW(hCharset, CB_ADDSTRING, 0, (LPARAM)L"Unicode");
        SendMessageW(hCharset, CB_ADDSTRING, 0, (LPARAM)L"UTF-8");
        SendMessageW(hCharset, CB_ADDSTRING, 0, (LPARAM)L"TCVN3 (ABC)");
        SendMessageW(hCharset, CB_ADDSTRING, 0, (LPARAM)L"VISCII");
        SendMessageW(hCharset, CB_ADDSTRING, 0, (LPARAM)L"VPS");
        SendMessageW(hCharset, CB_ADDSTRING, 0, (LPARAM)L"Win CP1258");
        SendMessageW(hCharset, CB_ADDSTRING, 0, (LPARAM)L"VIQR");
        SendMessageW(hCharset, CB_SETCURSEL, (WPARAM)app->settings.output_charset, 0);

        /* Set checkboxes */
        CheckDlgButton(hwnd, IDC_VIET_ON, app->settings.viet_enabled ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_AUTO_SWITCH, app->settings.auto_switch ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_ALWAYS_VIET, app->settings.always_viet ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_BACKSPACE, app->settings.backspace_compose ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_START_MIN, app->settings.start_minimized ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_AUTO_START, app->settings.auto_start ? BST_CHECKED : BST_UNCHECKED);

        /* Populate blacklist */
        HWND hList = GetDlgItem(hwnd, IDC_BLACKLIST);
        for (int i = 0; i < app->settings.blacklist_count; i++) {
            SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)app->settings.blacklist[i]);
        }
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_APPLY: {
            /* Read settings from dialog */
            app->settings.input_method = (EvkInputMethod)SendMessageW(
                GetDlgItem(hwnd, IDC_METHOD), CB_GETCURSEL, 0, 0);
            app->settings.output_charset = (EvkCharset)SendMessageW(
                GetDlgItem(hwnd, IDC_CHARSET), CB_GETCURSEL, 0, 0);
            app->settings.viet_enabled = (IsDlgButtonChecked(hwnd, IDC_VIET_ON) == BST_CHECKED);
            app->settings.auto_switch = (IsDlgButtonChecked(hwnd, IDC_AUTO_SWITCH) == BST_CHECKED);
            app->settings.always_viet = (IsDlgButtonChecked(hwnd, IDC_ALWAYS_VIET) == BST_CHECKED);
            app->settings.backspace_compose = (IsDlgButtonChecked(hwnd, IDC_BACKSPACE) == BST_CHECKED);
            app->settings.start_minimized = (IsDlgButtonChecked(hwnd, IDC_START_MIN) == BST_CHECKED);

            BOOL new_autostart = (IsDlgButtonChecked(hwnd, IDC_AUTO_START) == BST_CHECKED);
            if (new_autostart != app->settings.auto_start) {
                settings_set_autostart(new_autostart);
                app->settings.auto_start = new_autostart;
            }

            settings_save(&app->settings);
            tray_update_icon();

            MessageBoxW(hwnd, L"C\u00E0i \u0111\u1EB7t \u0111\u00E3 \u0111\u01B0\u1EE3c l\u01B0u!",
                        L"EVKey", MB_OK | MB_ICONINFORMATION);
            break;
        }

        case IDC_ADD_BLACK: {
            WCHAR buf[MAX_PATH] = {0};
            /* Simple input dialog */
            if (DialogBoxParamW(NULL, MAKEINTRESOURCEW(1002), hwnd,
                (DLGPROC)ConfigDialogProc, (LPARAM)buf)) {
                app->settings_add_blacklist(&app->settings, buf);
                HWND hList = GetDlgItem(hwnd, IDC_BLACKLIST);
                SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)buf);
            }
            break;
        }

        case IDC_REM_BLACK: {
            HWND hList = GetDlgItem(hwnd, IDC_BLACKLIST);
            int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR) {
                WCHAR buf[MAX_PATH];
                SendMessageW(hList, LB_GETTEXT, sel, (LPARAM)buf);
                settings_remove_blacklist(&app->settings, buf);
                SendMessageW(hList, LB_DELETESTRING, sel, 0);
            }
            break;
        }

        case IDC_CLOSE:
        case IDCANCEL:
            DestroyWindow(hwnd);
            s_hwndConfig = NULL;
            break;
        }
        return TRUE;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        s_hwndConfig = NULL;
        return TRUE;
    }
    return FALSE;
}

void dialog_show_config(HWND hwndParent)
{
    if (s_hwndConfig) {
        SetForegroundWindow(s_hwndConfig);
        return;
    }
    s_hwndConfig = CreateDialogW(NULL, MAKEINTRESOURCEW(IDD_CONFIG),
        hwndParent, ConfigDialogProc);
    if (s_hwndConfig) {
        ShowWindow(s_hwndConfig, SW_SHOW);
    }
}

/* ========================================================================
 * Text Converter Dialog
 *
 * A simple dialog for converting text between Vietnamese charsets.
 * User types text in input box, selects output charset, and gets
 * converted text in output box.
 * ======================================================================== */

static HWND s_hwndConverter = NULL;

#define IDD_CONVERTER   2001
#define IDC_CONV_INPUT  2010
#define IDC_CONV_OUTPUT 2011
#define IDC_CONV_METHOD 2012
#define IDC_CONV_CONVERT 2013

static BOOL CALLBACK ConverterDialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    EvkApp *app = &g_app;

    switch (msg) {
    case WM_INITDIALOG: {
        HWND hCombo = GetDlgItem(hwnd, IDC_CONV_METHOD);
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Unicode");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"UTF-8");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Unicode Ref (NCR)");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Unicode Hex");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"C String (\\uXXXX)");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"TCVN3 (ABC)");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"VISCII");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"VPS");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Win CP1258");
        SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_CONV_CONVERT) {
            /* Get input text */
            HWND hInput = GetDlgItem(hwnd, IDC_CONV_INPUT);
            int len = GetWindowTextLengthW(hInput);
            if (len == 0) return TRUE;

            wchar_t *input = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
            wchar_t *output = (wchar_t *)malloc((len * 16 + 1) * sizeof(wchar_t));
            if (!input || !output) { free(input); free(output); return TRUE; }

            GetWindowTextW(hInput, input, len + 1);

            /* Get selected charset */
            int charset_idx = (int)SendMessageW(GetDlgItem(hwnd, IDC_CONV_METHOD),
                CB_GETCURSEL, 0, 0);

            /* Map combo index to charset ID */
            static const EvkCharset charset_map[] = {
                CHARSET_UNICODE, CHARSET_UTF8, CHARSET_UNICODE_REF,
                CHARSET_UNICODE_HEX, CHARSET_CSTRING, CHARSET_TCVN3_ABC,
                CHARSET_VISCII, CHARSET_VPS, CHARSET_WINCP1258
            };
            EvkCharset cs = (charset_idx < 9) ? charset_map[charset_idx] : CHARSET_UNICODE;

            /* Convert */
            charset_convert_string(input, output, len * 16, cs);

            /* Show output */
            SetWindowTextW(GetDlgItem(hwnd, IDC_CONV_OUTPUT), output);

            free(input);
            free(output);
            return TRUE;
        }

        if (LOWORD(wp) == IDCANCEL || LOWORD(wp) == IDOK) {
            DestroyWindow(hwnd);
            s_hwndConverter = NULL;
            return TRUE;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        s_hwndConverter = NULL;
        return TRUE;
    }
    return FALSE;
}

void dialog_show_converter(HWND hwndParent)
{
    if (s_hwndConverter) {
        SetForegroundWindow(s_hwndConverter);
        return;
    }
    s_hwndConverter = CreateDialogW(NULL, MAKEINTRESOURCEW(IDD_CONVERTER),
        hwndParent, ConverterDialogProc);
    if (s_hwndConverter) {
        ShowWindow(s_hwndConverter, SW_SHOW);
    }
}
