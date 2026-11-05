#pragma once
#include "Inkplate.h" // Include Inkplate library to the sketch

struct FontCollection
{
    const GFXfont &normalTextFont;
    const GFXfont &boldTextFont;
    const GFXfont &largeTextFont;
    const GFXfont &smallTextFont;
};

void utf8ToIso88591(const char *utf8, char *iso, size_t maxIsoLen = 0);
void extractHourAndMinute(const char *timestamp, int &hour, int &minute);
