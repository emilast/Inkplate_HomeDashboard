#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>
#include <HTTPClient.h>
#include "WeatherData.h"
#include <Inkplate.h>
#include <Arduino.h>

class WeatherClient {
public:

    // --- Public methods ---
    void fetchWeatherData(WeatherData* weatherData, const float* latitude, const float* longitude);

private:
    // --- Private helper methods ---
    String getWeatherDescription(int code);
};

#endif
