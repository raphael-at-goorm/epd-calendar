#include "layout_weekly.h"
#include "render_utils.h"
#include "config.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

static const char *KO_DOW[] = {"일","월","화","수","목","금","토"};

// NotoSansKR_8 metrics
#define FONT8_ASCENT    7
#define FONT8_DESCENT   2
#define FONT8_LINE_H   13

// Virtual timeline: 09:00 – 23:30 at TGRID_W_PX_HOUR px/hr
// VEND = (23 - 9) * 48 + 24 = 672 + 24 = 696  virtual pixels
#define VPXHR   TGRID_W_PX_HOUR
#define VEND    ((TGRID_W_END_H - TGRID_START_H) * VPXHR + VPXHR / 2)

static void drawTextWhite8(int32_t x, int32_t y, const char *str, uint8_t *fb) {
    FontProperties p = {.fg_color=15, .bg_color=3, .fallback_glyph=0, .flags=0};
    int32_t cx = x, cy = y;
    write_mode((GFXfont*)&NotoSansKR_8, str, &cx, &cy, fb, WHITE_ON_BLACK, &p);
}

// Hours since TGRID_START_H → virtual pixels
static int timeToVY(time_t t) {
    struct tm lt;
    localtime_r(&t, &lt);
    float h = (float)(lt.tm_hour - TGRID_START_H) + lt.tm_min / 60.0f
                                                    + lt.tm_sec / 3600.0f;
    return (int)(h * VPXHR);
}

// Place current time at ~1/3 from top of visible area
static int getScrollOffset(time_t now, int evAreaH) {
    int curVY  = timeToVY(now);
    int offset = curVY - evAreaH / 3;
    int maxOff = VEND - evAreaH;
    if (offset < 0) offset = 0;
    if (maxOff > 0 && offset > maxOff) offset = maxOff;
    return offset;
}

static int vToScreen(int vY, int evAreaY, int scrollOff) {
    return evAreaY + vY - scrollOff;
}

// Returns screen Y of the current-time marker, or -1 if outside visible range.
int layout_weekly_timeMarkerY(time_t now) {
    struct tm lt;
    localtime_r(&now, &lt);
    if (lt.tm_hour < TGRID_START_H) return -1;
    float hf = (float)(lt.tm_hour - TGRID_START_H) + lt.tm_min / 60.0f;
    if ((int)(hf * VPXHR) > VEND) return -1;

    int evAreaY  = WEEK_STRIP_Y + WEEK_STRIP_H + 1;
    int evAreaH  = SCREEN_H - evAreaY;
    int scrollOff = getScrollOffset(now, evAreaH);
    int screenY  = vToScreen((int)(hf * VPXHR), evAreaY, scrollOff);
    if (screenY < evAreaY || screenY > evAreaY + evAreaH) return -1;
    return screenY;
}

// Wrap NotoSansKR_8 text within a block (title font — never grayed)
static void drawWrap8(const char *str, int x, int maxW,
                       int startBase, int maxBase, uint8_t *fb) {
    const char *p = str;
    int32_t curY = startBase;
    char lineBuf[64];

    while (*p && curY <= maxBase) {
        size_t lineBytes = 0;
        const char *q = p;
        bool forceOne  = true;

        while (*q) {
            uint8_t c = (uint8_t)*q;
            int cb = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
            if (lineBytes + cb >= sizeof(lineBuf) - 1) break;

            char probe[64];
            memcpy(probe, p, lineBytes + cb);
            probe[lineBytes + cb] = '\0';
            if (!forceOne && rutil_textWidth(&NotoSansKR_8, probe) > maxW) break;

            lineBytes += cb;
            q += cb;
            forceOne = false;
        }
        if (lineBytes == 0) break;

        memcpy(lineBuf, p, lineBytes);
        lineBuf[lineBytes] = '\0';
        drawTextWhite8(x, curY, lineBuf, fb);
        p += lineBytes;
        curY += FONT8_LINE_H;
    }
}

static void drawDayColumn(uint8_t *fb, const CalendarData &data,
                           int colX, int colW, int evAreaY, int evAreaH,
                           int year, int mon, int mday, bool isToday,
                           time_t now, int scrollOff) {
    const int PAD = 2;
    const int BX  = colX + PAD + 1;
    const int BW  = colW - PAD * 2 - 2;

    for (int i = 0; i < data.eventCount; i++) {
        const CalendarEvent &ev = data.events[i];
        if (ev.isAllDay) continue;

        struct tm lt;
        localtime_r(&ev.startTime, &lt);
        if (lt.tm_year + 1900 != year ||
            lt.tm_mon  + 1    != mon  ||
            lt.tm_mday        != mday) continue;

        int topVY = timeToVY(ev.startTime);
        int botVY = timeToVY(ev.endTime);
        if (topVY < 0)    topVY = 0;
        if (botVY > VEND) botVY = VEND;

        int topY = vToScreen(topVY, evAreaY, scrollOff);
        int botY = vToScreen(botVY, evAreaY, scrollOff);

        // Skip completely outside visible area
        if (topY >= evAreaY + evAreaH) continue;
        if (botY <= evAreaY) continue;

        // Clip to visible area
        if (topY < evAreaY) topY = evAreaY;
        if (botY > evAreaY + evAreaH) botY = evAreaY + evAreaH;

        int blkH = botY - topY;
        if (blkH < 5) blkH = 5;

        uint8_t bg = 0x33;  // dark gray block, white text
        rutil_fillRect(BX, topY + 1, BW, blkH - 1, bg, fb);

        if (blkH < FONT8_ASCENT + 14) continue;

        int firstBase = topY + 13 + FONT8_ASCENT;
        int lastBase  = topY + blkH - FONT8_DESCENT;

        // Start time in white on black block
        char tstr[8];
        snprintf(tstr, sizeof(tstr), "%02d:%02d", lt.tm_hour, lt.tm_min);
        drawTextWhite8(BX + 2, firstBase, tstr, fb);

        int titleBase = firstBase + FONT8_LINE_H;
        if (titleBase <= lastBase)
            drawWrap8(ev.summary, BX + 2, BW - 4, titleBase, lastBase, fb);
    }
}

void layout_weekly_draw(uint8_t *fb, const CalendarData &data, time_t now) {
    struct tm lt;
    localtime_r(&now, &lt);

    // ── Header ────────────────────────────────────────────────────────────────
    char hdrDate[64];
    rutil_fmtDateHeaderKo(now, hdrDate, sizeof(hdrDate));
    rutil_text(&NotoSansKR_12, hdrDate, 10, 26, fb);

    char hdrTime[12];
    snprintf(hdrTime, sizeof(hdrTime), "%02d:%02d", lt.tm_hour, lt.tm_min);
    rutil_textRight(&NotoSansKR_12, hdrTime, SCREEN_W - 10, 26, fb);

    rutil_hline(0, HEADER_H, SCREEN_W, 0x00, fb);

    // ── Find Sunday of current week ───────────────────────────────────────────
    struct tm weekSun = lt;
    weekSun.tm_mday -= lt.tm_wday;
    weekSun.tm_hour = 0; weekSun.tm_min = 0; weekSun.tm_sec = 0;
    mktime(&weekSun);

    // ── Week strip ────────────────────────────────────────────────────────────
    int y0     = WEEK_STRIP_Y;
    int y_dow  = y0 + 26;
    int y_date = y0 + 54;
    int stripH = WEEK_STRIP_H;

    for (int col = 0; col < 7; col++) {
        struct tm day = weekSun;
        day.tm_mday += col;
        mktime(&day);

        int cx = col * WEEK_COL_W;
        bool isToday = (day.tm_year == lt.tm_year &&
                        day.tm_mon  == lt.tm_mon  &&
                        day.tm_mday == lt.tm_mday);

        if (isToday) rutil_fillRect(cx, y0, WEEK_COL_W, stripH, 0xCC, fb);
        if (col > 0) rutil_vline(cx, y0, stripH, 0x44, fb);

        rutil_textCentered(&NotoSansKR_12, KO_DOW[day.tm_wday],
                           cx, WEEK_COL_W, y_dow, fb);

        char dateStr[8];
        snprintf(dateStr, sizeof(dateStr), "%d", day.tm_mday);

        if (isToday) {
            int bx = cx + WEEK_COL_W / 2;
            epd_fill_circle(bx, y_date - 7, 10, 0x00, fb);
            FontProperties wp = {.fg_color=15, .bg_color=0, .fallback_glyph=0, .flags=0};
            int32_t tx = bx - rutil_textWidth(&NotoSansKR_12, dateStr) / 2;
            int32_t ty = y_date;
            write_mode((GFXfont*)&NotoSansKR_12, dateStr, &tx, &ty, fb, WHITE_ON_BLACK, &wp);
        } else {
            rutil_textCentered(&NotoSansKR_12, dateStr, cx, WEEK_COL_W, y_date, fb);
        }
    }

    rutil_hline(0, y0 + stripH, SCREEN_W, 0x00, fb);

    // ── Event area ────────────────────────────────────────────────────────────
    int evAreaY  = y0 + stripH + 1;
    int evAreaH  = SCREEN_H - evAreaY;
    int scrollOff = getScrollOffset(now, evAreaH);

    // Vertical day separators
    for (int col = 1; col < 7; col++)
        rutil_vline(col * WEEK_COL_W, evAreaY, evAreaH, 0xCC, fb);

    // Time grid: every 30 min from 09:00 to 23:30
    // slot 0=09:00, slot 1=09:30, ..., slot 29=23:30
    int totalSlots = (TGRID_W_END_H - TGRID_START_H) * 2 + 1;
    for (int slot = 0; slot <= totalSlots; slot++) {
        int vY = slot * VPXHR / 2;
        int y  = vToScreen(vY, evAreaY, scrollOff);
        if (y < evAreaY || y > evAreaY + evAreaH) continue;
        bool isFullHour = (slot % 2 == 0);
        int  hour = TGRID_START_H + slot / 2;
        bool isMajor = isFullHour && (hour % 2 == 0);
        rutil_hline(0, y, SCREEN_W,
                    isMajor ? 0xCC : (isFullHour ? 0xDD : 0xEE), fb);
    }

    // Event blocks per column
    for (int col = 0; col < 7; col++) {
        struct tm day = weekSun;
        day.tm_mday += col;
        mktime(&day);
        bool isToday = (day.tm_year == lt.tm_year &&
                        day.tm_mon  == lt.tm_mon  &&
                        day.tm_mday == lt.tm_mday);
        drawDayColumn(fb, data, col * WEEK_COL_W, WEEK_COL_W, evAreaY, evAreaH,
                      day.tm_year + 1900, day.tm_mon + 1, day.tm_mday,
                      isToday, now, scrollOff);
    }

    // Current time marker (drawn last — overlays event blocks)
    int curVY = timeToVY(now);
    if (curVY >= 0 && curVY <= VEND) {
        int curY = vToScreen(curVY, evAreaY, scrollOff);
        if (curY >= evAreaY && curY <= evAreaY + evAreaH) {
            rutil_hline(0, curY, SCREEN_W, 0x00, fb);
            epd_fill_circle(4, curY, 4, 0x00, fb);
        }
    }
}
