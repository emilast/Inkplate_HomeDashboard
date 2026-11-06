#pragma once

#include <Inkplate.h>
#include "../Utils/FontCollection.h"
#include "DeparturesData.h"

// Draws the section title and the departures, or the error from a failed fetch
void drawDepartures(Inkplate &display, const FontCollection &fonts, const DepartureList &departures, const char *title, int xpos, int startYpos, int lineBadgeWidth, bool useInformalTime, int rowHeight);

// Draws the section title and a "no Wi-Fi" note in place of the departures
void drawOfflineMessage(Inkplate &display, const FontCollection &fonts, const char *title, int xpos, int startYpos, int rowHeight);
