#pragma once
#include <stdint.h>
#include "calendar_data.h"

void layout_weekly_draw(uint8_t *fb, const CalendarData &data, time_t now);

// Returns screen Y of the current-time marker, or -1 if outside 08:00–21:00.
int layout_weekly_timeMarkerY(time_t now);
