#include "google_calendar.h"
#include "google_auth.h"
#include "secrets.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// Count days from 1970-01-01 to the given date (UTC, no library needed).
static time_t dateToDays(int Y, int M, int D) {
    auto isLeap = [](int y){ return (y%4==0 && y%100!=0) || y%400==0; };
    static const int mdays[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    int days = 0;
    for (int y = 1970; y < Y; y++) days += isLeap(y) ? 366 : 365;
    for (int mo = 1; mo < M; mo++) { days += mdays[mo]; if (mo==2 && isLeap(Y)) days++; }
    return (time_t)(days + D - 1);
}

// Parse ISO8601 datetime "2026-04-22T14:00:00+09:00" → UTC time_t.
// No timegm() needed.
static time_t parseISO8601(const char *s) {
    if (!s || !*s) return 0;
    int Y=0, Mo=0, D=0, h=0, m=0, sc=0, tz_h=0, tz_m=0;
    sscanf(s, "%d-%d-%dT%d:%d:%d", &Y, &Mo, &D, &h, &m, &sc);
    if (Y < 1970) return 0;

    // Find timezone marker after the time portion
    const char *tz = s + 19;
    while (*tz && *tz != 'Z' && *tz != '+' && *tz != '-') tz++;
    char sign = *tz ? *tz : 'Z';
    if (sign != 'Z') sscanf(tz + 1, "%d:%d", &tz_h, &tz_m);

    time_t utc = dateToDays(Y, Mo, D) * 86400
               + (time_t)h * 3600 + m * 60 + sc;
    if (sign == '+') utc -= tz_h * 3600 + tz_m * 60;
    else if (sign == '-') utc += tz_h * 3600 + tz_m * 60;
    return utc;
}

// Parse date-only "2026-04-22" → midnight UTC time_t.
static time_t parseDateOnly(const char *s) {
    if (!s || !*s) return 0;
    int Y=0, M=0, D=0;
    sscanf(s, "%d-%d-%d", &Y, &M, &D);
    return dateToDays(Y, M, D) * 86400;
}

bool gcal_fetchEvents(CalendarData &data) {
    String token = gauth_getAccessToken();
    if (token.isEmpty()) {
        Serial.println("[gcal] No access token");
        return false;
    }

    // timeMin = this week's Sunday midnight (so past days this week are shown)
    // timeMax = now + CALENDAR_DAYS_AHEAD (keeps future coverage intact)
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0;
    lt.tm_mday -= lt.tm_wday;  // rewind to Sunday (tm_wday=0 is Sunday)
    time_t weekStart = mktime(&lt);
    time_t tmax = now + (time_t)CALENDAR_DAYS_AHEAD * 86400;

    char timeMin[32], timeMax[32];
    struct tm tm_min, tm_max;
    gmtime_r(&weekStart, &tm_min);
    gmtime_r(&tmax,      &tm_max);
    strftime(timeMin, sizeof(timeMin), "%Y-%m-%dT%H:%M:%SZ", &tm_min);
    strftime(timeMax, sizeof(timeMax), "%Y-%m-%dT%H:%M:%SZ", &tm_max);

    String calId = String(GOOGLE_CALENDAR_ID);
    // URL-encode @ sign
    calId.replace("@", "%40");

    String url = String("https://www.googleapis.com/calendar/v3/calendars/")
               + calId
               + "/events?singleEvents=true&orderBy=startTime&maxResults="
               + String(MAX_EVENTS)
               + "&timeMin=" + timeMin
               + "&timeMax=" + timeMax;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Accept-Encoding", "identity");  // disable gzip

    int code = http.GET();
    String body = http.getString();
    http.end();

    Serial.printf("[gcal] HTTP %d  body_len=%d\n", code, body.length());
    if (code != 200) {
        Serial.printf("[gcal] body: %.200s\n", body.c_str());
        return false;
    }

    JsonDocument *doc = new (ps_malloc(sizeof(JsonDocument))) JsonDocument();
    DeserializationError err = deserializeJson(*doc, body);

    if (err) {
        Serial.printf("[gcal] JSON error: %s\n", err.c_str());
        Serial.printf("[gcal] body start: %.100s\n", body.c_str());
        doc->~JsonDocument();
        free(doc);
        return false;
    }

    JsonArray items = (*doc)["items"].as<JsonArray>();
    data.eventCount = 0;

    for (JsonObject item : items) {
        if (data.eventCount >= MAX_EVENTS) break;

        // Skip working-location entries (사무실/재택 근무위치 이벤트)
        const char *evType = item["eventType"] | "";
        if (strcmp(evType, "workingLocation") == 0) continue;

        CalendarEvent &ev = data.events[data.eventCount];
        memset(&ev, 0, sizeof(ev));

        const char *summary = item["summary"] | "(제목 없음)";
        strncpy(ev.summary, summary, sizeof(ev.summary) - 1);

        const char *loc = item["location"] | "";
        strncpy(ev.location, loc, sizeof(ev.location) - 1);

        // Start time
        const char *startDT   = item["start"]["dateTime"] | "";
        const char *startDate = item["start"]["date"]     | "";
        if (startDT[0]) {
            ev.startTime = parseISO8601(startDT);
            ev.isAllDay  = false;
        } else {
            ev.startTime = parseDateOnly(startDate);
            ev.isAllDay  = true;
        }

        // End time
        const char *endDT   = item["end"]["dateTime"] | "";
        const char *endDate = item["end"]["date"]     | "";
        if (endDT[0]) {
            ev.endTime = parseISO8601(endDT);
        } else {
            ev.endTime = parseDateOnly(endDate) + 86400;
        }

        // Attendees + response status
        // Rule: if others exist, exclude self; if only self, include self (show 1).
        JsonArray att = item["attendees"].as<JsonArray>();
        ev.attendeeCount = 0;
        int nonSelfCount = 0;
        for (JsonObject a : att) { if (!(a["self"] | false)) nonSelfCount++; }
        bool onlySelf = (nonSelfCount == 0);
        for (JsonObject a : att) {
            if (ev.attendeeCount >= MAX_ATTENDEES) break;
            bool isSelf = a["self"] | false;
            if (isSelf && !onlySelf) continue;
            const char *disp  = a["displayName"] | "";
            const char *email = a["email"]       | "";
            bool hasName = (disp[0] != '\0' && strchr(disp, '@') == nullptr);
            if (hasName) {
                strncpy(ev.attendees[ev.attendeeCount], disp,
                        sizeof(ev.attendees[0]) - 1);
            } else if (email[0] != '\0') {
                strncpy(ev.attendees[ev.attendeeCount], email,
                        sizeof(ev.attendees[0]) - 1);
            }
            const char *rs = a["responseStatus"] | "needsAction";
            strncpy(ev.attendeeStatus[ev.attendeeCount], rs,
                    sizeof(ev.attendeeStatus[0]) - 1);
            ev.attendeeCount++;
        }

        data.eventCount++;
    }

    data.lastFetched = now;
    Serial.printf("[gcal] Fetched %d events\n", data.eventCount);

    doc->~JsonDocument();
    free(doc);
    return true;
}
