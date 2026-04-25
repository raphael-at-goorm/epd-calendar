#include "render_utils.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *KO_WEEKDAY[] = {"일","월","화","수","목","금","토"};
static const char *KO_WEEKDAY_LONG[] = {"일요일","월요일","화요일","수요일","목요일","금요일","토요일"};

// ── Text ──────────────────────────────────────────────────────────────────────

int32_t rutil_text(const GFXfont *font, const char *str,
                   int32_t x, int32_t y, uint8_t *fb) {
    int32_t cx = x, cy = y;
    writeln((GFXfont *)font, str, &cx, &cy, fb);
    return cx;
}

int32_t rutil_textWidth(const GFXfont *font, const char *str) {
    int32_t x = 0, y = 0, x1, y1, w, h;
    get_text_bounds(font, str, &x, &y, &x1, &y1, &w, &h, nullptr);
    return w;
}

void rutil_textCentered(const GFXfont *font, const char *str,
                        int32_t x, int32_t w, int32_t baseline, uint8_t *fb) {
    int32_t tw = rutil_textWidth(font, str);
    int32_t cx = x + (w - tw) / 2;
    rutil_text(font, str, cx, baseline, fb);
}

void rutil_textRight(const GFXfont *font, const char *str,
                     int32_t x_right, int32_t baseline, uint8_t *fb) {
    int32_t tw = rutil_textWidth(font, str);
    rutil_text(font, str, x_right - tw, baseline, fb);
}

// ── Shapes ────────────────────────────────────────────────────────────────────

void rutil_fillRect(int32_t x, int32_t y, int32_t w, int32_t h,
                    uint8_t color, uint8_t *fb) {
    epd_fill_rect(x, y, w, h, color, fb);
}

void rutil_hline(int32_t x, int32_t y, int32_t len, uint8_t color, uint8_t *fb) {
    epd_draw_hline(x, y, len, color, fb);
}

void rutil_vline(int32_t x, int32_t y, int32_t len, uint8_t color, uint8_t *fb) {
    epd_draw_vline(x, y, len, color, fb);
}

// ── Date / time formatting ────────────────────────────────────────────────────

void rutil_fmtTime(time_t t, char *buf, size_t len) {
    struct tm lt;
    localtime_r(&t, &lt);
    snprintf(buf, len, "%02d:%02d", lt.tm_hour, lt.tm_min);
}

void rutil_fmtTimeRange(time_t start, time_t end, char *buf, size_t len) {
    char s[8], e[8];
    rutil_fmtTime(start, s, sizeof(s));
    rutil_fmtTime(end,   e, sizeof(e));
    snprintf(buf, len, "%s - %s", s, e);
}

void rutil_fmtDateKo(time_t t, char *buf, size_t len) {
    struct tm lt;
    localtime_r(&t, &lt);
    snprintf(buf, len, "%d월 %d일 (%s)",
             lt.tm_mon + 1, lt.tm_mday, KO_WEEKDAY[lt.tm_wday]);
}

void rutil_fmtDateHeaderKo(time_t t, char *buf, size_t len) {
    struct tm lt;
    localtime_r(&t, &lt);
    snprintf(buf, len, "%d년 %d월 %d일 %s",
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
             KO_WEEKDAY_LONG[lt.tm_wday]);
}

// ── Text wrap ─────────────────────────────────────────────────────────────────

int32_t rutil_textWrap(const GFXfont *font, const char *str,
                       int32_t x, int32_t maxW, int32_t baseY,
                       int32_t lineH, int32_t stopY, uint8_t *fb) {
    const char *p = str;
    int32_t curY = baseY;
    char lineBuf[256];

    while (*p && curY <= stopY) {
        size_t lineBytes = 0;
        const char *q = p;
        bool forceOne = true;  // always include at least one character

        while (*q) {
            uint8_t c = (uint8_t)*q;
            int cb = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
            if (lineBytes + cb >= sizeof(lineBuf) - 1) break;

            char probe[256];
            memcpy(probe, p, lineBytes + cb);
            probe[lineBytes + cb] = '\0';
            if (!forceOne && rutil_textWidth(font, probe) > maxW) break;

            lineBytes += cb;
            q += cb;
            forceOne = false;
        }

        if (lineBytes == 0) break;

        memcpy(lineBuf, p, lineBytes);
        lineBuf[lineBytes] = '\0';

        if (fb) rutil_text(font, lineBuf, x, curY, fb);

        p += lineBytes;
        curY += lineH;
    }

    return curY;
}

// ── Ellipsize ─────────────────────────────────────────────────────────────────

void rutil_ellipsize(const GFXfont *font, const char *str,
                     int32_t max_px, char *out, size_t outLen) {
    if (rutil_textWidth(font, str) <= max_px) {
        strncpy(out, str, outLen - 1);
        out[outLen - 1] = '\0';
        return;
    }
    // Binary-search safe UTF-8 truncation
    const char *ellipsis = "...";
    int32_t ellW = rutil_textWidth(font, ellipsis);
    int32_t budget = max_px - ellW;

    size_t srcLen = strlen(str);
    // Walk byte-by-byte, accumulate character widths
    char tmp[256] = {};
    size_t pos = 0;
    while (pos < srcLen && pos < outLen - 4) {
        // Determine UTF-8 char length
        uint8_t c = (uint8_t)str[pos];
        int charBytes = 1;
        if      ((c & 0xF0) == 0xF0) charBytes = 4;
        else if ((c & 0xE0) == 0xE0) charBytes = 3;
        else if ((c & 0xC0) == 0xC0) charBytes = 2;

        // Tentatively append
        char probe[256];
        size_t copyLen = pos + charBytes < outLen - 4 ? pos + charBytes : pos;
        memcpy(probe, str, copyLen);
        probe[copyLen] = '\0';
        if (rutil_textWidth(font, probe) > budget) break;

        memcpy(tmp, probe, copyLen);
        tmp[copyLen] = '\0';
        pos += charBytes;
    }
    snprintf(out, outLen, "%s%s", tmp, ellipsis);
}
