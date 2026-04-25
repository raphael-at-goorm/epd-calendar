#pragma once

// ── Timezone ──────────────────────────────────────────────────────────────────
#define TIMEZONE_POSIX      "KST-9"
#define NTP_SERVER_1        "pool.ntp.org"
#define NTP_SERVER_2        "time.google.com"

// ── Update intervals ──────────────────────────────────────────────────────────
#define CALENDAR_FETCH_INTERVAL_MS   (1UL * 60UL * 1000UL)    // 1 minute
#define CLOCK_REFRESH_INTERVAL_MS    (60UL * 1000UL)           // 1 minute
#define MEETING_WARN_SECONDS         (10 * 60)                  // 10 min before start

// ── Google Calendar ───────────────────────────────────────────────────────────
// Fetch events this many days ahead
#define CALENDAR_DAYS_AHEAD          14
// Maximum events to store
#define MAX_EVENTS                   30
// Maximum attendees per event
#define MAX_ATTENDEES                10

// ── Weekly time grid ─────────────────────────────────────────────────────────
#define TGRID_START_H    9    // 09:00 (grid origin)
#define TGRID_END_H     21    // legacy — meeting view compat
// Weekly view scrollable timeline: 09:00 – 23:30
#define TGRID_W_END_H   23    // last full hour (23:30 drawn as end line)
#define TGRID_W_PX_HOUR 72    // pixels per hour (30-min events show title)

// ── Display layout (960×540) ──────────────────────────────────────────────────
#define SCREEN_W                     960
#define SCREEN_H                     540

// Header row
#define HEADER_H                     38

// Weekly view week strip (below header)
#define WEEK_STRIP_Y                 (HEADER_H + 2)
#define WEEK_STRIP_H                 76
#define WEEK_COL_W                   (SCREEN_W / 7)   // 137px per day

// Daily event list (below week strip in weekly view; full below header in daily view)
#define EVENT_ROW_H                  26
#define EVENT_TIME_W                 66    // left column width for time

// Meeting mode split
#define MEETING_SPLIT_X              460   // left panel ends here
