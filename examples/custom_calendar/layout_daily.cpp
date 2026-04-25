#include "layout_daily.h"
#include "render_utils.h"
#include "config.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

static void drawDailyContent(uint8_t *fb, const CalendarData &data, time_t now,
                              int32_t x, int32_t w, bool compact) {
    struct tm lt;
    localtime_r(&now, &lt);

    // ── Header ────────────────────────────────────────────────────────────────
    if (!compact) {
        char dateBuf[64];
        rutil_fmtDateHeaderKo(now, dateBuf, sizeof(dateBuf));
        rutil_text(&NotoSansKR_16, dateBuf, x + 12, 24, fb);

        char clockBuf[16];
        snprintf(clockBuf, sizeof(clockBuf), "%02d:%02d", lt.tm_hour, lt.tm_min);
        rutil_textRight(&NotoSansKR_24, clockBuf, x + w - 12, 26, fb);

        rutil_hline(x, HEADER_H, w, 0x00, fb);
    } else {
        char hdr[64];
        snprintf(hdr, sizeof(hdr), "%d월 %d일  %02d:%02d",
                 lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min);
        rutil_text(&NotoSansKR_16, hdr, x + 10, 24, fb);
        rutil_hline(x, HEADER_H, w, 0x00, fb);
    }

    // ── Event list ────────────────────────────────────────────────────────────
    int listStartY = HEADER_H + 4;
    int timeColW   = compact ? 60 : EVENT_TIME_W;
    const GFXfont *bodyFont = &NotoSansKR_16;
    int rowH = compact ? 24 : EVENT_ROW_H;

    int todayIdx[MAX_EVENTS];
    int todayCnt = getTodayEventIndices(data, now, todayIdx,
                                        sizeof(todayIdx) / sizeof(todayIdx[0]));

    int maxRows  = (SCREEN_H - listStartY) / rowH;
    int drawn = 0;
    for (int i = 0; i < todayCnt && drawn < maxRows; i++, drawn++) {
        const CalendarEvent &ev = data.events[todayIdx[i]];
        int rowY   = listStartY + drawn * rowH;
        int baseY  = rowY + rowH - 8;

        bool isNow = (now >= ev.startTime && now < ev.endTime);
        bool isSoon = (!isNow && now >= ev.startTime - MEETING_WARN_SECONDS && now < ev.startTime);

        if (isNow || isSoon) {
            rutil_fillRect(x, rowY, w, rowH, 0xEE, fb);
        }

        if (isNow)       rutil_text(bodyFont, ">", x + 2, baseY, fb);
        else if (isSoon) rutil_text(bodyFont, "!", x + 2, baseY, fb);

        char tstr[16];
        rutil_fmtTime(ev.startTime, tstr, sizeof(tstr));
        rutil_text(bodyFont, tstr, x + 14, baseY, fb);

        int titleX = x + timeColW;
        int titleW = w - timeColW - 12;
        char title[128];
        rutil_ellipsize(bodyFont, ev.summary, titleW, title, sizeof(title));
        rutil_text(bodyFont, title, titleX, baseY, fb);

        rutil_hline(x + 8, rowY + rowH - 1, w - 16, 0xDD, fb);
    }

    if (todayCnt == 0) {
        rutil_textCentered(&NotoSansKR_16, "오늘 일정 없음",
                           x, w, listStartY + 40, fb);
    }
}

void layout_daily_draw(uint8_t *fb, const CalendarData &data, time_t now) {
    drawDailyContent(fb, data, now, 0, SCREEN_W, false);
}

void layout_daily_drawCompact(uint8_t *fb, const CalendarData &data, time_t now,
                               int32_t x, int32_t w) {
    drawDailyContent(fb, data, now, x, w, true);
}
