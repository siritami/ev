/**
 * evk_input.c - Vietnamese Input Engine (Rewritten)
 *
 * Core logic for composing Vietnamese characters from keystrokes.
 *
 * HOW IT WORKS:
 *   Base characters (vowels, consonants) pass through normally so
 *   the user can see them in the editor. When a tone key (f/s/r/x/j
 *   in Telex) or special vowel key (dd, ow, etc.) is pressed, the
 *   engine erases the visible characters and replaces them with the
 *   composed Vietnamese character(s).
 *
 *   Example: typing "a" in Telex:
 *     1. Press 'a' -> 'a' appears in editor
 *     2. Press 's' (sac) -> 'a' is erased, 'a' with acute sent
 *
 *   Example: typing "a" with breve in Telex:
 *     1. Press 'a' -> 'a' appears
 *     2. Press 'a' again -> 'a' erased, breve-a sent
 *
 *   Example: typing compound vowel with tone in Telex:
 *     1. Press 'o' -> 'o' appears (start composition)
 *     2. Press 'w' -> 'o' erased, o-hook sent (visible: o-hook)
 *     3. Press 's' -> o-hook erased, o-hook with acute sent
 */

#include "evk.h"
#include "evk_table.h"

/* ========================================================================
 * Compose State Management
 * ======================================================================== */

void input_init(EvkComposeState *state)
{
    state->active = FALSE;
    state->base = 0;
    state->extra = 0;
    state->visible_count = 0;
    state->last_key_time = 0;
}

void input_reset(EvkComposeState *state)
{
    input_init(state);
}

/* ========================================================================
 * Send Characters to the Target Application
 *
 * Uses SendInput with KEYEVENTF_UNICODE for reliable character delivery
 * to any application (including UWP, browsers, terminals, etc.)
 * ======================================================================== */

static void send_unicode_char(wchar_t ch)
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

static void send_backspaces(int count)
{
    INPUT inputs[32];
    memset(inputs, 0, sizeof(inputs));
    int n = 0;

    for (int i = 0; i < count && n + 1 < 32; i++) {
        inputs[n].type = INPUT_KEYBOARD;
        inputs[n].ki.wVk = VK_BACK;
        inputs[n].ki.dwFlags = 0;
        n++;
        inputs[n].type = INPUT_KEYBOARD;
        inputs[n].ki.wVk = VK_BACK;
        inputs[n].ki.dwFlags = KEYEVENTF_KEYUP;
        n++;
    }

    if (n > 0)
        SendInput(n, inputs, sizeof(INPUT));
}

static void send_string(const wchar_t *str)
{
    for (const wchar_t *p = str; *p; p++) {
        send_unicode_char(*p);
    }
}

/* ========================================================================
 * Character Classification
 * ======================================================================== */

static int is_vowel_char(wchar_t ch)
{
    /* Standard Vietnamese vowels */
    if (ch == L'a' || ch == L'\x0103' || ch == L'\x00E2' ||  /* a, a-breve, a-circ */
        ch == L'e' || ch == L'\x00EA' ||                       /* e, e-circ */
        ch == L'i' ||                                           /* i */
        ch == L'o' || ch == L'\x00F4' || ch == L'\x01A1' ||   /* o, o-circ, o-horn */
        ch == L'u' || ch == L'\x01B0' ||                       /* u, u-horn */
        ch == L'y')                                             /* y */
        return 1;
    return 0;
}

/**
 * Find a character in the vowel table.
 * Returns pointer to VowelRow or NULL.
 */
static const VowelRow *find_vowel_row(wchar_t ch)
{
    for (int i = 0; vn_vowel_table[i].base; i++) {
        if (vn_vowel_table[i].base == ch)
            return &vn_vowel_table[i];
    }
    return NULL;
}

/**
 * Apply a tone to a base vowel, returning the precomposed character.
 */
static wchar_t apply_tone(wchar_t base, VnTone tone)
{
    const VowelRow *row = find_vowel_row(base);
    if (!row || tone <= TONE_NONE || tone >= TONE_COUNT)
        return 0;

    switch (tone) {
    case TONE_GRAVE: return row->grave;
    case TONE_ACUTE: return row->acute;
    case TONE_HOOK:  return row->hook;
    case TONE_TILDE: return row->tilde;
    case TONE_DOT:   return row->dot;
    default:         return 0;
    }
}

/* ========================================================================
 * Telex Input Method
 *
 * Returns:
 *   0 = key NOT consumed (pass through to target app)
 *   1 = key CONSUMED (hook suppresses the keystroke)
 *
 * When returning 1, the caller must:
 *   - If state->active: erase visible chars, then send the composed result
 *   - If !state->active: the key was already handled internally
 * ======================================================================== */

static int telex_process(EvkComposeState *state, wchar_t key)
{
    wchar_t k = (wchar_t)(ULONG_PTR)CharLowerW((LPWSTR)(ULONG_PTR)key);

    /* ---- Tone mark keys: f, s, r, x, j ---- */
    if (state->active && is_vowel_char(state->base)) {
        VnTone tone = TONE_NONE;
        if (k == L'f') tone = TONE_GRAVE;
        else if (k == L's') tone = TONE_ACUTE;
        else if (k == L'r') tone = TONE_HOOK;
        else if (k == L'x') tone = TONE_TILDE;
        else if (k == L'j') tone = TONE_DOT;

        if (tone != TONE_NONE) {
            wchar_t composed = apply_tone(state->base, tone);
            if (composed) {
                /* Erase visible characters, send composed result */
                send_backspaces(state->visible_count);
                if (state->extra) {
                    /* Compound vowel: first char passes through, tone on second */
                    send_unicode_char(state->extra);
                    send_unicode_char(composed);
                } else {
                    send_unicode_char(composed);
                }
                input_init(state);
                return 1; /* Consumed */
            }
        }

        /* z = remove tone, finalize without change */
        if (k == L'z') {
            send_backspaces(state->visible_count);
            if (state->extra) {
                send_unicode_char(state->extra);
                send_unicode_char(state->base);
            } else {
                send_unicode_char(state->base);
            }
            input_init(state);
            return 1;
        }
    }

    /* ---- Double-letter special vowels: dd, aa, ee, oo ---- */
    if (state->active && state->base == k && is_vowel_char(state->base)) {
        wchar_t special = 0;

        /* aa -> a-breve, ee -> e-circ, oo -> o-circ */
        if (k == L'a') special = L'\x0103';
        else if (k == L'e') special = L'\x00EA';
        else if (k == L'o') special = L'\x00F4';

        if (special) {
            send_backspaces(state->visible_count);
            send_unicode_char(special);
            state->base = special;
            state->extra = 0;
            state->visible_count = 1;
            return 1; /* Consumed */
        }
    }

    /* ---- dd -> d-stroke ---- */
    if (state->active && state->base == L'd' && k == L'd') {
        send_backspaces(state->visible_count);
        send_unicode_char(L'\x0111');
        state->base = L'\x0111';
        state->extra = 0;
        state->visible_count = 1;
        return 1; /* Consumed */
    }

    /* ---- w after specific vowels -> special vowels ---- */
    if (state->active && k == L'w' && state->visible_count == 1) {
        wchar_t special = 0;
        if (state->base == L'o') special = L'\x01A1';  /* o-horn */
        else if (state->base == L'u') special = L'\x01B0';  /* u-horn */
        else if (state->base == L'a') special = L'\x0103';  /* a-breve */

        if (special) {
            send_backspaces(state->visible_count);
            send_unicode_char(special);
            state->base = special;
            state->extra = 0;
            state->visible_count = 1;
            return 1; /* Consumed */
        }
    }

    /* ---- Compound vowels: 2nd letter after initial vowel ---- */
    if (state->active && state->visible_count == 1 && is_vowel_char(state->base)) {
        if (is_vowel_char(k) && k != state->base) {
            state->extra = k;
            state->visible_count = 2;
            /* Let the second char pass through visibly */
            return 0;
        }
    }

    /* ---- Start new composition on vowel ---- */
    if (!state->active && is_vowel_char(k)) {
        state->active = TRUE;
        state->base = k;
        state->extra = 0;
        state->visible_count = 1;
        /* Let the character pass through visibly */
        return 0;
    }

    /* ---- d starts composition (might become d-stroke) ---- */
    if (!state->active && k == L'd') {
        state->active = TRUE;
        state->base = L'd';
        state->extra = 0;
        state->visible_count = 1;
        /* Let 'd' pass through visibly */
        return 0;
    }

    /* ---- Non-composing key: flush if needed, let through ---- */
    if (state->active) {
        input_init(state);
        return 0;
    }

    return 0;
}

/* ========================================================================
 * VNI Input Method
 *
 * VNI uses digit keys for tones and special vowels:
 *   1=huyen, 2=sac, 3=hoi, 4=nga, 5=nang, 0=remove
 *   6=a-breve, 7=a-circ, 8=e-circ, 9=o-circ
 *   (w after o/u -> o-horn/u-horn)
 * ======================================================================== */

static int vni_process(EvkComposeState *state, wchar_t key)
{
    /* ---- Tone digit keys (1-5, 0) ---- */
    if (state->active && is_vowel_char(state->base)) {
        VnTone tone = TONE_NONE;
        if (key == L'1') tone = TONE_GRAVE;
        else if (key == L'2') tone = TONE_ACUTE;
        else if (key == L'3') tone = TONE_HOOK;
        else if (key == L'4') tone = TONE_TILDE;
        else if (key == L'5') tone = TONE_DOT;
        else if (key == L'0') {
            /* Remove tone: erase and re-send base */
            send_backspaces(state->visible_count);
            if (state->extra) {
                send_unicode_char(state->extra);
                send_unicode_char(state->base);
            } else {
                send_unicode_char(state->base);
            }
            input_init(state);
            return 1;
        }

        if (tone != TONE_NONE) {
            wchar_t composed = apply_tone(state->base, tone);
            if (composed) {
                send_backspaces(state->visible_count);
                if (state->extra) {
                    send_unicode_char(state->extra);
                    send_unicode_char(composed);
                } else {
                    send_unicode_char(composed);
                }
                input_init(state);
                return 1;
            }
        }
    }

    /* ---- Special vowel digit keys (6-9) ---- */
    if (state->active && state->base) {
        wchar_t special = 0;
        if (key == L'6') special = L'\x0103';   /* a-breve */
        else if (key == L'7') special = L'\x00E2';  /* a-circ */
        else if (key == L'8') special = L'\x00EA';  /* e-circ */
        else if (key == L'9') special = L'\x00F4';  /* o-circ */

        if (special) {
            send_backspaces(state->visible_count);
            send_unicode_char(special);
            state->base = special;
            state->extra = 0;
            state->visible_count = 1;
            return 1;
        }
    }

    /* ---- w after o/u -> special horn vowels ---- */
    if (state->active && key == L'w' && state->visible_count == 1) {
        wchar_t special = 0;
        if (state->base == L'o') special = L'\x01A1';
        else if (state->base == L'u') special = L'\x01B0';

        if (special) {
            send_backspaces(state->visible_count);
            send_unicode_char(special);
            state->base = special;
            state->extra = 0;
            state->visible_count = 1;
            return 1;
        }
    }

    /* ---- Start composition on vowel ---- */
    if (!state->active) {
        wchar_t k = (wchar_t)(ULONG_PTR)CharLowerW((LPWSTR)(ULONG_PTR)key);
        if (is_vowel_char(k)) {
            state->active = TRUE;
            state->base = k;
            state->extra = 0;
            state->visible_count = 1;
            return 0;
        }
    }

    /* ---- Flush on non-composing key ---- */
    if (state->active) {
        input_init(state);
        return 0;
    }

    return 0;
}

/* ========================================================================
 * Main Key Processing Entry Point
 *
 * Called from the keyboard hook for each key-down event.
 * Returns 1 if key was consumed (suppress the keystroke).
 * Returns 0 if key should pass through normally.
 *
 * When consuming a key, the function has already sent the replacement
 * characters via SendInput.
 * ======================================================================== */

int input_process_key(EvkComposeState *state, DWORD vkey, DWORD scan,
                      BOOL keyDown, UINT modifiers)
{
    EvkApp *app = &g_app;

    (void)scan;
    (void)modifiers;

    if (!keyDown)
        return 0;

    /* Timeout: if >500ms since last key, reset composition */
    DWORD now = GetTickCount();
    if (state->active && state->last_key_time > 0 &&
        (now - state->last_key_time > 500)) {
        input_init(state);
    }
    state->last_key_time = now;

    /* Skip non-printable keys */
    if (vkey == VK_CONTROL || vkey == VK_SHIFT || vkey == VK_MENU ||
        vkey == VK_LCONTROL || vkey == VK_RCONTROL ||
        vkey == VK_LSHIFT || vkey == VK_RSHIFT ||
        vkey == VK_LMENU || vkey == VK_RMENU ||
        vkey == VK_ESCAPE || vkey == VK_TAB ||
        vkey == VK_DELETE || vkey == VK_INSERT ||
        vkey == VK_HOME || vkey == VK_END ||
        vkey == VK_PRIOR || vkey == VK_NEXT ||
        vkey == VK_LEFT || vkey == VK_RIGHT ||
        vkey == VK_UP || vkey == VK_DOWN ||
        (vkey >= VK_F1 && vkey <= VK_F24) ||
        (vkey >= VK_LWIN && vkey <= VK_APPS)) {
        if (state->active)
            input_init(state);
        return 0;
    }

    /* Handle Backspace */
    if (vkey == VK_BACK && state->active) {
        input_init(state);
        return 0;
    }

    /* Handle Space and Enter: finalize composition */
    if ((vkey == VK_SPACE || vkey == VK_RETURN) && state->active) {
        input_init(state);
        return 0;
    }

    /* Convert VK to character */
    BYTE keyState[256];
    GetKeyboardState(keyState);
    WCHAR chars[4] = {0};
    int charCount = ToUnicode((UINT)vkey, (UINT)scan, keyState, chars, 4, 0);

    if (charCount != 1)
        return 0;

    wchar_t ch = chars[0];

    /* Skip non-printable chars */
    if (ch < 0x20 || ch == 0x7F)
        return 0;

    /* ---- Process through input method ---- */
    int consumed = 0;
    switch (app->settings.input_method) {
    case METHOD_TELEX:
        consumed = telex_process(state, ch);
        break;
    case METHOD_VNI:
        consumed = vni_process(state, ch);
        break;
    case METHOD_VIQR:
        consumed = telex_process(state, ch);
        break;
    default:
        break;
    }

    return consumed;
}

/* ========================================================================
 * Backspace Handling
 * ======================================================================== */

int input_backspace(EvkComposeState *state)
{
    if (!state->active)
        return 0;

    input_init(state);
    return 0;
}

/* ========================================================================
 * Get the composed character (for display/status purposes)
 * ======================================================================== */

wchar_t input_get_composed_char(EvkComposeState *state)
{
    if (!state->active || !state->base)
        return 0;
    return state->base;
}

/* ========================================================================
 * Public send functions (used by hook and other modules)
 * ======================================================================== */

void input_send_char(wchar_t ch)
{
    send_unicode_char(ch);
}

void input_send_backspace(int count)
{
    send_backspaces(count);
}

void input_send_string(const wchar_t *str)
{
    send_string(str);
}
