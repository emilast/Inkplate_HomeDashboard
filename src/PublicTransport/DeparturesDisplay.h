#pragma once

#include <Inkplate.h>
#include <ArduinoJson.h>
#include "../Utils/utils.h"

bool drawDepartures(Inkplate &display, const FontCollection &fonts, const char *siteId, const char *transportType, String title, int xpos, int startYpos, int lineBadgeWidth, bool useInformalTime, int row_height);
