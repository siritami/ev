/**
 * evk_macro.c - Macro System
 *
 * Loads and manages text macros from evkmacro.txt.
 * Macros allow users to define custom text expansions.
 *
 * Format of evkmacro.txt:
 *   Each line is: <expansion text>
 *   Lines starting with # are comments
 *   Empty lines are ignored
 */

#include "evk.h"

#define MAX_MACRO_LEN   256
#define MAX_MACRO_NAME  64

typedef struct {
    wchar_t key[MAX_MACRO_NAME];       /* Trigger text */
    wchar_t value[MAX_MACRO_LEN];      /* Expansion text */
    int     key_len;
} MacroEntry;

static MacroEntry *g_macros = NULL;
static int g_macro_count = 0;
static int g_macro_capacity = 0;

/* ========================================================================
 * Initialize Macro System
 * ======================================================================== */

void macro_init(void)
{
    g_macro_capacity = MAX_MACROS;
    g_macros = (MacroEntry *)calloc(g_macro_capacity, sizeof(MacroEntry));
    g_macro_count = 0;
}

/* ========================================================================
 * Load Macros from File
 *
 * Format: each line is the expansion text.
 * The trigger is derived from the input sequence.
 * ======================================================================== */

void macro_load(const wchar_t *path)
{
    if (!g_macros) macro_init();

    FILE *fp = _wfopen(path, L"r, ccs=UTF-8");
    if (!fp) return;

    wchar_t line[MAX_MACRO_LEN];
    g_macro_count = 0;

    while (g_macro_count < g_macro_capacity &&
           fgetws(line, MAX_MACRO_LEN, fp)) {
        /* Remove trailing newline */
        int len = (int)wcslen(line);
        while (len > 0 && (line[len-1] == L'\n' || line[len-1] == L'\r'))
            line[--len] = 0;

        /* Skip empty lines and comments */
        if (len == 0 || line[0] == L'#')
            continue;

        /* Split on first space: trigger expansion */
        wchar_t *space = wcschr(line, L' ');
        if (space) {
            *space = 0;
            wchar_t *trigger = line;
            wchar_t *expansion = space + 1;

            /* Trim spaces from trigger */
            int tlen = (int)wcslen(trigger);
            while (tlen > 0 && trigger[tlen-1] == L' ')
                trigger[--tlen] = 0;

            if (tlen > 0 && expansion[0]) {
                wcscpy_s(g_macros[g_macro_count].key, MAX_MACRO_NAME, trigger);
                wcscpy_s(g_macros[g_macro_count].value, MAX_MACRO_LEN, expansion);
                g_macros[g_macro_count].key_len = tlen;
                g_macro_count++;
            }
        }
    }

    fclose(fp);
}

/* ========================================================================
 * Free Macro Resources
 * ======================================================================== */

void macro_free(void)
{
    if (g_macros) {
        free(g_macros);
        g_macros = NULL;
    }
    g_macro_count = 0;
    g_macro_capacity = 0;
}

/* ========================================================================
 * Lookup Macro by Key
 *
 * Returns the expansion string if the key matches a macro,
 * or NULL if no macro is found.
 * ======================================================================== */

const wchar_t *macro_lookup(const wchar_t *key, int key_len)
{
    if (!g_macros || key_len <= 0) return NULL;

    for (int i = 0; i < g_macro_count; i++) {
        if (g_macros[i].key_len == key_len &&
            _wcsnicmp(g_macros[i].key, key, key_len) == 0) {
            return g_macros[i].value;
        }
    }
    return NULL;
}
