#include "DeparturesDisplay.h"
#include "../Utils/DisplayManager.h"
#include <HTTPClient.h>

const char *apiUrl = "https://transport.integration.sl.se/v1/sites/";

const int rightMargin = 25; // right margin on screen

// Signed difference in minutes from (h1:m1) to (h2:m2), assuming the two times are
// less than 12 hours apart. Handles midnight wrap-around, e.g. 23:59 -> 00:01 = +2.
static int minutesBetween(int h1, int m1, int h2, int m2)
{
  int diff = (h2 * 60 + m2) - (h1 * 60 + m1);
  if (diff < -720)
    diff += 1440;
  else if (diff > 720)
    diff -= 1440;
  return diff;
}

void parseAndDisplayDepartures(Inkplate &display, const FontCollection &fonts, JsonDocument &doc, const char *transportType, String title, int xpos, int startYpos, int lineBadgeWidth, bool useInformalTime, int row_height)
{
  int maxHeight = 320;
  int ypos = startYpos;
  const int rightPos = E_INK_WIDTH - rightMargin;

  // Clear area
  const int headerHeight = fonts.boldTextFont.yAdvance; // The header is printed with startYpos as baseline, so the text will appear above it
  display.fillRect(
    xpos,
    startYpos - headerHeight,
    // +3 to adjust for round errors etc when drawing fonts
    rightPos - xpos + 3,
    maxHeight + headerHeight,
    _WHITE);

  drawHeader(display, fonts.boldTextFont, title, xpos, ypos);

  JsonArray departures = doc["departures"].as<JsonArray>();

  // If no departures found, display a message
  if (departures.isNull() || departures.size() == 0)
  {
    ypos += row_height;
    display.setCursor(xpos, ypos);
    display.setFont(&fonts.normalTextFont);
    display.println("Inga avg\xe5ngar.");
    return;
  }

  for (JsonObject departure : departures)
  {
    if (departure.isNull()) continue;

    const char *transport_mode = departure["line"]["transport_mode"] | "";
    if (transportType == nullptr || strcmp(transport_mode, transportType) != 0)
    {
      continue; // Skip this departure if transport_mode is not matching the transportType
    }

    const char *destination = departure["destination"] | "Ok\xe4nd destination";
    const int direction = departure["direction_code"] | 0;
    const char *displayTime = departure["display"] | ""; // "17 min", "Now"
    const char *scheduledArrival = departure["scheduled"] | "";
    const char *expectedArrival = departure["expected"] | "";
    const char *line = departure["line"]["designation"] | "";
    const char *state = departure["journey"]["state"] | "";

    Serial.printf("Departure for %s: Line %s, %s, direction = %d\n", destination, line, displayTime, direction);

    char isoDestination[128];
    utf8ToIso88591(destination, isoDestination, sizeof(isoDestination));

    int hour = -1, minute = -1;
    extractHourAndMinute(expectedArrival, hour, minute);

    int scheduledHour = -1, scheduledMinute = -1;
    extractHourAndMinute(scheduledArrival, scheduledHour, scheduledMinute);

    display.setCursor(xpos, ypos);

    char scheduledTimeBuffer[10];
    if (scheduledHour >= 0 && scheduledMinute >= 0)
    {
      snprintf(scheduledTimeBuffer, sizeof(scheduledTimeBuffer), "%02d:%02d", scheduledHour, scheduledMinute);
    }
    else
    {
      snprintf(scheduledTimeBuffer, sizeof(scheduledTimeBuffer), "--:--");
    }

    char timeBuffer[100];

    if (useInformalTime)
    {
      snprintf(timeBuffer, sizeof(timeBuffer), "%s", displayTime);
    }
    else if (hour >= 0 && minute >= 0)
    {
      snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hour, minute);
    }
    else
    {
      snprintf(timeBuffer, sizeof(timeBuffer), "%s", displayTime[0] != '\0' ? displayTime : "--:--");
    }

    const int marginToOriginalTime = 25;

    ypos += row_height;

    // Line badge and destination are the same for every kind of departure
    drawInvertedString(display, fonts.normalTextFont, line, xpos, ypos, lineBadgeWidth);
    display.printf(" %s", isoDestination);

    bool cancelled = strcmp(state, "CANCELLED") == 0;
    // Only show a delay if it is more than 1 minute
    bool delayed = hour >= 0 && scheduledHour >= 0 && abs(minutesBetween(scheduledHour, scheduledMinute, hour, minute)) > 1;

    if (cancelled || delayed)
    {
      // Struck-through scheduled time at the right edge, and "INSTÄLLD" or the new time before it
      int w = drawRightString(display, fonts.normalTextFont, scheduledTimeBuffer, rightPos, ypos, true);
      const char *statusText = cancelled ? "INST\xc4LLD" : timeBuffer;
      drawRightString(display, fonts.normalTextFont, statusText, rightPos - w - marginToOriginalTime, ypos, false);
    }
    else
    {
      // On-time departure
      drawRightString(display, fonts.normalTextFont, timeBuffer, rightPos, ypos, false);
    }
    display.println();

    const GFXfont &deviationFont = fonts.smallTextFont;

    // Show deviation messages
    JsonArray deviations = departure["deviations"].as<JsonArray>();
    if (!deviations.isNull())
    {
      for (JsonObject deviation : deviations)
      {
        if (deviation.isNull()) continue;
        const char *deviationMessage = deviation["message"] | "";
        if (deviationMessage[0] == '\0') continue;

        char isoDeviationMessage[256];
        utf8ToIso88591(deviationMessage, isoDeviationMessage, sizeof(isoDeviationMessage));

        display.setFont(&deviationFont);

        ypos += deviationFont.yAdvance;
        display.setCursor(xpos, ypos);
        display.printf("-");
        int linesDrawn = drawWrappedText(display, fonts.smallTextFont, isoDeviationMessage, xpos + 20, ypos, E_INK_WIDTH - xpos - 20 - rightMargin);

        ypos += (linesDrawn - 1) * deviationFont.yAdvance;
      }
    }

    // Limit the number of rows
    if ( (ypos-startYpos) >= maxHeight)
    {
      break;
    }
  }
}

bool drawDepartures(Inkplate &display, const FontCollection &fonts, const char *siteId, const char *transportType, String title, int xpos, int startYpos, int lineBadgeWidth, bool useInformalTime, int row_height)
{
  bool success = false;
  HTTPClient http;
  http.useHTTP10(true); // Disable chunked transfer encoding, required when parsing from getStream()
  char requestUrl[256];
  snprintf(requestUrl, sizeof(requestUrl), "%s%s/departures?transport=%s&nocache=%u",
           apiUrl, siteId, transportType, (unsigned int)random(1000, 9999));
  http.begin(requestUrl);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (!error)
    {
      parseAndDisplayDepartures(display, fonts, doc, transportType, title, xpos, startYpos, lineBadgeWidth, useInformalTime, row_height);
      success = true;
    }
    else
    {
      Serial.printf("JSON parse error for departures: %s\n", error.c_str());
      display.setTextColor(_BLACK, _WHITE);
      drawHeader(display, fonts.boldTextFont, title, xpos, startYpos);
      int ypos = startYpos + row_height;
      display.setCursor(xpos, ypos);
      display.setFont(&fonts.normalTextFont);
      display.print("Kunde inte tolka svaret");
    }
  }
  else
  {
    // Show error message
    String response = http.getString();
    Serial.println("Error on HTTP request: " + String(httpCode) + " " + String(requestUrl) + " " + response);

    display.setTextColor(_BLACK, _WHITE);
    drawHeader(display, fonts.boldTextFont, title, xpos, startYpos);

    int ypos = startYpos + row_height;
    display.setCursor(xpos, ypos);
    display.setFont(&fonts.normalTextFont);
    display.printf("Fel vid API-anrop (%d):", httpCode);

    ypos += fonts.smallTextFont.yAdvance + 4;

    String errMsg = "";
    if (response.length() > 0)
    {
      JsonDocument errDoc;
      DeserializationError err = deserializeJson(errDoc, response);
      if (!err)
      {
        if (errDoc["message"].is<const char *>())
        {
          errMsg = errDoc["message"].as<const char *>();
        }
        else if (errDoc["error"].is<const char *>())
        {
          errMsg = errDoc["error"].as<const char *>();
        }
        else if (errDoc["description"].is<const char *>())
        {
          errMsg = errDoc["description"].as<const char *>();
        }
      }

      if (errMsg.length() == 0)
      {
        errMsg = response.substring(0, 200);
      }
    }
    else
    {
      errMsg = "(Tomt svar)";
    }

    char isoErrMsg[256];
    utf8ToIso88591(errMsg.c_str(), isoErrMsg, sizeof(isoErrMsg));
    drawWrappedText(display, fonts.smallTextFont, isoErrMsg, xpos, ypos, E_INK_WIDTH - xpos - rightMargin);
  }
  http.end();
  return success;
}

void drawOfflineMessage(Inkplate &display, const FontCollection &fonts, const char *title, int xpos, int startYpos, int row_height)
{
  display.setTextColor(_BLACK, _WHITE);
  drawHeader(display, fonts.boldTextFont, title, xpos, startYpos);
  display.setCursor(xpos, startYpos + row_height);
  display.setFont(&fonts.normalTextFont);
  display.print("Ingen WiFi-anslutning");
}
