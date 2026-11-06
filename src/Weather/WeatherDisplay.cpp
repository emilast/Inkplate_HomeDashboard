#include "Inkplate.h"

#include "WeatherDisplay.h"
#include "../Utils/DisplayManager.h"

#include "icons/icon_s_clear_sky.h"
#include "icons/icon_s_partly_cloudy.h"
#include "icons/icon_s_gray.h"
#include "icons/icon_s_fog.h"
#include "icons/icon_s_rain.h"
#include "icons/icon_s_snow.h"
#include "icons/icon_s_storm.h"
#include "icons/icon_s_thermometer.h"

static const uint8_t *getWeatherIcon(int code)
{
    switch (code)
    {
    case 1:
        return icon_s_clear_sky;
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
        return icon_s_partly_cloudy;
    case 7:
        return icon_s_fog;
    case 8:
    case 9:
    case 10:
        return icon_s_rain;
    case 11:
    case 21:
        return icon_s_storm;
    case 12:
    case 13:
    case 14:
    case 22:
    case 23:
    case 24:
        return icon_s_rain;
    case 15:
    case 16:
    case 17:
    case 25:
    case 26:
    case 27:
        return icon_s_snow;
    case 18:
    case 19:
    case 20:
        return icon_s_rain;
    default:
        return icon_s_gray;
    }
}

static int getTemperatureGraphY(float temperature, float paddedTempMin, float paddedTempMax, int chartBottom, int graphHeight, int marginY)
{
    float tempRange = paddedTempMax - paddedTempMin;
    if (tempRange == 0)
        tempRange = 1; // Avoid division by zero
    return chartBottom - ((temperature - paddedTempMin) / tempRange) * (graphHeight - 2 * marginY);
}

// Draw dithered gray bar
static void drawBarGray(Adafruit_GFX &display, int x, int chartBottom, int barHeight, int xStep)
{
#if MONOCHROME
    // In monochrome mode, draw dithered pattern for gray
    for (int dy = 0; dy < barHeight; ++dy)
    {
        for (int dx = 0; dx < xStep + 1; ++dx)
        {
            // Simple Bayer 2x2 dithering pattern
            if (((((x + dx) & 1) ^ ((chartBottom - barHeight + dy) & 1)) == 0))
            {
                display.drawPixel(x + dx, chartBottom - barHeight + dy, _BLACK);
            }
        }
    }
#else
    // In grayscale mode, use a medium gray color
    display.fillRect(x, chartBottom - barHeight, xStep + 1, barHeight, _GRAY5);
#endif
}

static void drawDailyForecast(Inkplate &display, const FontCollection &fonts, const WeatherData &weather, int xpos, int ypos, int widgetWidth);

// --- Draw Temperature & Precipitation Graph ---
static void drawTemperaturePrecipGraph(Inkplate &display, const FontCollection &fonts, const WeatherData &weather, int xpos, int ypos, int graphWidth, int graphHeight)
{
    // Layout values for graph placement
    int graphX = xpos;
    int graphY = ypos;

    display.setFont(&fonts.smallTextFont); // Set the font

    int marginX = 40;
    int marginY = 40;
    int chartLeft = graphX + marginX;
    int chartBottom = graphY + graphHeight - marginY;
    int chartTop = graphY + marginY;
    float actualTempMin = 100, actualTempMax = -100;
    float precipScale = 3;
    float precipMax = 0.0;
    float precipHourIx = -1;

    // Find actual min/max for temperature and precipitation
    for (int i = 0; i < GRAPH_HOURS; i++)
    {
        if (weather.hourlyTemps[i] < actualTempMin)
            actualTempMin = weather.hourlyTemps[i];
        if (weather.hourlyTemps[i] > actualTempMax)
            actualTempMax = weather.hourlyTemps[i];

        // Exclude the last value from precipMax since it's not rendered
        if (i < GRAPH_HOURS - 1 && weather.hourlyPrecip[i] > precipMax)
        {
            precipMax = weather.hourlyPrecip[i];
            precipHourIx = i; // Store the index of the hour with max precipitation
        }
    }

    // Add padding but ensure min temperature doesn't go below zero (or another reasonable value)
    float paddedTempMin = actualTempMin - 2;
    float paddedTempMax = actualTempMax + 2;

    float tempRange = paddedTempMax - paddedTempMin;
    if (tempRange == 0)
        tempRange = 1; // Avoid division by zero
    float xStep = (graphWidth - 2 * marginX) / (GRAPH_HOURS - 1.0);

    // Calculate Y position for the max temperature
    int yMax = chartBottom - ((paddedTempMax - paddedTempMin) / tempRange) * (graphHeight - 2 * marginY);

    // Draw axes
    display.drawLine(chartLeft, chartTop, chartLeft, chartBottom, _BLACK);                               // Y-axis
    display.drawLine(chartLeft, chartBottom, chartLeft + graphWidth - 2 * marginX, chartBottom, _BLACK); // X-axis
    display.drawLine(chartLeft, yMax, chartLeft + graphWidth - 2 * marginX, yMax, _BLACK);               // X-axis

    // Min temperature line
    int yMinActual = getTemperatureGraphY(actualTempMin, paddedTempMin, paddedTempMax, chartBottom, graphHeight, marginY);
    display.drawLine(chartLeft, yMinActual, chartLeft + graphWidth - 2 * marginX, yMinActual, _BLACK);

    // Max temperature line
    int yMaxActual = getTemperatureGraphY(actualTempMax, paddedTempMin, paddedTempMax, chartBottom, graphHeight, marginY);
    display.drawLine(chartLeft, yMaxActual, chartLeft + graphWidth - 2 * marginX, yMaxActual, _BLACK);

    // Current temperature line
    int yCurrent = getTemperatureGraphY(weather.currentTemp, paddedTempMin, paddedTempMax, chartBottom, graphHeight, marginY);

#if MONOCHROME
    display.drawThickLine(chartLeft, yCurrent, chartLeft + graphWidth - 2 * marginX, yCurrent, _BLACK, 3);
#else
    display.drawThickLine(chartLeft, yCurrent, chartLeft + graphWidth - 2 * marginX, yCurrent, _GRAY4, 3);
#endif

    // Log min, max and current y coordinates for debugging
    Serial.printf("Min Y: %d, Max Y: %d, Current Y: %d\n", yMinActual, yMaxActual, yCurrent);
    // Log the actual min and max temperatures
    Serial.printf("Actual Min Temp: %.1f, Actual Max Temp: %.1f, Current Temp: %.1f\n", actualTempMin, actualTempMax, weather.currentTemp);

    // Draw Min temperature label
    display.setCursor(chartLeft - 70, yMinActual + 7);
    display.print(actualTempMin, 1); // Show temperature with 1 decimal place
    display.print("\xb0");

    // Draw Max temperature label
    display.setCursor(chartLeft - 70, yMaxActual + 7);
    display.print(actualTempMax, 1); // Show temperature with 1 decimal place
    display.print("\xb0");

    // Draw current temperature label, avoid overlap with min/max labels
    const int labelSpacing = 18; // Minimum vertical spacing between labels
    int yCurrentLabel = yCurrent + 7;

    // If current label is too close to min label, move it up
    if (abs(yCurrentLabel - (yMinActual + 7)) < labelSpacing)
    {
        yCurrentLabel = yMinActual + 7 - labelSpacing;
    }

    // If current label is too close to max label, move it down
    if (abs(yCurrentLabel - (yMaxActual + 7)) < labelSpacing)
    {
        yCurrentLabel = yMaxActual + 7 + labelSpacing;
    }

    display.setCursor(chartLeft - 70, yCurrentLabel);
    display.print(weather.currentTemp, 1); // Show temperature with 1 decimal place
    display.print("\xb0");

    // Draw precipitation bars
    for (int i = 0; i < GRAPH_HOURS - 1; i++) // -1 to avoid out of bounds access, will render one bar too few but that's okay
    {
        if (weather.hourlyPrecip[i] == 0)
            continue; // Skip drawing bars for zero precipitation

        int x = chartLeft + i * xStep;
        int barHeightMin = (precipScale > 0) ? (weather.hourlyPrecipMin[i] / precipScale) * (marginY) : 0;
        int barHeight = (precipScale > 0) ? (weather.hourlyPrecip[i] / precipScale) * (marginY) : 0;

        Serial.printf("Min precipitation for hour %d: %f, barHeight: %d\n", i, weather.hourlyPrecipMin[i], barHeightMin);
        Serial.printf("Max precipitation for hour %d: %f, barHeight: %d\n", i, weather.hourlyPrecip[i], barHeight);

        // Draw the precipitation bar
        drawBarGray(display, x, chartBottom, barHeight, xStep);
        display.fillRect(x, chartBottom - barHeightMin, xStep + 1, barHeightMin, _BLACK);

        // Draw precipitation value below the bar, if it's the highest bar
        // Only show the value if it's above a certain threshold to avoid clutter
        if (precipHourIx == i)
        {
            // draw a triangle point upwards
            display.fillTriangle(
                x, chartBottom + 15,
                x + xStep / 2, chartBottom + 5,
                x + xStep, chartBottom + 15,
                _BLACK);

            // Prepare precipitation text and measure its width so it won't overflow
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f - %.1f", weather.hourlyPrecipMin[i], weather.hourlyPrecip[i]);

            int16_t tbx, tby;
            uint16_t tbw, tbh;
            display.getTextBounds(String(buf), 0, 0, &tbx, &tby, &tbw, &tbh);

            int chartRight = xpos + graphWidth - 2 * marginX;
            int printX = x;
            // If text would extend past right edge, shift left
            if (printX + (int)tbw > chartRight)
            {
                printX = chartRight - tbw;
                if (printX < chartLeft) printX = chartLeft; // ensure we don't go left of chart
            }

            display.setCursor(printX, chartBottom + 40);
            display.print(buf);

            // Print units with a small gap after the measured text
            display.setCursor(printX + tbw + 5, chartBottom + 40);
            display.print("mm");
        }
    }

    // Draw temperature line
    for (int i = 0; i < (GRAPH_HOURS - 1); i++)
    {
        int x1 = chartLeft + i * xStep;
        int x2 = chartLeft + (i + 1) * xStep;

        int y1 = getTemperatureGraphY(weather.hourlyTemps[i], paddedTempMin, paddedTempMax, chartBottom, graphHeight, marginY);
        int y2 = getTemperatureGraphY(weather.hourlyTemps[i + 1], paddedTempMin, paddedTempMax, chartBottom, graphHeight, marginY);

        // Draw a thicker line by drawing multiple parallel lines
        for (int offset = -1; offset <= 1; ++offset)
        {
            display.drawLine(x1, y1 + offset, x2, y2 + offset, _BLACK);
        }
    }

    // Time labels under X-axis
    for (int i = 0; i < GRAPH_HOURS; i++)
    {
        int x = chartLeft + i * xStep;
        // Draw vertical grid lines for each hour
        // hourlyTimes are already in local time ("HH:MM"); -1 if unknown
        int localHour = -1, localMin = 0;
        sscanf(weather.hourlyTimes[i].c_str(), "%d:%d", &localHour, &localMin);

        if (localHour == 0 || localHour == 12)
        {
            // Draw a bold line for 00:00 or 12:00
            for (int offset = -1; offset <= 1; ++offset)
            {
                display.drawLine(x + offset, chartTop, x + offset, chartBottom, _BLACK);
            }
        }
        else
        {
            display.drawLine(x, chartTop, x, chartBottom, _BLACK); // Y-axis
        }
    }
}

void drawWeather(Inkplate &display, const FontCollection &fonts, const WeatherData &weather, int xpos, int ypos, int rowHeight)
{
    const int labelWidth = 225;

    drawHeader(display, fonts.boldTextFont, "V\xE4"
                                            "der",
               xpos, ypos);
    ypos += rowHeight;

    ypos += 20; // Add some space before the weather info

    Serial.println("Weather code: " + String(weather.weatherCode));
    display.drawBitmap(xpos, ypos - 40, getWeatherIcon(weather.weatherCode), 48, 48, _BLACK);
    int wrappedLines = drawWrappedText(display, fonts.normalTextFont, weather.weatherDescription, xpos + 75, ypos, 325);

    ypos += rowHeight * wrappedLines;

    display.drawBitmap(xpos, ypos - 35, icon_s_thermometer, 48, 48, _BLACK);
    display.setCursor(xpos + 75, ypos);
    display.print(String(weather.currentTemp, 1));
    display.print("\xb0"
                  "C");

    ypos += rowHeight;

    ypos += 20; // Add some space before the next row

    display.setCursor(xpos, ypos);
    display.print("Vind:");
    display.setCursor(xpos + labelWidth, ypos);
    display.print(weather.windSpeed, 1);
    display.println("m/s");

    // Fit the graph and the daily forecast into the space left on the panel.
    // The Inkplate 10 (825 px) has room for the full-size graph, the Inkplate 5 V2
    // (720 px) needs a shorter one. If even a minimal graph leaves no room, the
    // daily forecast is left out rather than drawn off-screen.
    const int maxGraphHeight = 250;
    const int minGraphHeight = 150;
    const int graphToForecastGap = 20;
    const int forecastHeight = 120;
    const int bottomMargin = 5;

    int spaceLeft = E_INK_HEIGHT - bottomMargin - ypos;
    bool showForecast = spaceLeft - graphToForecastGap - forecastHeight >= minGraphHeight;
    int graphHeight = showForecast ? spaceLeft - graphToForecastGap - forecastHeight : spaceLeft;
    if (graphHeight > maxGraphHeight)
        graphHeight = maxGraphHeight;

    drawTemperaturePrecipGraph(display, fonts, weather, xpos + 20, ypos, 350, graphHeight);

    if (showForecast)
    {
        drawDailyForecast(display, fonts, weather, xpos, ypos + graphHeight + graphToForecastGap, 370);
    }
}

static void drawDailyForecast(Inkplate &display, const FontCollection &fonts, const WeatherData &weather, int xpos, int ypos, int widgetWidth)
{
    if (weather.dailyNames[0].length() == 0) return;

    int numDays = FORECAST_DAYS;
    int colWidth = widgetWidth / numDays;

    for (int d = 0; d < numDays; d++)
    {
        int colLeft = xpos + d * colWidth;
        int colCenterX = colLeft + colWidth / 2;

        // 1. Day Name Header
        display.setFont(&fonts.smallTextFont);
        display.setTextColor(_BLACK, _WHITE);
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(weather.dailyNames[d].c_str(), 0, 0, &x1, &y1, &w, &h);
        display.setCursor(colCenterX - (w / 2), ypos + 17);
        display.print(weather.dailyNames[d]);

        int iconY = ypos + 20;

        if (!weather.dailyHasData[d])
        {
            // No forecast entries for this day: say so instead of showing made-up values
            display.getTextBounds("-", 0, 0, &x1, &y1, &w, &h);
            display.setCursor(colCenterX - (w / 2), iconY + 48);
            display.print("-");
            continue;
        }

        // 2. Weather Icon (48x48)
        int iconX = colCenterX - 24;
        display.drawBitmap(iconX, iconY, getWeatherIcon(weather.dailyWeatherCodes[d]), 48, 48, _BLACK);

        // 3. Min/Max Temperature
        display.setFont(&fonts.smallTextFont);
        char tempBuf[32];
        snprintf(tempBuf, sizeof(tempBuf), "%.0f\xb0 / %.0f\xb0", weather.dailyMaxTemp[d], weather.dailyMinTemp[d]);
        display.getTextBounds(tempBuf, 0, 0, &x1, &y1, &w, &h);
        display.setCursor(colCenterX - (w / 2), iconY + 48 + 22);
        display.print(tempBuf);

        // 4. Daily Precipitation, as a min-max range. Whole millimetres above 10 mm
        // keep the text within the column.
        char precipBuf[32];
        float pMin = weather.dailyPrecipMin[d];
        float pMax = weather.dailyPrecip[d];
        if (pMax < 0.05f)
            snprintf(precipBuf, sizeof(precipBuf), "0 mm");
        else if (pMax >= 10.0f)
            snprintf(precipBuf, sizeof(precipBuf), "%.0f-%.0f mm", pMin, pMax);
        else
            snprintf(precipBuf, sizeof(precipBuf), "%.1f-%.1f mm", pMin, pMax);
        display.getTextBounds(precipBuf, 0, 0, &x1, &y1, &w, &h);
        display.setCursor(colCenterX - (w / 2), iconY + 48 + 44);
        display.print(precipBuf);
    }
}