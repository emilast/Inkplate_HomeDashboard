#include "DeparturesDisplay.h"
#include "../Utils/DisplayManager.h"
#include <HTTPClient.h>

const char *apiUrl = "https://transport.integration.sl.se/v1/sites/";

const int rightMargin = 25; // right margin on screen

void parseAndDisplayDepartures(Inkplate &display, const FontCollection &fonts, JsonDocument &doc, const char *transportType, String title, int xpos, int startYpos, int lineBadgeWidth, bool useInformalTime, int row_height)
{
  int maxHeight = 320;
  int ypos = startYpos;
  const int rightPos = E_INK_WIDTH - rightMargin;

  // Clear area
  const int headerHeight = fonts.boldTextFont.yAdvance; // The header is printed with startYpos as baseline, so the etxt will apear above it
  display.fillRect(
    xpos,
    startYpos - headerHeight,
    // +3 to adjust for round errors etc when drawing fonts
    rightPos - xpos + 3,
    maxHeight + headerHeight,
    _WHITE);

  drawHeader(display, fonts.boldTextFont, title, xpos, ypos);

  JsonArray departures = doc["departures"].as<JsonArray>();
  int rows = 0;

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
    if (transport_mode == nullptr || transportType == nullptr || strcmp(transport_mode, transportType) != 0)
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

    // Example usage
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
      snprintf(timeBuffer, sizeof(timeBuffer), "%s", (displayTime && displayTime[0] != '\0') ? displayTime : "--:--");
    }

    const int marginToOriginalTime = 25;

    ypos += row_height;

    if (state != nullptr && strcmp(state, "CANCELLED") == 0)
    {
      // Cancelled departure
      drawInvertedString(display, fonts.normalTextFont, line, xpos, ypos, lineBadgeWidth);

      display.printf(" %s", isoDestination);

      int w = drawRightString(display, fonts.normalTextFont, scheduledTimeBuffer, rightPos, ypos, true);
      drawRightString(display, fonts.normalTextFont, "INST\xc4LLD", rightPos - w - marginToOriginalTime, ypos, false);
      display.println();
    }
    else if (hour >= 0 && scheduledHour >= 0 && (hour != scheduledHour || minute != scheduledMinute) && (abs((hour * 60 + minute) - (scheduledHour * 60 + scheduledMinute)) > 1))
    {
      // Delayed departure (only if delay is more than 1 minute)
      drawInvertedString(display, fonts.normalTextFont, line, xpos, ypos, lineBadgeWidth);

      display.printf(" %s", isoDestination);

      int w = drawRightString(display, fonts.normalTextFont, scheduledTimeBuffer, rightPos, ypos, true);

      drawRightString(display, fonts.normalTextFont, timeBuffer, rightPos - w - marginToOriginalTime, ypos, false);
      display.println();
    }
    else
    {
      // On-time departure
      drawInvertedString(display, fonts.normalTextFont, line, xpos, ypos, lineBadgeWidth);

      display.printf(" %s", isoDestination);
      drawRightString(display, fonts.normalTextFont, timeBuffer, E_INK_WIDTH - rightMargin, ypos, false);

      display.println();
    }

    GFXfont deviationFont = fonts.smallTextFont;

    // Show deviation messages
    JsonArray deviations = departure["deviations"].as<JsonArray>();
    if (!deviations.isNull())
    {
      for (JsonObject deviation : deviations)
      {
        if (deviation.isNull()) continue;
        const char *deviationMessage = deviation["message"] | "";
        if (deviationMessage == nullptr || deviationMessage[0] == '\0') continue;

        char isoDeviationMessage[256];
        utf8ToIso88591(deviationMessage, isoDeviationMessage, sizeof(isoDeviationMessage));

        display.setFont(&deviationFont);

        ypos += deviationFont.yAdvance;
        display.setCursor(xpos, ypos);
        display.printf("-");
        int linesDrawn = drawWrappedText(display, fonts.smallTextFont, isoDeviationMessage, xpos + 20, ypos, E_INK_WIDTH - xpos - 20 - rightMargin);

        ypos += (linesDrawn - 1) * deviationFont.yAdvance;
        rows += linesDrawn;
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
  http.useHTTP10(false);
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
      display.setCursor(xpos, startYpos);
      display.setFont(&fonts.normalTextFont);
      display.println("JSON parse error");
    }
  }
  else
  {
    // Show error message
    String response = http.getString();
    Serial.println("Error on HTTP request: " + String(httpCode) + " " + String(requestUrl) + " " + response);
    display.setCursor(xpos, startYpos);
    display.setFont(&fonts.normalTextFont);
    display.print("Error on API call (");
    display.print(httpCode);
    display.println("):");

    startYpos += row_height;
    display.setCursor(xpos, startYpos);
    display.setFont(&fonts.smallTextFont);

    if (response.length() > 0) {
      String shortResponse = response.substring(0, 200);
      drawWrappedText(display, fonts.smallTextFont, shortResponse.c_str(), xpos, startYpos, E_INK_WIDTH - xpos - rightMargin);
    } else {
      display.print("(Empty response)");
    }
  }
  http.end();
  return success;
}
