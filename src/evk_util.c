/**
 * evk_util.c - Utility Functions
 *
 * Helper functions for Vietnamese character handling, string operations,
 * and Windows API utilities.
 */

#include "evk.h"

/* ========================================================================
 * Vietnamese Character Classification
 * ======================================================================== */

/* Unicode ranges for Vietnamese characters */
#define VN_BASE_MIN     0x0041  /* A */
#define VN_BASE_MAX     0x01B0  /* ư */
#define VN_COMPOSED_MIN 0x1EA0  /* Ạ */
#define VN_COMPOSED_MAX 0x1EF9  /* ỹ */

BOOL util_is_vietnamese_char(wchar_t ch)
{
    /* ASCII a-z, A-Z */
    if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z'))
        return TRUE;

    /* Modified vowels: ă, â, ê, ô, ơ, ư, đ */
    if (ch >= 0x0100 && ch <= 0x01B0)
        return TRUE;

    /* Precomposed Vietnamese: Ạ-ỹ */
    if (ch >= 0x1EA0 && ch <= 0x1EF9)
        return TRUE;

    return FALSE;
}

/* ========================================================================
 * Vietnamese Case Conversion
 * ======================================================================== */

wchar_t util_toupper_viet(wchar_t ch)
{
    /* Simple ASCII case */
    if (ch >= L'a' && ch <= L'z')
        return ch - 32;

    /* Vietnamese vowels: lowercase → uppercase */
    static const struct { wchar_t lower; wchar_t upper; } pairs[] = {
        { L'\x0103', L'\x0102' },  /* ă → Ă */
        { L'\x00E2', L'\x00C2' },  /* â → Â */
        { L'\x00EA', L'\x00CA' },  /* ê → Ê */
        { L'\x00F4', L'\x00D4' },  /* ô → Ô */
        { L'\x01A1', L'\x01A0' },  /* ơ → Ơ */
        { L'\x01B0', L'\x01AF' },  /* ư → Ư */
        { L'\x0111', L'\x0110' },  /* đ → Đ */
        { 0, 0 }
    };

    for (int i = 0; pairs[i].lower; i++) {
        if (ch == pairs[i].lower) return pairs[i].upper;
        if (ch == pairs[i].upper) return pairs[i].upper;
    }

    /* Precomposed Vietnamese: add 0x20 to get lowercase */
    if (ch >= 0x1EA0 && ch <= 0x1EF9 && (ch & 1) == 0)
        return ch; /* Already uppercase-ish */
    if (ch >= 0x1EA1 && ch <= 0x1EF9 && (ch & 1) == 1)
        return ch - 1; /* Lowercase → uppercase */

    return ch;
}

wchar_t util_tolower_viet(wchar_t ch)
{
    if (ch >= L'A' && ch <= L'Z')
        return ch + 32;

    static const struct { wchar_t upper; wchar_t lower; } pairs[] = {
        { L'\x0102', L'\x0103' },  /* Ă → ă */
        { L'\x00C2', L'\x00E2' },  /* Â → â */
        { L'\x00CA', L'\x00EA' },  /* Ê → ê */
        { L'\x00D4', L'\x00F4' },  /* Ô → ô */
        { L'\x01A0', L'\x01A1' },  /* Ơ → ơ */
        { L'\x01AF', L'\x01B0' },  /* Ư → ư */
        { L'\x0110', L'\x0111' },  /* Đ → đ */
        { 0, 0 }
    };

    for (int i = 0; pairs[i].upper; i++) {
        if (ch == pairs[i].upper) return pairs[i].lower;
        if (ch == pairs[i].lower) return pairs[i].lower;
    }

    if (ch >= 0x1EA0 && ch <= 0x1EF9 && (ch & 1) == 0)
        return ch + 1; /* Uppercase → lowercase */

    return ch;
}

/* ========================================================================
 * Path Utilities
 * ======================================================================== */

void util_get_exe_dir(wchar_t *buf, int buf_len)
{
    GetModuleFileNameW(NULL, buf, buf_len);
    /* Remove filename, keep directory */
    wchar_t *slash = wcsrchr(buf, L'\\');
    if (slash) *slash = 0;
}

/* ========================================================================
 * Keyboard Input Simulation
 * ======================================================================== */

/**
 * Send a single Unicode character via SendInput.
 * Uses KEYEVENTF_UNICODE flag for direct Unicode input.
 */
void util_send_keys(wchar_t ch)
{
    INPUT inputs[2];
    memset(inputs, 0, sizeof(inputs));

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wScan = ch;
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wScan = ch;
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));
}

/**
 * Post a character message to the foreground window.
 * Alternative to SendInput for apps that don't handle raw input.
 */
void util_post_key(wchar_t ch)
{
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
        PostMessageW(hwnd, WM_CHAR, (WPARAM)ch, 0);
    }
}

/**
 * Send multiple backspaces to erase characters.
 */
void util_send_backspaces(int count)
{
    INPUT inputs[32];
    memset(inputs, 0, sizeof(inputs));
    int n = 0;

    for (int i = 0; i < count && n < 30; i++) {
        inputs[n].type = INPUT_KEYBOARD;
        inputs[n].ki.wVk = VK_BACK;
        n++;
        inputs[n].type = INPUT_KEYBOARD;
        inputs[n].ki.wVk = VK_BACK;
        inputs[n].ki.dwFlags = KEYEVENTF_KEYUP;
        n++;
    }

    SendInput(n, inputs, sizeof(INPUT));
}

/* ========================================================================
 * Window Detection
 * ======================================================================== */

/**
 * Check if the foreground window is likely a text input field.
 * Returns TRUE if the window has a caret or is an edit control.
 */
BOOL util_is_text_input(HWND hwnd)
{
    if (!hwnd) return FALSE;

    /* Check for common edit control classes */
    WCHAR className[64];
    GetClassNameW(hwnd, className, 64);

    if (_wcsicmp(className, L"Edit") == 0 ||
        _wcsicmp(className, L"RichEdit") == 0 ||
        _wcsicmp(className, L"RichEdit20W") == 0 ||
        _wcsicmp(className, L"RichEdit20A") == 0 ||
        _wcsicmp(className, L"ThunderRT6TextBox") == 0 ||
        _wcsicmp(className, L"TextBox") == 0) {
        return TRUE;
    }

    /* Check if the window has a caret (blinking cursor) */
    GUITHREADINFO gti;
    gti.cbSize = sizeof(GUITHREADINFO);
    DWORD tid = GetWindowThreadProcessId(hwnd, NULL);
    if (GetGUIThreadInfo(tid, &gti)) {
        if (gti.hwndCaret == hwnd || gti.flags & GUI_CARETBLINKING)
            return TRUE;
    }

    return FALSE;
}

/**
 * Get the foreground window that accepts text input.
 * Walks up the window hierarchy to find the input field.
 */
HWND util_get_foreground_input(void)
{
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return NULL;

    /* Check if the foreground window itself accepts text */
    if (util_is_text_input(hwnd))
        return hwnd;

    /* Try to find a child edit control */
    HWND hChild = GetWindow(hwnd, GW_CHILD);
    while (hChild) {
        if (util_is_text_input(hChild))
            return hChild;
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }

    return hwnd; /* Return the main window as fallback */
}
