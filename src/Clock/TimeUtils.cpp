#include "TimeUtils.h"
#include <time.h>
#include <Inkplate.h>

void printLocalTime(Inkplate &display, const FontCollection &fonts, int xpos, int ypos)
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return;
    }
    char timeBuffer[20];
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", &timeinfo);
    Serial.println(timeBuffer);
    display.setCursor(xpos, ypos);
    display.setFont(&fonts.largeTextFont);
    display.println(timeBuffer);
    display.setFont(&fonts.normalTextFont);
}
