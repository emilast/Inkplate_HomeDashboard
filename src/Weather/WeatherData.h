#ifndef WEATHERDATA_H
#define WEATHERDATA_H

#include <Arduino.h>

#define GRAPH_HOURS 24
#define FORECAST_DAYS 3

class WeatherData {
public:
    // Member variables
    float currentTemp;
    float feelsLike;
    float precipitation;
    float windSpeed;
    float dailyMinTemp[FORECAST_DAYS];
    float dailyMaxTemp[FORECAST_DAYS];
    String dailyNames[FORECAST_DAYS];
    String weatherDescription;
    int weatherCode;
    int dailyWeatherCodes[FORECAST_DAYS];
    float dailyPrecipMin[FORECAST_DAYS]; // sum of hourly minimum precipitation
    float dailyPrecip[FORECAST_DAYS];    // sum of hourly maximum precipitation
    bool dailyHasData[FORECAST_DAYS];
    float hourlyTemps[GRAPH_HOURS];
    float hourlyPrecipMin[GRAPH_HOURS];
    float hourlyPrecip[GRAPH_HOURS];
    String hourlyTimes[GRAPH_HOURS];
};

#endif
