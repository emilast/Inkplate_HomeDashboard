#include "DeparturesDisplay.h"
#include "../Utils/DisplayManager.h"
#include "../Utils/utils.h"

static const int rightMargin = 25; // right margin on screen

// Draws one departure row and its deviation messages, returns the new ypos
static int drawDeparture(Inkplate &display, const FontCollection &fonts, const Departure &departure, int xpos, int ypos, int lineBadgeWidth, bool useInformalTime, int rowHeight)
{
    const int rightPos = E_INK_WIDTH - rightMargin;

    char scheduledTimeBuffer[10];
    if (departure.scheduledHour >= 0 && departure.scheduledMinute >= 0)
    {
        snprintf(scheduledTimeBuffer, sizeof(scheduledTimeBuffer), "%02d:%02d", departure.scheduledHour, departure.scheduledMinute);
    }
    else
    {
        snprintf(scheduledTimeBuffer, sizeof(scheduledTimeBuffer), "--:--");
    }

    char timeBuffer[100];

    if (useInformalTime)
    {
        snprintf(timeBuffer, sizeof(timeBuffer), "%s", departure.displayTime);
    }
    else if (departure.expectedHour >= 0 && departure.expectedMinute >= 0)
    {
        snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", departure.expectedHour, departure.expectedMinute);
    }
    else
    {
        snprintf(timeBuffer, sizeof(timeBuffer), "%s", departure.displayTime[0] != '\0' ? departure.displayTime : "--:--");
    }

    const int marginToOriginalTime = 25;

    ypos += rowHeight;

    // Line badge and destination are the same for every kind of departure
    drawInvertedString(display, fonts.normalTextFont, departure.line, xpos, ypos, lineBadgeWidth);
    display.printf(" %s", departure.destination);

    // Only show a delay if it is more than 1 minute
    bool delayed = departure.expectedHour >= 0 && departure.scheduledHour >= 0 &&
                   abs(minutesBetween(departure.scheduledHour, departure.scheduledMinute, departure.expectedHour, departure.expectedMinute)) > 1;

    if (departure.cancelled || delayed)
    {
        // Struck-through scheduled time at the right edge, and "INSTÄLLD" or the new time before it
        int w = drawRightString(display, fonts.normalTextFont, scheduledTimeBuffer, rightPos, ypos, true);
        const char *statusText = departure.cancelled ? "INST\xc4LLD" : timeBuffer;
        drawRightString(display, fonts.normalTextFont, statusText, rightPos - w - marginToOriginalTime, ypos, false);
    }
    else
    {
        // On-time departure
        drawRightString(display, fonts.normalTextFont, timeBuffer, rightPos, ypos, false);
    }
    display.println();

    const GFXfont &deviationFont = fonts.smallTextFont;

    for (int i = 0; i < departure.deviationCount; i++)
    {
        display.setFont(&deviationFont);

        ypos += deviationFont.yAdvance;
        display.setCursor(xpos, ypos);
        display.printf("-");
        int linesDrawn = drawWrappedText(display, deviationFont, departure.deviations[i], xpos + 20, ypos, E_INK_WIDTH - xpos - 20 - rightMargin);

        ypos += (linesDrawn - 1) * deviationFont.yAdvance;
    }

    return ypos;
}

void drawDepartures(Inkplate &display, const FontCollection &fonts, const DepartureList &departures, const char *title, int xpos, int startYpos, int lineBadgeWidth, bool useInformalTime, int rowHeight)
{
    const int maxHeight = 320;

    display.setTextColor(_BLACK, _WHITE);
    drawHeader(display, fonts.boldTextFont, title, xpos, startYpos);

    int ypos = startYpos + rowHeight;
    display.setCursor(xpos, ypos);
    display.setFont(&fonts.normalTextFont);

    switch (departures.status)
    {
    case DepartureStatus::HttpError:
        display.printf("Fel vid API-anrop (%d):", departures.httpCode);
        ypos += fonts.smallTextFont.yAdvance + 4;
        drawWrappedText(display, fonts.smallTextFont, departures.errorMessage, xpos, ypos, E_INK_WIDTH - xpos - rightMargin);
        return;
    case DepartureStatus::ParseError:
        display.print("Kunde inte tolka svaret");
        return;
    case DepartureStatus::Ok:
        break;
    }

    if (departures.count == 0)
    {
        display.println("Inga avg\xe5ngar.");
        return;
    }

    ypos = startYpos;
    for (int i = 0; i < departures.count; i++)
    {
        ypos = drawDeparture(display, fonts, departures.departures[i], xpos, ypos, lineBadgeWidth, useInformalTime, rowHeight);

        // Limit the number of rows
        if ((ypos - startYpos) >= maxHeight)
        {
            break;
        }
    }
}

void drawOfflineMessage(Inkplate &display, const FontCollection &fonts, const char *title, int xpos, int startYpos, int rowHeight)
{
    display.setTextColor(_BLACK, _WHITE);
    drawHeader(display, fonts.boldTextFont, title, xpos, startYpos);
    display.setCursor(xpos, startYpos + rowHeight);
    display.setFont(&fonts.normalTextFont);
    display.print("Ingen WiFi-anslutning");
}
