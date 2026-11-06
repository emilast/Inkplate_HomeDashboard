#pragma once

#include "WeatherData.h"

// Fetches the SMHI forecast for the given position into weather. Returns false,
// leaving weather unchanged, if the request or the parsing fails.
bool fetchWeather(WeatherData &weather, float latitude, float longitude);
