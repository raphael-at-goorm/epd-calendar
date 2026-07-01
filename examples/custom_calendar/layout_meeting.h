#pragma once
#include <stdint.h>
#include "calendar_data.h"

// Draw the split meeting mode screen.
// Left panel: today's timeline (09:00–23:30) with event blocks.
// Right panel: selected event detail with attendee response status.
void layout_meeting_draw(uint8_t *fb, const CalendarData &data, time_t now,
                          const CalendarEvent &meeting);
