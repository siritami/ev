/**
 * evk_input.c - Vietnamese Input Engine
 *
 * Core logic for processing keystrokes and composing Vietnamese characters.
 * Supports Telex and VNI input methods.
 *
 * The engine works by maintaining a "compose state" that tracks:
 *   1. The base vowel/letter typed so far
 *   2. Any compound vowel being built (e.g. "oa" → "ơa")
 *   3. The tone mark applied
 *   4. Whether the composition is active
 *
 * When a complete Vietnamese character is formed, it sends the output
 * via simulated keystrokes (SendInput) or clipboard paste.
 */

#include "evk.h"
#include "evk_table.h"

/* ========================================================================
 * Compose State Management
 * ======================================================================== */

void input_init(EvkComposeState *state)
{
    memset(state, 0, sizeof(*state));
    state->active = FALSE;
    state->base_char = 0;
    state->tone_char = 0;
    state->compose_len = 0;
    state->last_key_time = 0;
}

void input_reset(EvkComposeState *state)
{
    if (state->compose_len > 0) {
        /* Flush any pending composition by sending raw characters */
        for (int i = 0; i < state->compose_len; i++) {
            if (state->buffer[i]) {
                input_send_char(state->buffer[i]);
            }
        }
    }
    input_init(state);
}

/* ========================================================================
 * Telex Input Method
 *
 * Telex rules:
 *   - Double vowels: aa→ă, ee→ê, oo→ô, ow→ơ, uw→ư
 *   - Double consonant: dd→đ
 *   - Tone marks: f=huyền, s=sắc, r=hỏi, x=ngã, j=nặng
 *   - z after vowel removes tone
 *   - w after d → đ (dd)
 *   - w after vowels → compound vowel indicator (e.g. ow→ơ)
 *
 * ======================================================================== */

BOOL telex_process(EvkComposeState *state, wchar_t key)
{
    wchar_t lower_key = (wchar_t)CharLowerW((LPWSTR)(ULONG_PTR)key);

    /* ---- Tone mark keys (only valid after a vowel) ---- */
    if (state->base_char && wcschr(telex_tone_keys, lower_key)) {
        VnTone new_tone = TONE_NONE;

        for (int i = 0; telex_tones[i].key; i++) {
            if (telex_tones[i].key == lower_key) {
                new_tone = telex_tones[i].tone;
                break;
            }
        }

        if (new_tone != TONE_NONE) {
            /* z = remove tone */
            if (lower_key == L'z') {
                state->tone_char = 0;
            } else {
                /* If same tone pressed twice, cycle: e.g. aa→a (remove) */
                if (state->tone_char == vn_apply_tone(state->base_char, new_tone)) {
                    state->tone_char = 0;
                } else {
                    state->tone_char = vn_apply_tone(state->base_char, new_tone);
                }
            }
            return TRUE; /* Compose state updated, don't send key */
        }
    }

    /* ---- Double-letter vowels: aa, ee, oo ---- */
    if (state->compose_len > 0 && state->base_char == lower_key) {
        /* Check for special compound: dd→đ */
        if (lower_key == L'd' && state->compose_len == 1) {
            /* dd → đ */
            state->buffer[0] = L'\x0111';
            state->base_char = L'\x0111';
            state->compose_len = 1;
            return TRUE;
        }

        /* Check vowel doubling: aa→ă, ee→ê, oo→ô */
        for (int i = 0; telex_vowels[i].input; i++) {
            if (telex_vowels[i].input[0] == lower_key &&
                telex_vowels[i].input[1] == lower_key) {
                state->buffer[0] = telex_vowels[i].output;
                state->base_char = telex_vowels[i].output;
                state->compose_len = 1;
                return TRUE;
            }
        }
    }

    /* ---- w after vowel → compound vowel (ơ, ư) ---- */
    if (state->base_char && lower_key == L'w') {
        /* ow → ơ, uw → ư */
        if (state->base_char == L'o' && state->compose_len == 1) {
            state->buffer[0] = L'\x01A1'; /* ơ */
            state->base_char = L'\x01A1';
            return TRUE;
        }
        if (state->base_char == L'u' && state->compose_len == 1) {
            state->buffer[0] = L'\x01B0'; /* ư */
            state->base_char = L'\x01B0';
            return TRUE;
        }
        /* aw → ă (alternative to aa) */
        if (state->base_char == L'a' && state->compose_len == 1) {
            state->buffer[0] = L'\x0103'; /* ă */
            state->base_char = L'\x0103';
            return TRUE;
        }
        /* After other vowels: w starts a compound */
        if (state->compose_len == 1) {
            state->vowel_extra = L'w';
            state->compose_len = 2;
            state->buffer[1] = L'w';
            return TRUE;
        }
    }

    /* ---- d→đ in single keystroke (when not composing 'd') ---- */
    if (lower_key == L'd' && !state->active) {
        /* Start composing 'd' - might become 'đ' if followed by 'd' */
        state->active = TRUE;
        state->base_char = L'd';
        state->buffer[0] = L'd';
        state->compose_len = 1;
        return TRUE;
    }

    /* ---- Check if key starts a Vietnamese letter ---- */
    if (!state->active) {
        for (int i = 0; vn_start_chars[i]; i++) {
            if (vn_start_chars[i] == lower_key) {
                /* Start composition */
                state->active = TRUE;
                state->base_char = lower_key;
                state->buffer[0] = lower_key;
                state->compose_len = 1;
                return TRUE;
            }
        }
        return FALSE; /* Not a Vietnamese letter, let it pass through */
    }

    /* ---- Compound vowel: second letter after initial vowel ---- */
    if (state->active && state->compose_len == 1 && state->base_char) {
        /* Check if this key forms a compound vowel with the base */
        for (int i = 0; compound_vowels[i].input; i++) {
            if (compound_vowels[i].base == state->base_char &&
                compound_vowels[i].input[1] == lower_key) {
                state->vowel_extra = lower_key;
                state->compose_len = 2;
                state->buffer[1] = lower_key;
                return TRUE;
            }
        }
    }

    /* ---- Letter not part of current composition: flush and restart ---- */
    if (state->active) {
        /* Flush current composition */
        input_reset(state);
        /* Start new composition if this is a valid start */
        for (int i = 0; vn_start_chars[i]; i++) {
            if (vn_start_chars[i] == lower_key) {
                state->active = TRUE;
                state->base_char = lower_key;
                state->buffer[0] = lower_key;
                state->compose_len = 1;
                return TRUE;
            }
        }
        return FALSE;
    }

    return FALSE;
}

/* ========================================================================
 * VNI Input Method
 *
 * VNI uses digit keys 0-9 after vowels:
 *   1=huyền, 2=sắc, 3=hỏi, 4=ngã, 5=nặng, 0=remove
 *   6=ă, 7=â, 8=ê, 9=ô
 *
 * ======================================================================== */

BOOL vni_process(EvkComposeState *state, wchar_t key)
{
    /* VNI digit tone keys */
    for (int i = 0; vni_tones[i].key; i++) {
        if (vni_tones[i].key == key && state->base_char) {
            VnTone new_tone = vni_tones[i].tone;

            if (new_tone == TONE_NONE) {
                state->tone_char = 0;
            } else {
                state->tone_char = vn_apply_tone(state->base_char, new_tone);
            }
            return TRUE;
        }
    }

    /* VNI special vowel digits */
    if (state->active && state->base_char) {
        wchar_t special = 0;
        switch (key) {
        case L'6': special = L'\x0103'; break; /* ă */
        case L'7': special = L'\x00E2'; break; /* â */
        case L'8': special = L'\x00EA'; break; /* ê */
        case L'9': special = L'\x00F4'; break; /* ô */
        }

        if (special) {
            state->base_char = special;
            state->buffer[0] = special;
            state->compose_len = 1;
            return TRUE;
        }
    }

    /* Regular letter: check if it starts Vietnamese composition */
    if (!state->active) {
        wchar_t lower_key = (wchar_t)CharLowerW((LPWSTR)(ULONG_PTR)key);
        for (int i = 0; vn_start_chars[i]; i++) {
            if (vn_start_chars[i] == lower_key) {
                state->active = TRUE;
                state->base_char = lower_key;
                state->buffer[0] = lower_key;
                state->compose_len = 1;
                return TRUE;
            }
        }
    }

    /* VNI: digits after non-vowel = flush + pass through */
    if (state->active && !state->base_char) {
        input_reset(state);
        return FALSE;
    }

    return FALSE;
}

/* ========================================================================
 * Main Key Processing Entry Point
 *
 * Called from the keyboard hook for each key event.
 * Returns TRUE if the key was consumed (should not pass through).
 * ======================================================================== */

BOOL input_process_key(EvkComposeState *state, DWORD vkey, DWORD scan,
                       BOOL keyDown, UINT modifiers)
{
    EvkApp *app = &g_app;

    /* Only process on key down */
    if (!keyDown)
        return FALSE;

    /* Track timing for timeout detection (500ms gap = reset) */
    DWORD now = GetTickCount();
    if (state->active && (now - state->last_key_time > 500)) {
        input_reset(state);
    }
    state->last_key_time = now;

    /* Shift release after typing a vowel → cycle tone */
    /* (handled in key up, skip here) */

    /* Convert VK code to character */
    BYTE keyState[256];
    GetKeyboardState(keyState);
    WCHAR chars[4];
    int charCount = ToUnicode(vkey, scan, keyState, chars, 4, 0);

    if (charCount != 1)
        return FALSE;

    wchar_t ch = chars[0];

    /* Process based on current input method */
    BOOL consumed = FALSE;
    switch (app->settings.input_method) {
    case METHOD_TELEX:
        consumed = telex_process(state, ch);
        break;
    case METHOD_VNI:
        consumed = vni_process(state, ch);
        break;
    case METHOD_VIQR:
        /* VIQR processing (simplified) */
        consumed = telex_process(state, ch); /* Fallback to Telex */
        break;
    default:
        break;
    }

    return consumed;
}

/* ========================================================================
 * Backspace Handling
 *
 * When backspace is pressed during composition:
 *   - If composing a tone → remove tone
 *   - If composing a compound vowel → simplify to single vowel
 *   - If composing a single vowel → end composition, pass backspace
 * ======================================================================== */

BOOL input_backspace(EvkComposeState *state)
{
    if (!state->active || state->compose_len == 0)
        return FALSE;

    if (state->tone_char) {
        /* Remove tone first */
        state->tone_char = 0;
        return TRUE;
    }

    if (state->compose_len > 1) {
        /* Simplify compound vowel to single vowel */
        state->vowel_extra = 0;
        state->compose_len = 1;
        state->buffer[1] = 0;
        return TRUE;
    }

    /* End composition entirely, let backspace through */
    input_init(state);
    return FALSE;
}

/* ========================================================================
 * Get the final composed Vietnamese character
 *
 * Combines base vowel + tone into a single precomposed Unicode character.
 * ======================================================================== */

wchar_t input_get_composed_char(EvkComposeState *state)
{
    if (!state->active || state->compose_len == 0)
        return 0;

    wchar_t base = state->buffer[0];

    /* If a tone has been applied, get the precomposed character */
    if (state->tone_char) {
        return state->tone_char;
    }

    /* No tone - return the base character */
    return base;
}

/* ========================================================================
 * Output: Send Vietnamese Character
 *
 * Sends the composed character by:
 *   1. First sending backspaces to erase the compose buffer
 *   2. Then sending the final character via SendInput
 * ======================================================================== */

void input_send_char(wchar_t ch)
{
    INPUT inputs[4];
    memset(inputs, 0, sizeof(inputs));

    /* If it's a simple ASCII character, send it directly */
    if (ch >= 0x20 && ch <= 0x7E) {
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = 0;
        inputs[0].ki.wScan = ch;
        inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[0].ki.time = 0;
        SendInput(1, inputs, sizeof(INPUT));
        return;
    }

    /* Unicode character: use clipboard paste for reliability */
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t) * 2);
        if (hMem) {
            wchar_t *p = (wchar_t *)GlobalLock(hMem);
            p[0] = ch;
            p[1] = 0;
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();

        /* Send Ctrl+V to paste */
        memset(inputs, 0, sizeof(inputs));
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_CONTROL;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = 'V';
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = 'V';
        inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = VK_CONTROL;
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(4, inputs, sizeof(INPUT));
    }
}

void input_send_backspace(int count)
{
    INPUT inputs[32];
    memset(inputs, 0, sizeof(inputs));
    int n = 0;

    for (int i = 0; i < count && n < 30; i++) {
        inputs[n].type = INPUT_KEYBOARD;
        inputs[n].ki.wVk = VK_BACK;
        inputs[n].ki.dwFlags = 0;
        n++;
        inputs[n].type = INPUT_KEYBOARD;
        inputs[n].ki.wVk = VK_BACK;
        inputs[n].ki.dwFlags = KEYEVENTF_KEYUP;
        n++;
    }

    SendInput(n, inputs, sizeof(INPUT));
}

void input_send_string(const wchar_t *str)
{
    for (const wchar_t *p = str; *p; p++) {
        input_send_char(*p);
    }
}
