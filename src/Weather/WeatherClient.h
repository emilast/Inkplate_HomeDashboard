#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>
#include <HTTPClient.h>
#include "WeatherData.h"
#include <Inkplate.h>
#include <Arduino.h>

class WeatherClient {
public:

    // --- Struct UserInfo ---
    struct UserInfo {
      String lastUpdated;
      int currentHour;
      String city;
      String username;
      String lastUpdatedDate;
      String lastUpdatedTime;
      bool apiError;
      bool useMetric;
      String temperatureLabel;
      String speedLabel;
    };

    // --- Public methods ---
    void fetchWeatherData(WeatherData* weatherData, const float* latitude, const float* longitude);

private:
    // --- Private helper methods ---
    String getWeatherDescription(int code);
    String extractSun(String dateTime);
    String getDayName(int dayIndex);
};

#endif
