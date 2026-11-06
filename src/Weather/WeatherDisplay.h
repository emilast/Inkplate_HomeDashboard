#pragma once
#include <Inkplate.h>
#include "../Utils/FontCollection.h"
#include "WeatherData.h"

void drawWeather(Inkplate &display, const FontCollection &fonts, const WeatherData &weather, int xpos, int ypos, int rowHeight);