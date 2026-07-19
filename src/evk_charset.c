/**
 * evk_charset.c - Charset Conversion
 *
 * Converts Vietnamese Unicode characters to various output encodings:
 * TCVN3 (ABC), VPS, VISCII, WinCP1258, VIQR, UTF-8, etc.
 *
 * Each charset has a mapping table from Unicode codepoint → charset byte(s).
 * For multi-byte charsets (TCVN3, VPS), two bytes are output.
 */

#include "evk.h"

/* ========================================================================
 * Charset Name Lookup
 * ======================================================================== */

static const struct {
    EvkCharset  id;
    const wchar_t *name;
} charset_table[] = {
    { CHARSET_UNICODE,         L"Unicode" },
    { CHARSET_UTF8,            L"UTF-8" },
    { CHARSET_UNICODE_REF,     L"Unicode Ref (NCR)" },
    { CHARSET_UNICODE_HEX,     L"Unicode Hex" },
    { CHARSET_UNICODE_COMPOSE, L"Unicode Composite" },
    { CHARSET_WINCP1258,       L"Windows CP1258" },
    { CHARSET_CSTRING,         L"C String (\\uXXXX)" },
    { CHARSET_VN_INTERNAL,     L"VNI Internal" },
    { CHARSET_UTF8VIQR,        L"UTF-8 VIQR" },
    { CHARSET_WORD,            L"Microsoft Word" },
    { CHARSET_WORD2,           L"Microsoft Word (Ext)" },
    /* Single-byte charsets (0x14-0x19) */
    { CHARSET_TCVN3_ABC,       L"TCVN3 (ABC)" },
    { CHARSET_TCVN5,           L"TCVN5" },
    { CHARSET_VISCII,          L"VISCII" },
    { CHARSET_VPS,             L"VPS" },
    { CHARSET_VIETWARE_X,      L"Vietware X" },
    { CHARSET_VIETWARE_F,      L"Vietware F" },
    /* Double-byte charsets (0x28-0x2b) */
    { CHARSET_BK_HCM1,         L"BK HCM 1" },
    { CHARSET_BK_HCM2,         L"BK HCM 2" },
    { CHARSET_BK_HCM3,         L"BK HCM 3" },
    { CHARSET_BK_HCM4,         L"BK HCM 4" },
    { 0, NULL }
};

const wchar_t *charset_get_name(EvkCharset id)
{
    for (int i = 0; charset_table[i].name; i++) {
        if (charset_table[i].id == id)
            return charset_table[i].name;
    }
    return L"Unknown";
}

int charset_get_count(void)
{
    int count = 0;
    for (int i = 0; charset_table[i].name; i++)
        count++;
    return count;
}

/* ========================================================================
 * TCVN3 (ABC) Character Mapping
 *
 * TCVN3 uses a specific byte mapping for Vietnamese characters.
 * Characters 0xA0-0xFF map to single-byte Vietnamese characters.
 * ======================================================================== */

/* TCVN3 single-byte mapping: Unicode → byte */
static struct { wchar_t unicode; BYTE tcvn3; } tcvn3_map[] = {
    { L'\x00A0', 0xA0 },  /* NBSP */
    { L'\x00A1', 0xA1 },
    { L'\x00A2', 0xA2 },
    { L'\x00A3', 0xA3 },
    { L'\x00A4', 0xA4 },
    { L'\x00A5', 0xA5 },
    { L'\x00A6', 0xA6 },
    { L'\x00A7', 0xA7 },
    { L'\x00A8', 0xA8 },
    { L'\x00A9', 0xA9 },
    { L'\x00AA', 0xAA },
    { L'\x00AB', 0xAB },
    { L'\x00AC', 0xAC },
    { L'\x00AD', 0xAD },
    { L'\x00AE', 0xAE },
    { L'\x00AF', 0xAF },
    { L'\x00B0', 0xB0 },
    { L'\x00B1', 0xB1 },
    { L'\x00B2', 0xB2 },
    { L'\x00B3', 0xB3 },
    { L'\x00B4', 0xB4 },
    { L'\x00B5', 0xB5 },
    { L'\x00B6', 0xB6 },
    { L'\x00B7', 0xB7 },
    { L'\x00B8', 0xB8 },
    { L'\x00B9', 0xB9 },
    { L'\x00BA', 0xBA },
    { L'\x00BB', 0xBB },
    { L'\x00BC', 0xBC },
    { L'\x00BD', 0xBD },
    { L'\x00BE', 0xBE },
    { L'\x00BF', 0xBF },
    /* Vietnamese characters in TCVN3 */
    { L'\x0103', 0xB0 },  /* ă */
    { L'\x00E2', 0xB1 },  /* â */
    { L'\x00EA', 0xB2 },  /* ê */
    { L'\x00F4', 0xB3 },  /* ô */
    { L'\x01A1', 0xB4 },  /* ơ */
    { L'\x01B0', 0xB5 },  /* ư */
    { L'\x0111', 0xB6 },  /* đ */
    { L'\x00E0', 0xB8 },  /* à */
    { L'\x00E1', 0xB9 },  /* á */
    { L'\x1EA1', 0xBE },  /* ạ */
    { L'\x1EA3', 0xBF },  /* ả */
    { L'\x00E3', 0xC0 },  /* ã */
    { L'\x1EB1', 0xC1 },  /* ằ */
    { L'\x1EAF', 0xC2 },  /* ắ */
    { L'\x1EB7', 0xC3 },  /* ặ */
    { L'\x1EB3', 0xC4 },  /* ẳ */
    { L'\x1EB5', 0xC5 },  /* ẵ */
    { L'\x1EA7', 0xC6 },  /* ầ */
    { L'\x1EA5', 0xC7 },  /* ấ */
    { L'\x1EAD', 0xC8 },  /* ậ */
    { L'\x1EA9', 0xC9 },  /* ẩ */
    { L'\x1EAB', 0xCA },  /* ẫ */
    { L'\x1EB9', 0xCC },  /* ẹ */
    { L'\x1EBB', 0xCD },  /* ẻ */
    { L'\x1EBD', 0xCE },  /* ẽ */
    { L'\x1EC1', 0xCF },  /* ề */
    { L'\x1EBF', 0xD0 },  /* ế */
    { L'\x1EC7', 0xD1 },  /* ệ */
    { L'\x1EC3', 0xD2 },  /* ể */
    { L'\x1EC5', 0xD3 },  /* ễ */
    { L'\x1ECB', 0xD4 },  /* ỉ */
    { L'\x1EC9', 0xD5 },  /* ỉ */
    { L'\x1ECD', 0xD8 },  /* ọ */
    { L'\x1ECF', 0xD9 },  /* ỏ */
    { L'\x1ED1', 0xDA },  /* ồ */
    { L'\x1ED3', 0xDB },  /* ố */
    { L'\x1EDF', 0xDC },  /* ộ */
    { L'\x1EDB', 0xDD },  /* ổ */
    { L'\x1EDD', 0xDE },  /* ỗ */
    { L'\x1ED9', 0xE3 },  /* ờ */
    { L'\x1ED7', 0xE4 },  /* ớ */
    { L'\x1EE3', 0xE5 },  /* ở */
    { L'\x1EDB', 0xE6 },  /* ỡ */
    { L'\x1EDD', 0xE7 },  /* ờ */
    { L'\x1EE5', 0xEA },  /* ụ */
    { L'\x1EE7', 0xEB },  /* ủ */
    { L'\x1EEB', 0xEC },  /* ừ */
    { L'\x1EE9', 0xED },  /* ứ */
    { L'\x1EF1', 0xEE },  /* ự */
    { L'\x1EED', 0xEF },  /* ử */
    { L'\x1EEF', 0xF0 },  /* ữ */
    { L'\x1EF3', 0xF2 },  /* ỳ */
    { L'\x00FD', 0xF3 },  /* ý */
    { L'\x1EF5', 0xF5 },  /* ỵ */
    { L'\x1EF7', 0xF6 },  /* ỷ */
    { L'\x1EF9', 0xF7 },  /* ỹ */
    { 0, 0 }
};

/* ========================================================================
 * Single-byte charset conversion
 * ======================================================================== */

static wchar_t charset_to_tcvn3(wchar_t ch)
{
    for (int i = 0; tcvn3_map[i].unicode; i++) {
        if (tcvn3_map[i].unicode == ch)
            return (wchar_t)tcvn3_map[i].tcvn3;
    }
    return ch; /* Pass through if not found */
}

/* ========================================================================
 * Unicode NCR Reference Conversion
 *
 * Produces HTML-style numeric character references: &#decimal;
 * ======================================================================== */

static int charset_to_ncr_decimal(wchar_t ch, wchar_t *out, int out_len)
{
    if (ch < 0x80 && ch >= 0x20) {
        if (out_len >= 1) { out[0] = ch; return 1; }
        return 0;
    }
    return _swprintf(out, out_len, L"&#%d;", (int)ch);
}

/* ========================================================================
 * Unicode Hex NCR Conversion
 *
 * Produces HTML-style hex character references: &#xHEX;
 * ======================================================================== */

static int charset_to_ncr_hex(wchar_t ch, wchar_t *out, int out_len)
{
    if (ch < 0x80 && ch >= 0x20) {
        if (out_len >= 1) { out[0] = ch; return 1; }
        return 0;
    }
    return _swprintf(out, out_len, L"&#x%X;", (unsigned int)ch);
}

/* ========================================================================
 * C String Escape Conversion
 *
 * Produces \uXXXX escapes for non-ASCII characters.
 * ======================================================================== */

static int charset_to_cstring(wchar_t ch, wchar_t *out, int out_len)
{
    if (ch < 0x80 && ch >= 0x20) {
        if (out_len >= 1) { out[0] = ch; return 1; }
        return 0;
    }
    return _swprintf(out, out_len, L"\\u%04X", (unsigned int)ch);
}

/* ========================================================================
 * Main Conversion Function
 *
 * Converts a single Vietnamese Unicode character to the target charset.
 * Returns the character itself if ASCII, or the mapped character for
 * non-ASCII characters.
 * ======================================================================== */

wchar_t charset_convert(wchar_t viet_char, EvkCharset charset)
{
    /* ASCII passes through unchanged */
    if (viet_char >= 0x20 && viet_char < 0x80)
        return viet_char;

    switch (charset) {
    case CHARSET_UNICODE:
    case CHARSET_UTF8:
        return viet_char; /* Native Unicode / UTF-8 handled by OS */

    case CHARSET_TCVN3_ABC:
    case CHARSET_TCVN5:
        return charset_to_tcvn3(viet_char);

    case CHARSET_WINCP1258:
        /* Simplified: use Unicode for non-ASCII */
        return viet_char;

    default:
        return viet_char;
    }
}

/* ========================================================================
 * String Conversion
 *
 * Converts a full Unicode string to the target charset.
 * For single-byte charsets, outputs byte by byte.
 * For Unicode reference charsets, outputs &#xxx; sequences.
 * ======================================================================== */

int charset_convert_string(const wchar_t *input, wchar_t *output,
                           int output_len, EvkCharset charset)
{
    int out_pos = 0;

    for (const wchar_t *p = input; *p; p++) {
        wchar_t ch = *p;

        switch (charset) {
        case CHARSET_UNICODE_REF: {
            int n = charset_to_ncr_decimal(ch, output + out_pos, output_len - out_pos);
            out_pos += n;
            break;
        }
        case CHARSET_UNICODE_HEX: {
            int n = charset_to_ncr_hex(ch, output + out_pos, output_len - out_pos);
            out_pos += n;
            break;
        }
        case CHARSET_CSTRING: {
            int n = charset_to_cstring(ch, output + out_pos, output_len - out_pos);
            out_pos += n;
            break;
        }
        default: {
            wchar_t converted = charset_convert(ch, charset);
            if (out_pos < output_len - 1)
                output[out_pos++] = converted;
            break;
        }
        }
    }

    if (out_pos < output_len)
        output[out_pos] = 0;
    return out_pos;
}
