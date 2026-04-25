#include "layout_meeting.h"
#include "render_utils.h"
#include "config.h"
#include "build_version.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Timeline constants (left panel)
#define TLINE_TOP      10
#define TLINE_LABEL_W  28
#define TLINE_EV_X     (TLINE_LABEL_W + 2)
#define TLINE_EV_W     (MEETING_SPLIT_X - TLINE_EV_X - 4)
#define TLINE_VPXHR    TGRID_W_PX_HOUR
#define TLINE_VEND     ((TGRID_W_END_H - TGRID_START_H) * TLINE_VPXHR + TLINE_VPXHR / 2)

// Safe ascent for NotoSansKR_12 (actual ascent may be up to 11–12px)
#define FONT12_ASCENT   12

static void drawTextGray(const GFXfont *font, const char *str,
                          int32_t x, int32_t y, uint8_t *fb) {
    FontProperties p = {.fg_color=9, .bg_color=15, .fallback_glyph=0, .flags=0};
    int32_t cx = x, cy = y;
    write_mode((GFXfont*)font, str, &cx, &cy, fb, BLACK_ON_WHITE, &p);
}

static void drawTextWhite(const GFXfont *font, const char *str,
                           int32_t x, int32_t y, uint8_t *fb) {
    FontProperties p = {.fg_color=15, .bg_color=0, .fallback_glyph=0, .flags=0};
    int32_t cx = x, cy = y;
    write_mode((GFXfont*)font, str, &cx, &cy, fb, WHITE_ON_BLACK, &p);
}

static void drawTextBold(const GFXfont *font, const char *str,
                          int32_t x, int32_t y, uint8_t *fb) {
    rutil_text(font, str, x,     y, fb);
    rutil_text(font, str, x + 1, y, fb);
}

static int tline_timeToVY(time_t t) {
    struct tm lt; localtime_r(&t, &lt);
    float h = (float)(lt.tm_hour - TGRID_START_H) + lt.tm_min / 60.0f + lt.tm_sec / 3600.0f;
    return (int)(h * TLINE_VPXHR);
}

static int tline_scrollOff(time_t now, int areaH) {
    int curVY  = tline_timeToVY(now);
    int offset = curVY - areaH / 3;
    int maxOff = TLINE_VEND - areaH;
    if (offset < 0) offset = 0;
    if (maxOff > 0 && offset > maxOff) offset = maxOff;
    return offset;
}

static int tline_vToScreen(int vY, int scrollOff) {
    return TLINE_TOP + vY - scrollOff;
}


// ── Left panel: daily timeline (09:00–23:30, scrolls to current time) ─────────
static void drawTimeline(uint8_t *fb, const CalendarData &data, time_t now,
                          const CalendarEvent &selEv) {
    struct tm lt;
    localtime_r(&now, &lt);

    const int areaH = SCREEN_H - TLINE_TOP;
    int scrollOff   = tline_scrollOff(now, areaH);

    // Grid lines every 30 min from 09:00 to 23:30
    int totalSlots = (TGRID_W_END_H - TGRID_START_H) * 2 + 1;
    for (int slot = 0; slot <= totalSlots; slot++) {
        int vY = slot * TLINE_VPXHR / 2;
        int y  = tline_vToScreen(vY, scrollOff);
        if (y < TLINE_TOP || y > SCREEN_H) continue;
        bool isFullHour = (slot % 2 == 0);
        int  hour       = TGRID_START_H + slot / 2;
        bool isMajor    = isFullHour && (hour % 2 == 0);
        rutil_hline(TLINE_LABEL_W, y, MEETING_SPLIT_X - TLINE_LABEL_W,
                    isMajor ? 0xAA : (isFullHour ? 0xCC : 0xDD), fb);
        if (isFullHour) {
            char lbl[8];
            snprintf(lbl, sizeof(lbl), "%d", hour);
            drawTextGray(&NotoSansKR_12, lbl, 2, y + 13, fb);
        }
    }

    // Today's events
    for (int i = 0; i < data.eventCount; i++) {
        const CalendarEvent &ev = data.events[i];
        if (ev.isAllDay) continue;

        struct tm evlt;
        localtime_r(&ev.startTime, &evlt);
        if (evlt.tm_year != lt.tm_year ||
            evlt.tm_mon  != lt.tm_mon  ||
            evlt.tm_mday != lt.tm_mday) continue;

        int topVY = tline_timeToVY(ev.startTime);
        int botVY = tline_timeToVY(ev.endTime);
        if (topVY < 0)         topVY = 0;
        if (botVY > TLINE_VEND) botVY = TLINE_VEND;

        int topY = tline_vToScreen(topVY, scrollOff);
        int botY = tline_vToScreen(botVY, scrollOff);

        if (topY >= SCREEN_H || botY <= TLINE_TOP) continue;
        if (topY < TLINE_TOP) topY = TLINE_TOP;
        if (botY > SCREEN_H)  botY = SCREEN_H;

        int evH = botY - topY;
        if (evH < 8) evH = 8;

        bool isSelected = (ev.startTime == selEv.startTime &&
                           ev.endTime   == selEv.endTime);
        bool isPast     = (now > ev.endTime);

        if (isSelected) {
            rutil_fillRect(TLINE_EV_X, topY, TLINE_EV_W, evH, 0x22, fb);
            if (evH >= FONT12_ASCENT + 19) {
                char title[128];
                rutil_ellipsize(&NotoSansKR_12, ev.summary, TLINE_EV_W - 6, title, sizeof(title));
                drawTextWhite(&NotoSansKR_12, title, TLINE_EV_X + 4, topY + FONT12_ASCENT + 17, fb);
            }
        } else if (isPast) {
            rutil_fillRect(TLINE_EV_X, topY, TLINE_EV_W, evH, 0xEE, fb);
        } else {
            rutil_fillRect(TLINE_EV_X, topY, TLINE_EV_W, evH, 0xCC, fb);
            if (evH >= FONT12_ASCENT + 19) {
                char title[128];
                rutil_ellipsize(&NotoSansKR_12, ev.summary, TLINE_EV_W - 6, title, sizeof(title));
                rutil_text(&NotoSansKR_12, title, TLINE_EV_X + 4, topY + FONT12_ASCENT + 17, fb);
            }
        }
    }

    // Current time marker
    int curVY = tline_timeToVY(now);
    int curY  = tline_vToScreen(curVY, scrollOff);
    if (curY >= TLINE_TOP && curY <= SCREEN_H) {
        rutil_hline(TLINE_LABEL_W, curY, MEETING_SPLIT_X - TLINE_LABEL_W, 0x00, fb);
        epd_fill_circle(TLINE_LABEL_W, curY, 4, 0x00, fb);
    }
}

// ── Right panel: event detail ─────────────────────────────────────────────────
static void drawEventDetail(uint8_t *fb, const CalendarEvent &ev, time_t now) {
    const int rx = MEETING_SPLIT_X + 1;
    const int rw = SCREEN_W - rx;
    const int px = rx + 14;
    const int pw = rw - 28;

    rutil_vline(MEETING_SPLIT_X, 0, SCREEN_H, 0x00, fb);

    bool inProgress = (now >= ev.startTime);
    long diff       = (long)(ev.startTime - now);

    // ── Badge strip: shifted +10px, expanded +10px (y=14, h=32, bottom=46) ──
    const char *badge = inProgress ? "진행 중" : "곧 시작";
    rutil_fillRect(rx + 1, 14, rw - 2, 32, 0x00, fb);
    int32_t bx = rx + 1 + (rw - 2 - rutil_textWidth(&NotoSansKR_12, badge)) / 2;
    drawTextWhite(&NotoSansKR_12, badge, bx, 14 + 22, fb);  // baseline at mid-strip

    // ── Title: NotoSansKR_12 bold, up to 2 lines, starts below badge ─────────
    // badge bottom=46, titleY=76 → text top=76-12=64, gap=18px
    int titleY = 76;
    int32_t nextY = rutil_textWrap(&NotoSansKR_12, ev.summary, px,     pw,
                                   titleY, 16, titleY + 16, fb);
              rutil_textWrap(&NotoSansKR_12, ev.summary, px + 1, pw,
                             titleY, 16, titleY + 16, fb);

    rutil_hline(rx + 8, nextY + 4, rw - 16, 0xBB, fb);

    // ── Info rows: NotoSansKR_12, bold labels, LABEL_COL=60, ROW_H=43 ────────
    const int LABEL_COL = 60;   // wide gap between label and content
    const int ROW_H     = 43;   // 28 + 15px extra spacing
    int y = nextY + 22;

    // Time row
    char trange[32];
    rutil_fmtTimeRange(ev.startTime, ev.endTime, trange, sizeof(trange));
    char countdown[28] = {};
    if (!inProgress && diff > 0)
        snprintf(countdown, sizeof(countdown), " (%ld분 후)", diff / 60);
    else if (inProgress)
        snprintf(countdown, sizeof(countdown), " (%ld분 경과)", (long)(now - ev.startTime) / 60);
    char timeInfo[72];
    snprintf(timeInfo, sizeof(timeInfo), "%s%s", trange, countdown);
    drawTextBold(&NotoSansKR_12, "시간", px, y, fb);
    rutil_text(&NotoSansKR_12, timeInfo, px + LABEL_COL, y, fb);
    y += ROW_H;

    // Location row
    if (ev.location[0]) {
        char loc[128];
        rutil_ellipsize(&NotoSansKR_12, ev.location, pw - LABEL_COL, loc, sizeof(loc));
        drawTextBold(&NotoSansKR_12, "장소", px, y, fb);
        rutil_text(&NotoSansKR_12, loc, px + LABEL_COL, y, fb);
        y += ROW_H;
    }

    // Attendees
    if (ev.attendeeCount > 0) {
        char hdr[32];
        snprintf(hdr, sizeof(hdr), "참석자 (%d명)", ev.attendeeCount);
        drawTextBold(&NotoSansKR_12, hdr, px, y, fb);
        y += 8;
        rutil_hline(px, y, pw, 0xCC, fb);
        y += 18;

        const int attRowH = 39;   // 24 + 15px extra spacing
        int maxShow = (SCREEN_H - y - 6) / attRowH;
        int show    = ev.attendeeCount < maxShow ? ev.attendeeCount : maxShow;

        for (int i = 0; i < show; i++) {
            char name[64];
            rutil_ellipsize(&NotoSansKR_12, ev.attendees[i], pw, name, sizeof(name));
            if (strcmp(ev.attendeeStatus[i], "declined") == 0)
                drawTextGray(&NotoSansKR_12, name, px, y, fb);
            else
                rutil_text(&NotoSansKR_12, name, px, y, fb);
            y += attRowH;
        }

        if (ev.attendeeCount > maxShow) {
            char more[16];
            snprintf(more, sizeof(more), "외 %d명", ev.attendeeCount - maxShow);
            drawTextGray(&NotoSansKR_12, more, px, y, fb);
        }
    }
}

// ── Public entry point ────────────────────────────────────────────────────────
void layout_meeting_draw(uint8_t *fb, const CalendarData &data, time_t now,
                          const CalendarEvent &ev) {
    drawTimeline(fb, data, now, ev);
    drawEventDetail(fb, ev, now);

    // Build version — bottom-right corner
    rutil_textRight(&NotoSansKR_8, BUILD_VERSION, SCREEN_W - 4, SCREEN_H - 4, fb);
}
