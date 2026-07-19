/**
 * evk.h - EVKey Vietnamese Keyboard IME
 *
 * Open-source reimplementation based on reverse engineering of EVKey64.exe.
 * EVKey is a Vietnamese keyboard input method for Windows that intercepts
 * keystrokes and converts them to Vietnamese characters using Telex, VNI,
 * VIQR, TCVN3, VISCII, and other input methods.
 *
 * License: See LICENSE file.
 */

#ifndef EVK_H
#define EVK_H

/* ========================================================================
 * Platform & Build Configuration
 * ======================================================================== */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 /* Windows 7+ */
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define UNICODE 1
#define _UNICODE 1

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <imm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ========================================================================
 * Application Constants
 * ======================================================================== */

#define APP_NAME            L"EVKey"
#define APP_FULLNAME        L"EV Vietnamese Keyboard"
#define APP_CLASS           L"EVKey_MainWnd"
#define APP_VERSION         L"6.0"

#define WM_SHELLHOOK        (WM_USER + 0x401)
#define WM_EVK_QUIT         (WM_USER + 0x402)
#define WM_EVK_DLG_CLOSE    (WM_USER + 0x464)
#define WM_EVK_DLG_CMD1     (WM_USER + 0x465)
#define WM_EVK_DLG_CMD2     (WM_USER + 0x466)
#define WM_EVK_DLG_CMD3     (WM_USER + 0x467)
#define WM_EVK_DLG_CMD4     (WM_USER + 0x468)
#define WM_EVK_DLG_CMD5     (WM_USER + 0x469)
#define WM_EVK_SETTINGS     (WM_USER + 0x8D2)

/* Tray icon callback */
#define WM_TRAYICON         (WM_USER + 0x401)

/* Menu command IDs */
#define IDM_TOGGLE_VIET     0xC352  /* Toggle Vietnamese ON/OFF (default) */
#define IDM_METHOD_TELEX    0xC35B
#define IDM_METHOD_VNI      0xC35C
#define IDM_METHOD_VIQR     0xC361
#define IDM_OPEN_CONVERTER  0xC363
#define IDM_OPEN_CONVERTER2 0xC364
#define IDM_AUTO_SWITCH     0xC36C
#define IDM_ALWAYS_VIET     0xC36D
#define IDM_CHARSET_BASE    0xC38D  /* + index = output charset */
#define IDM_OPEN_CONFIG     0xC7A5
#define IDM_BACKSPACE       0xC7A7
#define IDM_EXIT            0xC787
#define IDM_ABOUT           0xC799
#define IDM_ON_VIET         0xC79C
#define IDM_FORCE_VIET      0xC79E
#define IDM_SHOW_CONFIG     0xC790

/* ========================================================================
 * Vietnamese Input Methods
 * ======================================================================== */

typedef enum {
    METHOD_TELEX = 0,
    METHOD_VNI,
    METHOD_VIQR,
    METHOD_TCVN3,
    METHOD_VISCII,
    METHOD_BK_HCM,
    METHOD_USER_DEFINED,
    METHOD_COUNT
} EvkInputMethod;

/* ========================================================================
 * Output Charset Encodings
 * ======================================================================== */

typedef enum {
    CHARSET_UNICODE = 0,
    CHARSET_UTF8,
    CHARSET_UNICODE_REF,     /* NCR decimal &#xxx; */
    CHARSET_UNICODE_HEX,     /* NCR hex &#xHHH; */
    CHARSET_UNICODE_COMPOSE, /* Decomposed Unicode */
    CHARSET_WINCP1258,
    CHARSET_CSTRING,         /* \uXXXX */
    CHARSET_VN_INTERNAL,
    CHARSET_UTF8VIQR = 11,
    CHARSET_WORD = 30,
    CHARSET_WORD2 = 31,
    CHARSET_TCVN3_ABC = 0x14,
    CHARSET_TCVN5,
    CHARSET_VISCII,
    CHARSET_VPS,
    CHARSET_VIETWARE_X,
    CHARSET_VIETWARE_F,
    CHARSET_BK_HCM1 = 0x28,
    CHARSET_BK_HCM2,
    CHARSET_BK_HCM3,
    CHARSET_BK_HCM4,
    CHARSET_COUNT
} EvkCharset;

/* ========================================================================
 * Vietnamese Tone Marks (diacritics)
 * ======================================================================== */

typedef enum {
    TONE_NONE = 0,
    TONE_GRAVE,      /* à (huyền) */
    TONE_ACUTE,      /* á (sắc) */
    TONE_HOOK,       /* ả (hỏi) */
    TONE_TILDE,      /* ã (ngã) */
    TONE_DOT,        /* ạ (nặng) */
    TONE_COUNT
} VnTone;

/* ========================================================================
 * Vietnamese Vowel Composition State
 * ======================================================================== */

/* Vowel base + tone → precomposed character mapping */
typedef struct {
    wchar_t base;       /* Base vowel: a, e, i, o, u, y, d */
    VnTone   tone;      /* Tone mark */
    wchar_t  composed;  /* Result character */
} VnCharMap;

/* ========================================================================
 * Application Settings
 * ======================================================================== */

#define MAX_BLACKLIST       128
#define MAX_MACROS          256
#define MAX_APP_PATH        260
#define INI_BUF_SIZE        1024

typedef struct {
    /* Input method */
    EvkInputMethod  input_method;

    /* Output charset */
    EvkCharset      output_charset;

    /* Flags */
    BOOL            viet_enabled;       /* Vietnamese mode ON/OFF */
    BOOL            auto_switch;        /* Auto-switch per application */
    BOOL            always_viet;        /* Always Vietnamese */
    BOOL            backspace_compose;  /* Backspace removes last compose char */

    /* Hotkeys */
    UINT            toggle_hotkey;      /* Virtual key + modifiers */
    UINT            switch_hotkey;

    /* UI */
    BOOL            show_notifications;
    BOOL            start_minimized;
    BOOL            auto_start;         /* Run on Windows login */

    /* Paths */
    WCHAR           exe_dir[MAX_APP_PATH];
    WCHAR           ini_path[MAX_APP_PATH];
    WCHAR           macro_path[MAX_APP_PATH];

    /* Blacklist: applications where EVKey is disabled */
    WCHAR           blacklist[MAX_BLACKLIST][MAX_APP_PATH];
    int             blacklist_count;

    /* Per-app Vietnamese state overrides */
    WCHAR           on_vn_apps[MAX_BLACKLIST][MAX_APP_PATH];
    int             on_vn_count;
    WCHAR           off_vn_apps[MAX_BLACKLIST][MAX_APP_PATH];
    int             off_vn_count;

} EvkSettings;

/* ========================================================================
 * Keyboard Compose State (tracks in-progress Vietnamese character)
 * ======================================================================== */

typedef struct {
    BOOL    active;         /* Whether we are composing */
    wchar_t base_char;      /* The base letter typed so far */
    wchar_t tone_char;      /* The tone mark applied */
    wchar_t vowel附加;      /* Additional vowel (e.g. 'a' in 'oa', 'uy' etc.) */
    int     compose_len;    /* Number of chars in compose buffer */
    wchar_t buffer[16];     /* Compose buffer for display */
    DWORD   last_key_time;  /* Timestamp of last keypress */
    DWORD   last_vkey;      /* Last virtual key code */
    DWORD   last_scan;      /* Last scan code */
    UINT    modifiers;      /* Current modifier state */
    BOOL    shift_held;     /* Shift key state for tone cycling */
} EvkComposeState;

/* ========================================================================
 * Global Application Context
 * ======================================================================== */

typedef struct {
    HINSTANCE       hInstance;
    HWND            hwndMain;       /* Hidden main window */
    HWND            hwndTray;       /* Tray notification window */

    HHOOK           hookKeyboard;   /* WH_KEYBOARD_LL hook */
    HANDLE          hThread;        /* Helper thread */
    HANDLE          hEvent;         /* Sync event */

    HICON           hIconBig;
    HICON           hIconSmall;

    HMENU           hMenuTray;      /* System tray popup menu */

    EvkSettings     settings;
    EvkComposeState compose;

    /* Active window tracking */
    HWND            hwndActive;
    WCHAR           active_app[MAX_APP_PATH];
    BOOL            is_blacklisted;

    /* Shell hook */
    UINT            shellHookMsg;

    /* Tray icon state */
    NOTIFYICONDATAW nid;

    /* Dark mode */
    BOOL            darkMode;

    /* DLLs loaded dynamically */
    HMODULE         hUxTheme;
    HMODULE         hComctl32;

} EvkApp;

/* Global app instance */
extern EvkApp g_app;

/* ========================================================================
 * Function Declarations - Main Entry (evk_main.c)
 * ======================================================================== */

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmd, int nShow);
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK DialogWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

/* ========================================================================
 * Function Declarations - Keyboard Hook (evk_hook.c)
 * ======================================================================== */

void     hook_install(void);
void     hook_uninstall(void);
LRESULT  hook_callback(int nCode, WPARAM wp, LPARAM lp);

/* ========================================================================
 * Function Declarations - Vietnamese Input Engine (evk_input.c)
 * ======================================================================== */

void     input_init(EvkComposeState *state);
void     input_reset(EvkComposeState *state);
BOOL     input_process_key(EvkComposeState *state, DWORD vkey, DWORD scan,
                           BOOL keyDown, UINT modifiers);
BOOL     input_backspace(EvkComposeState *state);
wchar_t  input_get_composed_char(EvkComposeState *state);
wchar_t  input_tone_cycle(EvkComposeState *state);
void     input_send_char(wchar_t ch);
void     input_send_backspace(int count);
void     input_send_string(const wchar_t *str);

/* Telex processing */
BOOL     telex_process(EvkComposeState *state, wchar_t key);
/* VNI processing */
BOOL     vni_process(EvkComposeState *state, wchar_t key);

/* ========================================================================
 * Function Declarations - Charset Conversion (evk_charset.c)
 * ======================================================================== */

wchar_t  charset_convert(wchar_t viet_char, EvkCharset charset);
int      charset_convert_string(const wchar_t *input, wchar_t *output,
                                int output_len, EvkCharset charset);
const wchar_t *charset_get_name(EvkCharset id);
int      charset_get_count(void);

/* ========================================================================
 * Function Declarations - System Tray (evk_tray.c)
 * ======================================================================== */

BOOL     tray_create(HWND hwnd);
void     tray_destroy(HWND hwnd);
void     tray_show_menu(HWND hwnd);
void     tray_update_icon(void);
void     tray_set_tooltip(const wchar_t *text);

/* ========================================================================
 * Function Declarations - Settings (evk_settings.c)
 * ======================================================================== */

void     settings_init(EvkSettings *s);
void     settings_load(EvkSettings *s);
void     settings_save(const EvkSettings *s);
BOOL     settings_is_blacklisted(const EvkSettings *s, const wchar_t *app);
void     settings_add_blacklist(EvkSettings *s, const wchar_t *app);
void     settings_remove_blacklist(EvkSettings *s, const wchar_t *app);

/* ========================================================================
 * Function Declarations - Dialogs (evk_dialog.c)
 * ======================================================================== */

void     dialog_show_config(HWND hwndParent);
void     dialog_show_converter(HWND hwndParent);
void     dialog_show_about(HWND hwndParent);

/* ========================================================================
 * Function Declarations - Macros (evk_macro.c)
 * ======================================================================== */

void     macro_init(void);
void     macro_load(const wchar_t *path);
void     macro_free(void);
const wchar_t *macro_lookup(const wchar_t *key, int key_len);

/* ========================================================================
 * Function Declarations - Dark Mode (evk_theme.c)
 * ======================================================================== */

void     theme_init(void);
BOOL     theme_is_dark(void);
void     theme_apply_to_dialog(HWND hwnd);
int      theme_get_sys_color(int idx);

/* ========================================================================
 * Function Declarations - Utility (evk_util.c)
 * ======================================================================== */

BOOL     util_is_vietnamese_char(wchar_t ch);
wchar_t  util_toupper_viet(wchar_t ch);
wchar_t  util_tolower_viet(wchar_t ch);
void     util_get_exe_dir(wchar_t *buf, int buf_len);
void     util_send_keys(wchar_t ch);
void     util_post_key(wchar_t ch);
BOOL     util_is_text_input(HWND hwnd);
HWND     util_get_foreground_input(void);

#endif /* EVK_H */
