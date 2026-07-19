/**
 * evk_settings.c - Settings Management
 *
 * Handles loading and saving EVKey settings from setting.ini.
 * Settings include input method, output charset, blacklist, etc.
 *
 * Settings file format (Windows INI):
 *   [setting]           - Main settings
 *   [blacklist]         - Per-app blacklist
 *   [off_vn]            - Apps where Vietnamese is disabled
 *   [on_vn]             - Apps where Vietnamese is forced on
 *   [user_method_input] - User input method settings
 */

#include "evk.h"

/* ========================================================================
 * Default Settings
 * ======================================================================== */

void settings_init(EvkSettings *s)
{
    memset(s, 0, sizeof(*s));

    /* Defaults */
    s->input_method = METHOD_TELEX;
    s->output_charset = CHARSET_UNICODE;
    s->viet_enabled = TRUE;
    s->auto_switch = TRUE;
    s->always_viet = FALSE;
    s->backspace_compose = TRUE;
    s->show_notifications = TRUE;
    s->start_minimized = FALSE;
    s->auto_start = FALSE;

    /* Build default paths */
    util_get_exe_dir(s->exe_dir, MAX_APP_PATH);

    /* Default blacklist: games and apps that don't need Vietnamese input */
    const wchar_t *default_blacklist[] = {
        L"crossfire.exe",
        L"csgo.exe",
        L"dota2.exe",
        L"league of legends.exe",
        L"valorant.exe",
        L"pubg.exe",
        L"fortnite.exe",
        L"genshinimpact.exe",
        L"roblox.exe",
        NULL
    };

    s->blacklist_count = 0;
    for (int i = 0; default_blacklist[i] && s->blacklist_count < MAX_BLACKLIST; i++) {
        wcscpy_s(s->blacklist[s->blacklist_count], MAX_APP_PATH, default_blacklist[i]);
        s->blacklist_count++;
    }
}

/* ========================================================================
 * Load Settings from INI File
 * ======================================================================== */

void settings_load(EvkSettings *s)
{
    /* Build INI path: {exe_dir}\setting.ini */
    wcscpy_s(s->ini_path, MAX_APP_PATH, s->exe_dir);
    PathAppendW(s->ini_path, L"setting.ini");

    /* Build macro path: {exe_dir}\evkmacro.txt */
    wcscpy_s(s->macro_path, MAX_APP_PATH, s->exe_dir);
    PathAppendW(s->macro_path, L"evkmacro.txt");

    /* Check if INI file exists */
    if (GetFileAttributesW(s->ini_path) == INVALID_FILE_ATTRIBUTES) {
        settings_save(s); /* Create with defaults */
        return;
    }

    WCHAR buf[INI_BUF_SIZE];

    /* [setting] section */
    GetPrivateProfileStringW(L"setting", L"input_method", L"telex",
        buf, INI_BUF_SIZE, s->ini_path);
    if (_wcsicmp(buf, L"vni") == 0)        s->input_method = METHOD_VNI;
    else if (_wcsicmp(buf, L"viqr") == 0)   s->input_method = METHOD_VIQR;
    else if (_wcsicmp(buf, L"tcvn3") == 0)  s->input_method = METHOD_TCVN3;
    else if (_wcsicmp(buf, L"viscii") == 0) s->input_method = METHOD_VISCII;
    else                                    s->input_method = METHOD_TELEX;

    s->viet_enabled = GetPrivateProfileIntW(L"setting", L"viet_enabled", 1, s->ini_path);
    s->auto_switch  = GetPrivateProfileIntW(L"setting", L"auto_switch", 1, s->ini_path);
    s->always_viet  = GetPrivateProfileIntW(L"setting", L"always_viet", 0, s->ini_path);
    s->backspace_compose = GetPrivateProfileIntW(L"setting", L"backspace", 1, s->ini_path);
    s->start_minimized   = GetPrivateProfileIntW(L"setting", L"start_min", 0, s->ini_path);
    s->auto_start   = GetPrivateProfileIntW(L"setting", L"auto_start", 0, s->ini_path);
    s->output_charset = (EvkCharset)GetPrivateProfileIntW(L"setting", L"charset", 0, s->ini_path);

    /* [blacklist] section - load per-app blacklist overrides */
    s->blacklist_count = 0;
    WCHAR key[32];
    for (int i = 0; i < MAX_BLACKLIST; i++) {
        _swprintf(key, L"app%d", i);
        GetPrivateProfileStringW(L"blacklist", key, L"",
            buf, MAX_APP_PATH, s->ini_path);
        if (buf[0] == 0) break;

        /* Convert to lowercase */
        for (WCHAR *p = buf; *p; p++)
            *p = (WCHAR)CharLowerW((LPWSTR)(ULONG_PTR)*p);

        wcscpy_s(s->blacklist[s->blacklist_count], MAX_APP_PATH, buf);
        s->blacklist_count++;
    }

    /* [on_vn] section - apps where Vietnamese is forced on */
    s->on_vn_count = 0;
    for (int i = 0; i < MAX_BLACKLIST; i++) {
        _swprintf(key, L"app%d", i);
        GetPrivateProfileStringW(L"on_vn", key, L"",
            buf, MAX_APP_PATH, s->ini_path);
        if (buf[0] == 0) break;
        for (WCHAR *p = buf; *p; p++)
            *p = (WCHAR)CharLowerW((LPWSTR)(ULONG_PTR)*p);
        wcscpy_s(s->on_vn_apps[s->on_vn_count], MAX_APP_PATH, buf);
        s->on_vn_count++;
    }

    /* [off_vn] section - apps where Vietnamese is disabled */
    s->off_vn_count = 0;
    for (int i = 0; i < MAX_BLACKLIST; i++) {
        _swprintf(key, L"app%d", i);
        GetPrivateProfileStringW(L"off_vn", key, L"",
            buf, MAX_APP_PATH, s->ini_path);
        if (buf[0] == 0) break;
        for (WCHAR *p = buf; *p; p++)
            *p = (WCHAR)CharLowerW((LPWSTR)(ULONG_PTR)*p);
        wcscpy_s(s->off_vn_apps[s->off_vn_count], MAX_APP_PATH, buf);
        s->off_vn_count++;
    }
}

/* ========================================================================
 * Save Settings to INI File
 * ======================================================================== */

void settings_save(const EvkSettings *s)
{
    WCHAR buf[INI_BUF_SIZE];

    /* [setting] section */
    const wchar_t *method_str = L"telex";
    switch (s->input_method) {
    case METHOD_VNI:        method_str = L"vni"; break;
    case METHOD_VIQR:       method_str = L"viqr"; break;
    case METHOD_TCVN3:      method_str = L"tcvn3"; break;
    case METHOD_VISCII:     method_str = L"viscii"; break;
    case METHOD_BK_HCM:     method_str = L"bkhcm"; break;
    case METHOD_USER_DEFINED: method_str = L"user"; break;
    }

    WritePrivateProfileStringW(L"setting", L"input_method", method_str, s->ini_path);
    _swprintf(buf, L"%d", s->viet_enabled);
    WritePrivateProfileStringW(L"setting", L"viet_enabled", buf, s->ini_path);
    _swprintf(buf, L"%d", s->auto_switch);
    WritePrivateProfileStringW(L"setting", L"auto_switch", buf, s->ini_path);
    _swprintf(buf, L"%d", s->always_viet);
    WritePrivateProfileStringW(L"setting", L"always_viet", buf, s->ini_path);
    _swprintf(buf, L"%d", s->backspace_compose);
    WritePrivateProfileStringW(L"setting", L"backspace", buf, s->ini_path);
    _swprintf(buf, L"%d", s->start_minimized);
    WritePrivateProfileStringW(L"setting", L"start_min", buf, s->ini_path);
    _swprintf(buf, L"%d", s->auto_start);
    WritePrivateProfileStringW(L"setting", L"auto_start", buf, s->ini_path);
    _swprintf(buf, L"%d", (int)s->output_charset);
    WritePrivateProfileStringW(L"setting", L"charset", buf, s->ini_path);

    /* Clear old blacklist entries */
    WCHAR key[32];
    for (int i = 0; i < MAX_BLACKLIST + 10; i++) {
        _swprintf(key, L"app%d", i);
        WritePrivateProfileStringW(L"blacklist", key, NULL, s->ini_path);
        WritePrivateProfileStringW(L"on_vn", key, NULL, s->ini_path);
        WritePrivateProfileStringW(L"off_vn", key, NULL, s->ini_path);
    }

    /* [blacklist] section */
    for (int i = 0; i < s->blacklist_count; i++) {
        _swprintf(key, L"app%d", i);
        WritePrivateProfileStringW(L"blacklist", key, s->blacklist[i], s->ini_path);
    }

    /* [on_vn] section */
    for (int i = 0; i < s->on_vn_count; i++) {
        _swprintf(key, L"app%d", i);
        WritePrivateProfileStringW(L"on_vn", key, s->on_vn_apps[i], s->ini_path);
    }

    /* [off_vn] section */
    for (int i = 0; i < s->off_vn_count; i++) {
        _swprintf(key, L"app%d", i);
        WritePrivateProfileStringW(L"off_vn", key, s->off_vn_apps[i], s->ini_path);
    }
}

/* ========================================================================
 * Blacklist Operations
 * ======================================================================== */

BOOL settings_is_blacklisted(const EvkSettings *s, const wchar_t *app)
{
    if (!app || !app[0]) return FALSE;

    for (int i = 0; i < s->blacklist_count; i++) {
        if (_wcsicmp(s->blacklist[i], app) == 0)
            return TRUE;
    }
    return FALSE;
}

void settings_add_blacklist(EvkSettings *s, const wchar_t *app)
{
    if (s->blacklist_count >= MAX_BLACKLIST) return;

    /* Check for duplicates */
    for (int i = 0; i < s->blacklist_count; i++) {
        if (_wcsicmp(s->blacklist[i], app) == 0) return;
    }

    WCHAR lower[MAX_APP_PATH];
    wcscpy_s(lower, MAX_APP_PATH, app);
    for (WCHAR *p = lower; *p; p++)
        *p = (WCHAR)CharLowerW((LPWSTR)(ULONG_PTR)*p);

    wcscpy_s(s->blacklist[s->blacklist_count], MAX_APP_PATH, lower);
    s->blacklist_count++;
}

void settings_remove_blacklist(EvkSettings *s, const wchar_t *app)
{
    for (int i = 0; i < s->blacklist_count; i++) {
        if (_wcsicmp(s->blacklist[i], app) == 0) {
            /* Shift remaining entries */
            for (int j = i; j < s->blacklist_count - 1; j++) {
                wcscpy_s(s->blacklist[j], MAX_APP_PATH, s->blacklist[j + 1]);
            }
            s->blacklist_count--;
            return;
        }
    }
}

/* ========================================================================
 * Auto-Start Registration (Windows Registry)
 * ======================================================================== */

static const wchar_t *AUTOSTART_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t *AUTOSTART_VALUE = L"EVKey";

void settings_set_autostart(BOOL enabled)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enabled) {
            WCHAR path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
            RegSetValueExW(hKey, AUTOSTART_VALUE, 0, REG_SZ,
                (BYTE *)path, (DWORD)((wcslen(path) + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, AUTOSTART_VALUE);
        }
        RegCloseKey(hKey);
    }
}

BOOL settings_get_autostart(void)
{
    HKEY hKey;
    BOOL exists = FALSE;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD type, size;
        exists = (RegQueryValueExW(hKey, AUTOSTART_VALUE, NULL, &type, NULL, &size) == ERROR_SUCCESS);
        RegCloseKey(hKey);
    }
    return exists;
}
