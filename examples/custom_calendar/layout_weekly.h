#pragma once
#include <stdint.h>
#include "calendar_data.h"

void layout_weekly_draw(uint8_t *fb, const CalendarData &data, time_t now);
