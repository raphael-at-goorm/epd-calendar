#pragma once
#include <stdint.h>
#include "calendar_data.h"

// Draw the full-screen daily view into fb.
void layout_daily_draw(uint8_t *fb, const CalendarData &data, time_t now);

// Draw a compact daily view into a restricted horizontal band [x, x+w].
// Used as the left panel in meeting mode.
void layout_daily_drawCompact(uint8_t *fb, const CalendarData &data, time_t now,
                               int32_t x, int32_t w);
