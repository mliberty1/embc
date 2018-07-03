/* Copyright 2015 Jetperch LLC.  All rights reserved. */

#include "embc/cstr.h"
#include "embc/config.h"
#include <ctype.h>

int embc_cstr_copy(char * tgt, char const * src, embc_size_t tgt_size) {
    if ((NULL == tgt) || (tgt_size <= 0)) {
        return -1; // nonsensical input.
    }
    if (NULL == src) {
        *tgt = 0;
        return 0;
    }
    char * tgt_end = tgt + tgt_size - 1;
    while (1) {
        if (!*src) {
            break;
        }
        if (tgt >= tgt_end) {
            *tgt = 0;
            return 1;
        }
        *tgt++ = *src++;
    }
    *tgt = 0; // ensure NULL terminated.
    return 0;
}

int embc_cstr_casecmp(const char * s1, const char * s2) {
    char c1;
    char c2;
    if (!s1) {
        return -1;
    }
    if (!s2) {
        return 1;
    }
    while (1) {
        if (*s1 == 0) {
            if (*s2 == 0) {
                return 0;
            }
            return -1;
        } else if (*s2 == 0) {
            return 1;
        }
        c1 = toupper(*s1);
        c2 = toupper(*s2);
        if (c1 < c2) {
            return -1;
        } else if (c1 > c2) {
            return 1;
        }
        ++s1;
        ++s2;
    }
}

const char * embc_cstr_starts_with(const char * s, const char * prefix) {
    if (!prefix || !s) {
        return s;
    }
    while (1) {
        if (*prefix == 0) {
            return s;
        }
        if ((*s == 0) || (*prefix != *s)) {
            return 0;
        }
        ++prefix;
        ++s;
    }
}

int embc_cstr_to_u32(const char * src, uint32_t * value) {
    uint32_t v = 0;

    if ((NULL == src) || (NULL == value)) {
        return 1;
    }

    while (*src && isspace((uint8_t) *src)) {
        ++src;
    }
    if (!*src) { // empty string.
        return 1;
    }
    if ((src[0] == '0') && (src[1] == 'x')) {
        // hex string
        uint32_t nibble;
        src += 2;
        while (*src) {
            char c = *src;
            if ((c >= '0') && (c <= '9')) {
                nibble = c - '0';
            } else if ((c >= 'a') && (c <= 'f')) {
                nibble = c - 'a' + 10;
            } else if ((c >= 'A') && (c <= 'F')) {
                nibble = c - 'A' + 10;
            } else {
                break;
            }
            v = v * 16 + nibble;
            ++src;
        }
    } else {
        while ((*src >= '0') && (*src <= '9')) {
            v = v * 10 + (*src - '0');
            ++src;
        }
    }
    while (*src) {
        if (!isspace((uint8_t) *src++)) { // did not parse full string
            return 1;
        }
    }
    *value = v;
    return 0;
}

int embc_cstr_to_i32(const char * src, int32_t * value) {
    int neg = 0;
    uint32_t v;

    if ((NULL == src) || (NULL == value)) {
        return 1;
    }

    while (*src && isspace((uint8_t) *src)) {
        ++src;
    }

    if (*src == '-') {
        neg = 1;
        ++src;
    } else if (*src == '+') {
        ++src;
    }

    int rc = embc_cstr_to_u32(src, &v);
    if (rc) {
        return rc;
    }
    *value = (int32_t) v;
    if (neg) {
        *value = -*value;
    }
    return 0;
}

int embc_cstr_to_i32s(const char * src, int32_t exponent, int32_t * value) {
    int32_t v = 0;
    int32_t decimal = -1;  // still on integer part
    int neg = 0;

    if (!src) {
        return 0;
    }
    if (exponent < 0) {
        return -1;
    }

    while (*src && isspace((uint8_t) *src)) {
        ++src;
    }

    if (*src == '-') {
        neg = 1;
        ++src;
    } else if (*src == '+') {
        ++src;
    }

    while (decimal < exponent) {
        if (*src == '.') {
            decimal = 0;
            ++src;
            continue;
        }
        if ((*src == 0) || (*src == ',')) {
            if (decimal < 0) {
                break;
            } else {
                v *= 10;
                ++decimal;
            }
        } else {
            if (decimal >= 0) {
                ++decimal;
            }
            v *= 10;
            if ((*src >= '0') && (*src <= '9')) {
                v += *src - '0';
            } else {
                return 1;
            }
            ++src;
        }
    }

    while (*src) {
        if ((*src >= '0') && (*src <= '9')) {
            // discard extra digits
        } else if (!isspace((uint8_t) *src)) { // did not parse full string
            return 1;
        }
        ++src;
    }

    if (decimal < 0) {
        decimal = 0;
    }
    while (decimal < exponent) {
        v *= 10;
        ++decimal;
    }
    if (neg) {
        v = -v;
    }
    if (value) {
        *value = v;
    }
    return 0;
}

#if EMBC_CSTR_FLOAT_ENABLE
int embc_cstr_to_f32(const char * src, float * value) {
    char *p;
    float x0;

    if ((NULL == src) || (NULL == value)) {
        return 1;
    }

    while (*src && isspace((uint8_t) *src)) {
        ++src;
    }
    if (!*src) { // empty string.
        return 1;
    }

    x0 = strtof(src, &p);
    if ((p == NULL)) {
        return 1;
    }
    while (*p) {
        if (!isspace((uint8_t) *p++)) { // did not parse full string
            return 1;
        }
    }

    *value = (float)x0;
    return 0;
}
#endif

int embc_cstr_toupper(char * s) {
    if (NULL == s) {
        return 1;
    }
    while (*s) {
        *s = toupper((uint8_t) *s);
        ++s;
    }
    return 0;
}

int embc_cstr_to_index(char const * s, char const * const * table, int * index) {
    if ((NULL == s) || (NULL == table) || (NULL == index)) {
        return 2;
    }
    int idx = 0;
    while (*table) {
        if (strcmp(s, *table) == 0) {
            *index = idx;
            return 0;
        }
        ++idx;
        ++table;
    }
    return 1;
}

static const char * const true_table_[] = {"ON", "1", "ENABLE", "TRUE", NULL};
static const char * const false_table[] = {"OFF", "0", "DISABLE", "FALSE", NULL};

int embc_cstr_to_bool(char const * s, bool * value) {
    char buffer[16]; // longer than any entry
    int index;
    if ((s == NULL) || (NULL == value)) {
        return 1;
    }
    embc_cstr_array_copy(buffer, s);
    embc_cstr_toupper(buffer);
    if (0 == embc_cstr_to_index(buffer, true_table_, &index)) {
        *value = true;
        return 0;
    }
    if (0 == embc_cstr_to_index(buffer, false_table, &index)) {
        *value = false;
        return 0;
    }
    return 1;
}

uint8_t embc_cstr_hex_to_u4(char v) {
    if ((v >= '0') && (v <= '9')) {
        return v - '0';
    } else if ((v >= 'a') && (v <= 'f')) {
        return v - 'a' + 10;
    } else if ((v >= 'A') && (v <= 'F')) {
        return v - 'A' + 10;
    } else {
        return 0;
    }
}

char embc_cstr_u4_to_hex(uint8_t v) {
    if (v < 10) {
        return ('0' + v);
    } else if (v < 16) {
        return ('A' + (v - 10));
    } else {
        return '0';
    }
}
