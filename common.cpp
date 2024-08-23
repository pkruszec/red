#include "common.h"

static size_t utf32_to_8(uint32_t cp, uint8_t *buf, size_t sz)
{
    if (cp <= 0x7F) {
        if (sz < 1) return 0;
        buf[0] = cp;
        return 1;
    }

    if (cp <= 0x7FF) {
        if (sz < 2) return 0;
        buf[0] = 0xC0 | (cp >> 6);
        buf[1] = 0x80 | (cp & 0x3F);
        return 2;
    }

    if (cp <= 0xFFFF) {
        if (sz < 3) return 0;
        buf[0] = 0xE0 | (cp >> 12);
        buf[1] = 0x80 | ((cp >> 6) & 0x3F);
        buf[2] = 0x80 | (cp & 0x3F);
        return 3;
    }

    if (cp <= 0x10FFFF) {
        if (sz < 4) return 0;
        buf[0] = 0xF0 | (cp >> 18);
        buf[1] = 0x80 | ((cp >> 12) & 0x3F);
        buf[2] = 0x80 | ((cp >> 6) & 0x3F);
        buf[3] = 0x80 | (cp & 0x3F);
        return 4;
    }
    
    return 0;
}

static uint32_t utf8_to_32(uint8_t *buf, size_t sz, size_t *used)
{
    // This is allocated for the circumstance when 'used' == 0.
    // It is practically free, and there's no need to write null checks all over the place.
    size_t null;
    
    if (sz < 1) return 0;
    if (!used) used = &null;
    
    if ((buf[0] & 0x80) == 0x00) {
        *used = 1;
        return buf[0] & 0x7F;
    }
    
    if ((buf[0] & 0xE0) == 0xC0) {
        if (sz < 2) return 0;
        *used = 2;
        return
            (buf[1] & 0x3F) |
            ((buf[0] & 0x1F) << 6);
    }
    
    if ((buf[0] & 0xF0) == 0xE0) {
        if (sz < 3) return 0;
        *used = 3;
        return
            (buf[2] & 0x3F) |
            ((buf[1] & 0x3F) << 6) |
            ((buf[0] & 0x0F) << 12);
    }
    
    if ((buf[0] & 0xF8) == 0xF0) {
        if (sz < 4) return 0;
        *used = 4;
        return
            (buf[3] & 0x3F) |
            ((buf[2] & 0x3F) << 6) |
            ((buf[1] & 0x3F) << 12) |
            ((buf[0] & 0x07) << 18);
    }

    return 0;
}
