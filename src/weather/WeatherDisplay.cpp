#include "Inkplate.h"

#include "WeatherDisplay.h"
#include "../Utils/DisplayManager.h"
#include "WeatherClient.h"

#include "icons/tempSymbol.h" // Include .h file that contains byte array for temperature symbol.
#include "icons/icon_s_clear_sky.h"
#include "icons/icon_s_partly_cloudy.h"
#include "icons/icon_s_gray.h"
#include "icons/icon_s_fog.h"
#include "icons/icon_s_rain.h"
#include "icons/icon_s_snow.h"
#include "icons/icon_s_storm.h"
#include "icons/icon_s_thermometer.h"

// Latitude and longitude of the position to show weather for
float latitude = 59.255798826517136;
float longitude = 17.98211146904839;

// Weather
WeatherClient weatherClient;
WeatherData weatherData;

const uint8_t *getWeatherIcon(int code)
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

void drawTemperature(Inkplate &display, int xpos, int ypos)
{
    int temperature = display.readTemperature();
    display.drawImage(tempSymbol, xpos, ypos, 38, 79, _BLACK);
    display.setCursor(xpos + 55, ypos + 50);
    display.print(temperature, DEC);
    display.print("\xb0"
                  "C");
}

int getTemperatureGraphY(float temperature, float paddedTempMin, float paddedTempMax, int chartBottom, int graphHeight, int marginY)
{
    float tempRange = paddedTempMax - paddedTempMin;
    if (tempRange == 0)
        tempRange = 1; // Avoid division by zero
    return chartBottom - ((temperature - paddedTempMin) / tempRange) * (graphHeight - 2 * marginY);
}


int calculateLocalTimeZoneOffset(WeatherData *weatherData)
{
    struct tm localTime;
    if (!getLocalTime(&localTime)) return 0;

    // Parse the first hourly time (format "HH:MM")
    int weatherHour = 0, weatherMin = 0;
    sscanf(weatherData->hourlyTimes[0].c_str(), "%d:%d", &weatherHour, &weatherMin);

    // Calculate the difference in hours between local time and weather time
    int localHour = localTime.tm_hour;
    int diff = localHour - weatherHour;

    // If closer to the next hour, then SMHI seems to use the next hour as the first hour in the hourly forecast.
    if (localTime.tm_min > 30)
    {
        diff++; // Adjust for minutes if local time is earlier in the hour
    }

    // Log the local time and weather time for debugging
    Serial.printf("Local time: %02d:%02d, Weather time: %02d:%02d, Difference: %d hours\n",
                    localHour, localTime.tm_min, weatherHour, weatherMin, diff);

    // Adjust for possible day wrap-around
    if (diff < -12)
        diff += 24;
    if (diff > 12)
        diff -= 24;

    int timeZoneOffset = diff;
    Serial.printf("Calculated timeZoneOffset: %d\n", timeZoneOffset);

    return timeZoneOffset;
}

// Draw dithered gray bar
void drawBarGray(Adafruit_GFX &display, int x, int chartBottom, int barHeight, int xStep) {
#if MONOCHROME
    // In monochrome mode, draw dithered pattern for gray
    for (int dy = 0; dy < barHeight; ++dy) {
        for (int dx = 0; dx < xStep + 1; ++dx) {
        // Simple Bayer 2x2 dithering pattern
            if (((((x + dx) & 1) ^ ((chartBottom - barHeight + dy) & 1)) == 0)) {
                display.drawPixel(x + dx, chartBottom - barHeight + dy, _BLACK);
            }
        }
    }
#else
    // In grayscale mode, use a medium gray color
    display.fillRect(x, chartBottom - barHeight, xStep + 1, barHeight, _GRAY5);
#endif
}

// --- Draw Temperature & Precipitation Graph ---
void drawTemperaturePrecipGraph(Inkplate &display, const FontCollection &fonts, WeatherData *weatherData, float currentTemp, int xpos, int ypos, int graphWidth, int graphHeight)
{
    // Layout values for graph placement
    int graphX = xpos;
    int graphY = ypos;

    display.setFont(&fonts.smallTextFont); // Set the font

    // display.setCursor(graphX-35, graphY+5);
    // // display.setFont(&FreeSans12pt7b);
    // display.setTextColor(BLACK);
    // display.print("Hourly temperature and precipitation");

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
        if (weatherData->hourlyTemps[i] < actualTempMin)
            actualTempMin = weatherData->hourlyTemps[i];
        if (weatherData->hourlyTemps[i] > actualTempMax)
            actualTempMax = weatherData->hourlyTemps[i];

        // Exclude the last value from precipMax since it's not rendered
        if (i < GRAPH_HOURS - 1 && weatherData->hourlyPrecip[i] > precipMax)
        {
            precipMax = weatherData->hourlyPrecip[i];
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

    // Calculate Y positions for min, and max temperatures
    int yMin = chartBottom - ((paddedTempMin - paddedTempMin) / tempRange) * (graphHeight - 2 * marginY);
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
    int yCurrent = getTemperatureGraphY(currentTemp, paddedTempMin, paddedTempMax, chartBottom, graphHeight, marginY);

#if MONOCHROME
    display.drawThickLine(chartLeft, yCurrent, chartLeft + graphWidth - 2 * marginX, yCurrent, _BLACK, 3);
#else
    display.drawThickLine(chartLeft, yCurrent, chartLeft + graphWidth - 2 * marginX, yCurrent, _GRAY4, 3);
#endif

    // Log min, max and current y coordinates for debugging
    Serial.printf("Min Y: %d, Max Y: %d, Current Y: %d\n", yMinActual, yMaxActual, yCurrent);
    // Log the actual min and max temperatures
    Serial.printf("Actual Min Temp: %.1f, Actual Max Temp: %.1f, Current Temp: %.1f\n", actualTempMin, actualTempMax, currentTemp);

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
    display.print(currentTemp, 1); // Show temperature with 1 decimal place
    display.print("\xb0");

    // Draw precipitation bars
    for (int i = 0; i < GRAPH_HOURS - 1; i++) // -1 to avoid out of bounds access, will render one bar too few but that's okay
    {
        // weatherData->hourlyPrecip[i] = random(0, 100);

        if (weatherData->hourlyPrecip[i] == 0)
            continue; // Skip drawing bars for zero precipitation

        int x = chartLeft + i * xStep;
        int barHeightMin = (precipScale > 0) ? (weatherData->hourlyPrecipMin[i] / precipScale) * (marginY) : 0;
        int barHeight = (precipScale > 0) ? (weatherData->hourlyPrecip[i] / precipScale) * (marginY) : 0;

        Serial.printf("Min precipitation for hour %d: %f, barHeight: %d\n", i, weatherData->hourlyPrecipMin[i], barHeightMin);
        Serial.printf("Max precipitation for hour %d: %f, barHeight: %d\n", i, weatherData->hourlyPrecip[i], barHeight);

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
            snprintf(buf, sizeof(buf), "%.1f - %.1f", weatherData->hourlyPrecipMin[i], weatherData->hourlyPrecip[i]);

            int16_t tbx, tby;
            uint16_t tbw, tbh;
            display.getTextBounds(String(buf), 0, 0, &tbx, &tby, &tbw, &tbh);

            int chartRight = xpos + graphWidth - 2 * marginX;
            int printX = x;
            // If text would extend past right edge, shift left
            if (printX + (int)tbw > chartRight) {
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

        // int y1 = chartBottom - ((weatherData->hourlyTemps[i] - paddedTempMin) / tempRange) * (graphHeight - 2 * marginY);
        int y1 = getTemperatureGraphY(weatherData->hourlyTemps[i], paddedTempMin, paddedTempMax, chartBottom, graphHeight, marginY);
        int y2 = getTemperatureGraphY(weatherData->hourlyTemps[i + 1], paddedTempMin, paddedTempMax, chartBottom, graphHeight, marginY);

        // Draw a thicker line by drawing multiple parallel lines
        for (int offset = -1; offset <= 1; ++offset)
        {
            display.drawLine(x1, y1 + offset, x2, y2 + offset, _BLACK);
        }
        // display.drawThickLine(x1, y1, x2, y2, _BLACK, 3);
    }

    // Calculate timezone offset using getLocalTime() and first hourlyTimes element
    int timeZoneOffset = calculateLocalTimeZoneOffset(weatherData); // fallback default

    // Time labels under X-axis
    for (int i = 0; i < GRAPH_HOURS; i++)
    {
        int x = chartLeft + i * xStep;
        // Draw vertical grid lines for each hour
        // Convert weather time to local time using calculated timeZoneOffset
        int weatherHour = 0, weatherMin = 0;
        sscanf(weatherData->hourlyTimes[i].c_str(), "%d:%d", &weatherHour, &weatherMin);

        int localHour = (weatherHour + timeZoneOffset + 24) % 24;

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

        // Show time labels
        // if (i % 6 == 0) {
        //   display.setCursor(x + 3, chartBottom + 20);
        //   display.print(weatherData->hourlyTimes[i]);
        // }
    }
}

void drawWeather(Inkplate &display, const FontCollection &fonts, int xpos, int ypos, int row_height, int framesDrawn)
{
    if (framesDrawn == 0)
    {
        weatherClient.fetchWeatherData(&weatherData, &latitude, &longitude);
    }

    const int label_width = 225;

    drawHeader(display, fonts.boldTextFont, "V\xE4"
                               "der",
               xpos, ypos);
    ypos += row_height;

    ypos += 20; // Add some space before the weather info

    Serial.println("Weather code: " + String(weatherData.weatherCode));
    display.drawBitmap(xpos, ypos - 40, getWeatherIcon(weatherData.weatherCode), 48, 48, _BLACK);
    int wrappedLines = drawWrappedText(display, fonts.normalTextFont, weatherData.weatherDescription, xpos + 75, ypos, 325);

    ypos += row_height * wrappedLines;

    display.drawBitmap(xpos, ypos - 35, icon_s_thermometer, 48, 48, _BLACK);
    display.setCursor(xpos + 75, ypos);
    display.print(String(weatherData.currentTemp, 1));
    display.print("\xb0"
                  "C");

    ypos += row_height;

    ypos += 20; // Add some space before the next row

    // UV index missing from SMHI API
    // display.setCursor(xpos, ypos);
    // display.print("UV Index:");
    // display.setCursor(xpos + label_width, ypos);
    // display.println(weatherData.uvIndex);

    // ypos += row_height;

    display.setCursor(xpos, ypos);
    display.print("Vind:");
    display.setCursor(xpos + label_width, ypos);
    display.print(weatherData.windSpeed, 1); // Divide by 3.6 if value is in km/h
    display.println("m/s");

    // ypos += row_height;

    drawTemperaturePrecipGraph(display, fonts, &weatherData, weatherData.currentTemp, xpos + 20, ypos, 350, 250);

    drawDailyForecast(display, fonts, &weatherData, xpos, ypos + 270, 370);
}

void drawDailyForecast(Inkplate &display, const FontCollection &fonts, WeatherData *weatherData, int xpos, int ypos, int widgetWidth)
{
    if (!weatherData) return;
    if (weatherData->dailyNames[0].length() == 0) return;

    int numDays = FORECAST_DAYS;
    int colWidth = widgetWidth / numDays;

    // Top divider line below 24-hour diagram
    display.drawLine(xpos + 5, ypos - 10, xpos + widgetWidth - 5, ypos - 10, _BLACK);

    for (int d = 0; d < numDays; d++)
    {
        int colLeft = xpos + d * colWidth;
        int colCenterX = colLeft + colWidth / 2;

        // Subtle vertical separator between columns
        if (d > 0)
        {
#if MONOCHROME
            display.drawLine(colLeft, ypos - 4, colLeft, ypos + 115, _BLACK);
#else
            display.drawLine(colLeft, ypos - 4, colLeft, ypos + 115, _GRAY4);
#endif
        }

        // 1. Day Name Header
        // display.setFont(&fonts.boldTextFont);
        display.setFont(&fonts.smallTextFont);
        display.setTextColor(_BLACK, _WHITE);
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(weatherData->dailyNames[d].c_str(), 0, 0, &x1, &y1, &w, &h);
        display.setCursor(colCenterX - (w / 2), ypos + 17);
        display.print(weatherData->dailyNames[d]);

        // 2. Weather Icon (48x48)
        int iconX = colCenterX - 24;
        int iconY = ypos + 20;
        display.drawBitmap(iconX, iconY, getWeatherIcon(weatherData->dailyWeatherCodes[d]), 48, 48, _BLACK);

        // 3. Min/Max Temperature
        display.setFont(&fonts.smallTextFont);
        char tempBuf[32];
        snprintf(tempBuf, sizeof(tempBuf), "%.0f\xb0 / %.0f\xb0", weatherData->dailyMaxTemp[d], weatherData->dailyMinTemp[d]);
        display.getTextBounds(tempBuf, 0, 0, &x1, &y1, &w, &h);
        display.setCursor(colCenterX - (w / 2), iconY + 48 + 22);
        display.print(tempBuf);

        // 4. Daily Precipitation
        char precipBuf[32];
        snprintf(precipBuf, sizeof(precipBuf), "%.1f mm", weatherData->dailyPrecip[d]);
        display.getTextBounds(precipBuf, 0, 0, &x1, &y1, &w, &h);
        display.setCursor(colCenterX - (w / 2), iconY + 48 + 44);
        display.print(precipBuf);
    }
}