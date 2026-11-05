#pragma once

#include <Inkplate.h>
#include <ArduinoJson.h>
#include "../Utils/utils.h"

// Draws the section title and a "no Wi-Fi" note in place of the departures
void drawOfflineMessage(Inkplate &display, const FontCollection &fonts, const char *title, int xpos, int startYpos, int row_height);

bool drawDepartures(Inkplate &display, const FontCollection &fonts, const char *siteId, const char *transportType, String title, int xpos, int startYpos, int lineBadgeWidth, bool useInformalTime, int row_height);
