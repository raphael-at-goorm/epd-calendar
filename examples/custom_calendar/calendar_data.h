#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"

struct CalendarEvent {
    char    summary[128];
    char    location[128];
    time_t  startTime;
    time_t  endTime;
    bool    isAllDay;
    char    attendees[MAX_ATTENDEES][64];
    char    attendeeStatus[MAX_ATTENDEES][16];  // "accepted","declined","tentative","needsAction"
    int     attendeeCount;
};

struct CalendarData {
    CalendarEvent events[MAX_EVENTS];
    int           eventCount;
    time_t        lastFetched;
};

// Returns the index of the first event that overlaps [now, now+MEETING_WARN_SECONDS),
// or -1 if none.
inline int findActiveMeetingIndex(const CalendarData &data, time_t now) {
    for (int i = 0; i < data.eventCount; i++) {
        const CalendarEvent &e = data.events[i];
        if (e.isAllDay) continue;
        if (now >= (e.startTime - MEETING_WARN_SECONDS) && now < e.endTime) {
            return i;
        }
    }
    return -1;
}

// Returns today's events (start time falls within the calendar day of `now`).
inline int getTodayEventIndices(const CalendarData &data, time_t now,
                                int *outIndices, int maxOut) {
    struct tm local;
    localtime_r(&now, &local);
    local.tm_hour = 0; local.tm_min = 0; local.tm_sec = 0;
    time_t dayStart = mktime(&local);
    local.tm_hour = 23; local.tm_min = 59; local.tm_sec = 59;
    time_t dayEnd = mktime(&local);

    int n = 0;
    for (int i = 0; i < data.eventCount && n < maxOut; i++) {
        time_t s = data.events[i].startTime;
        if (s >= dayStart && s <= dayEnd) outIndices[n++] = i;
    }
    return n;
}

// Returns events whose startTime falls in Mon-Sun of the week containing `now`.
inline int getWeekEventIndices(const CalendarData &data, time_t now,
                               int *outIndices, int maxOut) {
    struct tm lt;
    localtime_r(&now, &lt);
    int dow = (lt.tm_wday == 0) ? 6 : (lt.tm_wday - 1);
    lt.tm_mday -= dow;
    lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0;
    time_t weekStart = mktime(&lt);
    time_t weekEnd   = weekStart + 7 * 86400;

    int n = 0;
    for (int i = 0; i < data.eventCount && n < maxOut; i++) {
        if (data.events[i].isAllDay) continue;
        time_t s = data.events[i].startTime;
        if (s >= weekStart && s < weekEnd) outIndices[n++] = i;
    }
    return n;
}
