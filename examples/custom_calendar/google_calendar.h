#pragma once
#include "calendar_data.h"

// Fetch events from Google Calendar API into `data`.
// Uses gauth_getAccessToken() internally.
// Returns true on success.
bool gcal_fetchEvents(CalendarData &data);
