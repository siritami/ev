/**
 * evk_table.h - Vietnamese Character Conversion Tables
 *
 * Lookup tables for mapping Vietnamese input sequences to composed characters.
 * Supports Telex, VNI, and VIQR input methods, plus charset output tables.
 *
 * Vietnamese has 6 tones and 12 base vowels (a, ă, â, e, ê, i, o, ô, ơ, u, ư, y)
 * plus consonants đ, plus multipart vowels (oa, oe, uy, uya, etc.)
 */

#ifndef EVK_TABLE_H
#define EVK_TABLE_H

#include "evk.h"

/* ========================================================================
 * Telex Input Method Mapping
 *
 * Telex uses letter keys to add tones:
 *   f = huyền (grave)    aa → à
 *   s = sắc (acute)      as → á
 *   r = hỏi (hook)       ar → ả
 *   x = ngã (tilde)      ax → ã
 *   j = nặng (dot)       aj → ạ
 *   z = remove tone      az → a
 *
 * Double letters create new vowels:
 *   aa → ă, ee → ê, oo → ô, ow → ơ, uw → ư
 *
 * ======================================================================== */

/* Telex vowel base combinations → special vowel */
typedef struct {
    const wchar_t *input;    /* Typed sequence */
    wchar_t        output;   /* Base vowel result */
    int            len;      /* Length of input sequence */
} TelexVowelMap;

static const TelexVowelMap telex_vowels[] = {
    { L"aw",  L'\x0103', 2 },  /* ă */
    { L"aa",  L'\x0103', 2 },  /* ă (alternative) */
    { L"ee",  L'\x00EA', 2 },  /* ê */
    { L"oo",  L'\x00F4', 2 },  /* ô */
    { L"ow",  L'\x01A1', 2 },  /* ơ */
    { L"uw",  L'\x01B0', 2 },  /* ư */
    { L"dd",  L'\x0111', 2 },  /* đ */
    { NULL, 0, 0 }
};

/* Telex key → tone mark */
typedef struct {
    wchar_t  key;       /* Telex key: f, s, r, x, j, z */
    VnTone   tone;      /* Tone to apply */
} TelexToneMap;

static const TelexToneMap telex_tones[] = {
    { L'f', TONE_GRAVE },   /* huyền */
    { L's', TONE_ACUTE },   /* sắc */
    { L'r', TONE_HOOK },    /* hỏi */
    { L'x', TONE_TILDE },   /* ngã */
    { L'j', TONE_DOT },     /* nặng */
    { L'z', TONE_NONE },    /* remove tone */
    { 0, 0 }
};

/* ========================================================================
 * VNI Input Method Mapping
 *
 * VNI uses digit keys:
 *   1 = huyền, 2 = sắc, 3 = hỏi, 4 = ngã, 5 = nặng, 0 = remove
 *   6 = ă, 7 = â, 8 = ê, 9 = ô, 0 (after vowel) = ơ, (none) = ư
 *
 * ======================================================================== */

typedef struct {
    wchar_t  key;       /* VNI digit key */
    VnTone   tone;
} VniToneMap;

static const VniToneMap vni_tones[] = {
    { L'1', TONE_GRAVE },
    { L'2', TONE_ACUTE },
    { L'3', TONE_HOOK },
    { L'4', TONE_TILDE },
    { L'5', TONE_DOT },
    { L'0', TONE_NONE },
    { 0, 0 }
};

/* ========================================================================
 * VIQR Input Method Mapping
 *
 * VIQR uses ASCII punctuation marks after vowels:
 *   '  = sắc, `  = huyền, ? = hỏi, ~ = ngã, . = nặng
 *   (  = ă,  ) = â,  * = ê,  + = ô,  = = ơ,  (none) = ư
 *   dd = đ
 *
 * ======================================================================== */

typedef struct {
    wchar_t  key;
    VnTone   tone;
} ViqrToneMap;

static const ViqrToneMap viqr_tones[] = {
    { L'\'', TONE_ACUTE },
    { L'`',  TONE_GRAVE },
    { L'?',  TONE_HOOK },
    { L'~',  TONE_TILDE },
    { L'.',  TONE_DOT },
    { L'/',  TONE_NONE },  /* remove */
    { 0, 0 }
};

/* ========================================================================
 * Complete Vietnamese Character Table
 *
 * Maps: base vowel × tone → precomposed Unicode character
 * 12 vowels × 5 tones + 12 base = 72 entries + consonants
 * ======================================================================== */

typedef struct {
    wchar_t  base;
    wchar_t  grave;     /* huyền  - grave */
    wchar_t  acute;     /* sắc    - acute */
    wchar_t  hook;      /* hỏi    - hook above */
    wchar_t  tilde;     /* ngã    - tilde */
    wchar_t  dot;       /* nặng    - dot below */
} VowelRow;

static const VowelRow vn_vowel_table[] = {
    /* base   grave    acute    hook     tilde    dot     */
    { L'a',  L'\x00E0', L'\x00E1', L'\x00E3', L'\x00E3', L'\x1EA1' },  /* a */
    { L'\x0103', L'\x1EB1', L'\x1EAF', L'\x1EB3', L'\x1EB5', L'\x1EB7' },  /* ă */
    { L'\x00E2', L'\x1EA7', L'\x1EA5', L'\x1EA9', L'\x1EAB', L'\x1EAD' },  /* â */
    { L'e',  L'\x00E8', L'\x00E9', L'\x1EBB', L'\x1EBD', L'\x1EB9' },  /* e */
    { L'\x00EA', L'\x1EC1', L'\x1EBF', L'\x1EC3', L'\x1EC5', L'\x1EC7' },  /* ê */
    { L'i',  L'\x00EC', L'\x00ED', L'\x1EC9', L'\x1EC9', L'\x1ECB' },  /* i */
    { L'o',  L'\x00F2', L'\x00F3', L'\x1ECF', L'\x1ED1', L'\x1ED3' },  /* o */
    { L'\x00F4', L'\x1ED9', L'\x1ED7', L'\x1EDB', L'\x1EDD', L'\x1EDF' },  /* ô */
    { L'\x01A1', L'\x1EDD', L'\x1EDB', L'\x1EDF', L'\x1EE1', L'\x1EE3' },  /* ơ */
    { L'u',  L'\x00F9', L'\x00FA', L'\x1EE7', L'\x1EE9', L'\x1EEB' },  /* u */
    { L'\x01B0', L'\x1EEB', L'\x1EE9', L'\x1EED', L'\x1EEF', L'\x1EF1' },  /* ư */
    { L'y',  L'\x1EF3', L'\x00FD', L'\x1EF7', L'\x1EF9', L'\x1EF5' },  /* y */
    { L'd',  L'\x0111', L'\x0111', L'\x0111', L'\x0111', L'\x0111' },  /* đ (always same) */
    { 0, 0, 0, 0, 0, 0 }
};

/* ========================================================================
 * Compound Vowel Table
 *
 * When a vowel follows an initial vowel, some combinations form compound
 * vowels that have their own precomposed forms.
 *
 * Example: o + a → oa (separate chars), but ơ + a → ơa (compound)
 *
 * This table maps compound vowel sequences to their composed forms.
 * ======================================================================== */

typedef struct {
    const wchar_t *input;   /* Compound vowel input sequence */
    wchar_t        base;    /* First vowel character */
    wchar_t        second;  /* Second vowel character */
} CompoundVowel;

static const CompoundVowel compound_vowels[] = {
    /* Compounds starting with ơ */
    { L"oa",  L'\x01A1', L'a' },
    { L"oe",  L'\x01A1', L'e' },
    { L"oi",  L'\x01A1', L'i' },
    { L"oo",  L'\x01A1', L'o' },
    { L"ou",  L'\x01A1', L'u' },
    /* Compounds starting with ô */
    { L"oa",  L'\x00F4', L'a' },
    { L"oe",  L'\x00F4', L'e' },
    /* Compounds starting with u */
    { L"ua",  L'u', L'a' },
    { L"ue",  L'u', L'e' },
    { L"ui",  L'u', L'i' },
    { L"uo",  L'u', L'o' },
    { L"uy",  L'u', L'y' },
    /* Compounds starting with ư */
    { L"uya", L'\x01B0', L'a' },
    { L"uyê", L'\x01B0', L'\x00EA' },
    { L"uyn", L'\x01B0', L'n' },
    /* Compounds starting with a */
    { L"ai",  L'a', L'i' },
    { L"ao",  L'a', L'o' },
    { L"au",  L'a', L'u' },
    { L"ay",  L'a', L'y' },
    /* Compounds starting with ă */
    { L"\x0103i", L'\x0103', L'i' },
    { L"\x0103u", L'\x0103', L'u' },
    { L"\x0103y", L'\x0103', L'y' },
    /* Compounds starting with â */
    { L"\x00E2u", L'\x00E2', L'u' },
    { L"\x00E2y", L'\x00E2', L'y' },
    /* Compounds starting with e */
    { L"eo",  L'e', L'o' },
    /* Compounds starting with ê */
    { L"\x00EAu", L'\x00EA', L'u' },
    /* Compounds starting with o */
    { L"oa",  L'o', L'a' },
    { L"oe",  L'o', L'e' },
    { L"oi",  L'o', L'i' },
    { L"on",  L'o', L'n' },
    { L"om",  L'o', L'm' },
    { L"op",  L'o', L'p' },
    { L"ot",  L'o', L't' },
    { NULL, 0, 0 }
};

/* ========================================================================
 * Telex Special Keys
 *
 * In Telex, some keys have dual purpose:
 *   w after consonants → special vowels (ow→ơ, uw→ư)
 *   w after 'd' → đ → dd
 *   f/s/r/x/j can add tones to already-typed vowel
 * ======================================================================== */

/* Characters that can start a Vietnamese word */
static const int vn_start_chars[] = {
    L'a', L'\x0103', L'\x00E2',  /* a, ă, â */
    L'd', L'\x0111',             /* d, đ */
    L'e', L'\x00EA',             /* e, ê */
    L'i',
    L'o', L'\x00F4', L'\x01A1',  /* o, ô, ơ */
    L'u', L'\x01B0',             /* u, ư */
    L'y',
    0
};

/* Characters that can follow a vowel (tone markers in Telex) */
static const wchar_t telex_tone_keys[] = L"fsrxjz";

/* Characters that can form compound vowels after base vowels */
static const wchar_t telex_compound_keys[] = L"aeiouyw";

/* ========================================================================
 * Vowel lookup helpers
 * ======================================================================== */

/**
 * Look up a character in the vowel table.
 * Returns pointer to the VowelRow, or NULL if not a vowel.
 */
static const VowelRow *vn_find_vowel(wchar_t ch)
{
    for (int i = 0; vn_vowel_table[i].base; i++) {
        if (vn_vowel_table[i].base == ch)
            return &vn_vowel_table[i];
    }
    return NULL;
}

/**
 * Apply a tone to a base vowel character.
 * Returns the precomposed character, or 0 if not found.
 */
static wchar_t vn_apply_tone(wchar_t base, VnTone tone)
{
    const VowelRow *row = vn_find_vowel(base);
    if (!row || tone == TONE_NONE || tone >= TONE_COUNT)
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

/**
 * Check if a character is a Vietnamese vowel (including modified vowels).
 */
static int vn_is_vowel(wchar_t ch)
{
    return vn_find_vowel(ch) != NULL;
}

/**
 * Check if a character is a Vietnamese consonant that can be doubled.
 * In Telex: dd → đ, ee → ê, oo → ô, ow → ơ, uw → ư, aw → ă
 */
static int vn_can_double(wchar_t ch)
{
    return (ch == L'd' || ch == L'a' || ch == L'e' ||
            ch == L'o' || ch == L'u');
}

#endif /* EVK_TABLE_H */
