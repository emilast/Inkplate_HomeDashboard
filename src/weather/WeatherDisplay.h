#pragma once
#include <Inkplate.h>
#include "../Utils/utils.h"
#include "WeatherData.h"

void drawWeather(Inkplate &display, const FontCollection &fonts, int xpos, int ypos, int row_height, int framesDrawn);
void drawDailyForecast(Inkplate &display, const FontCollection &fonts, WeatherData *weatherData, int xpos, int ypos, int widgetWidth);
