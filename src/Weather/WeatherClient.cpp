#include "WeatherClient.h"
#include <WiFi.h>          
#include <HTTPClient.h>     
#include "WeatherData.h"     

#include <ArduinoJson.h>     

#include <Inkplate.h>


// Function to get weather description based on the weather code
String WeatherClient::getWeatherDescription(int code) {
switch (code) {
        case 1: return String("Klart v\xE4""der"); // Clear sky
        case 2: return String("N\xE4""stan klart"); // Nearly clear sky
        case 3: return String("V\xE4""xlande molnighet"); // Variable cloudiness
        case 4: return String("Halvklart"); // Halfclear sky
        case 5: return String("Molnigt"); // Cloudy sky
        case 6: return String("Mulet"); // Overcast
        case 7: return String("Dimma"); // Fog
        case 8: return String("L\xE4""tta regnskurar"); // Light rain showers
        case 9: return String("M\xE5""ttliga regnskurar"); // Moderate rain showers
        case 10: return String("Kraftiga regnskurar"); // Heavy rain showers
        case 11: return String("\xC5""skv\xE4""der"); // Thunderstorm
        case 12: return String("L\xE4""tta bl\xF6""tsn\xF6""skurar"); // Light sleet showers
        case 13: return String("M\xE5""ttliga bl\xF6""tsn\xF6""skurar"); // Moderate sleet showers
        case 14: return String("Kraftiga bl\xF6""tsn\xF6""skurar"); // Heavy sleet showers
        case 15: return String("L\xE4""tta sn\xF6""byar"); // Light snow showers
        case 16: return String("M\xE5""ttliga sn\xF6""byar"); // Moderate snow showers
        case 17: return String("Kraftiga sn\xF6""byar"); // Heavy snow showers
        case 18: return String("L\xE4""tt regn"); // Light rain
        case 19: return String("M\xE5""ttligt regn"); // Moderate rain
        case 20: return String("Kraftigt regn"); // Heavy rain
        case 21: return String("\xC5""ska"); // Thunder
        case 22: return String("L\xE4""tt bl\xF6""tsn\xF6"); // Light sleet
        case 23: return String("M\xE5""ttlig bl\xF6""tsn\xF6"); // Moderate sleet
        case 24: return String("Kraftig bl\xF6""tsn\xF6"); // Heavy sleet
        case 25: return String("L\xE4""tt sn\xF6""fall"); // Light snowfall
        case 26: return String("M\xE5""ttligt sn\xF6""fall"); // Moderate snowfall
        case 27: return String("Kraftigt sn\xF6""fall"); // Heavy snowfall
        default: return String("Ok\xE4""nd v\xE4""dertyp: ") + code; // Unknown weather type
    }
}

String WeatherClient::extractSun(String dateTime) {
  int tIndex = dateTime.indexOf('T');
  if (tIndex != -1 && tIndex + 5 < dateTime.length()) {
      return dateTime.substring(tIndex + 1, tIndex + 6);  // e.g., "06:11"
  }
  return "??:??";
}

// Function to get the current day of the week and return the next 7 days dynamically
String WeatherClient::getDayName(int dayIndex) {
  static const char *daysOfWeek[] = {"S\xf6n", "M\xe5n", "Tis", "Ons", "Tor", "Fre", "L\xf6r"};
  
  // Get the current time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
      return "---";  // Return error if time can't be fetched
  }

  // Get the current day index (0 = Sunday, 1 = Monday, etc.)
  int currentDay = timeinfo.tm_wday;

  // Calculate the day of the week for the given index
  int dayOfWeekIndex = (currentDay + dayIndex) % 7;

  return String(daysOfWeek[dayOfWeekIndex]);
}


int getCurrentHour() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
      return -1;  // Return -1 if time is not available (error)
  }

  return timeinfo.tm_hour;
}


float windChill(float tempC, float windSpeedKph) {
    return 13.12 + 0.6215 * tempC - 11.37 * pow(windSpeedKph, 0.16) + 0.3965 * tempC * pow(windSpeedKph, 0.16);
}

float heatIndex(float tempC, float humidity) {
    float T = tempC;
    float R = humidity;

    return -8.784695 +
           1.61139411 * T +
           2.338549 * R +
          -0.14611605 * T * R +
          -0.012308094 * pow(T, 2) +
          -0.016424828 * pow(R, 2) +
           0.002211732 * pow(T, 2) * R +
           0.00072546 * T * pow(R, 2) +
          -0.000003582 * pow(T, 2) * pow(R, 2);
}

float feelsLikeC(float tempC, float humidity, float windSpeedMps) {
    float windKph = windSpeedMps * 3.6;

    if (tempC <= 10.0 && windKph > 4.8) {
        return windChill(tempC, windKph);
    } else if (tempC >= 27.0 && humidity >= 40.0) {
        return heatIndex(tempC, humidity);
    } else {
        return tempC;  // no correction
    }
}


static inline time_t utcToEpoch(int year, int month, int day, int hour, int minute, int second) {
    int y = year;
    int m = month;
    if (m <= 2) {
        y -= 1;
        m += 12;
    }
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = static_cast<unsigned>(y - era * 400);
    unsigned doy = (153 * (m - 3) + 2) / 5 + day - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int days = era * 146097 + static_cast<int>(doe) - 719468;
    return (time_t)days * 86400 + hour * 3600 + minute * 60 + second;
}

// Function to fetch weather data from SMHI API (SNOW1gv1)
void WeatherClient::fetchWeatherData(WeatherData* weatherData, const float* latitude, const float* longitude) {
    if (!weatherData || !latitude || !longitude) return;

    char url[320];
    snprintf(url, sizeof(url),
             "https://opendata-download-metfcst.smhi.se/api/category/snow1g/version/1/geotype/point/lon/%.6f/lat/%.6f/data.json?parameters=air_temperature,wind_speed,precipitation_amount_max,precipitation_amount_min,symbol_code,relative_humidity&timeseries=96",
             *longitude, *latitude);

    Serial.printf("Fetching weather data from: %s\n", url);

    HTTPClient http;
    http.useHTTP10(true); // Disable chunked transfer encoding for compatibility with stream parsing
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        JsonDocument filter;
        filter["timeSeries"][0]["time"] = true;
        filter["timeSeries"][0]["data"]["air_temperature"] = true;
        filter["timeSeries"][0]["data"]["wind_speed"] = true;
        filter["timeSeries"][0]["data"]["precipitation_amount_max"] = true;
        filter["timeSeries"][0]["data"]["precipitation_amount_min"] = true;
        filter["timeSeries"][0]["data"]["symbol_code"] = true;
        filter["timeSeries"][0]["data"]["relative_humidity"] = true;

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
        
        if (error) {
            Serial.printf("JSON deserialization error: %s\n", error.c_str());
            http.end();
            return;
        }

        JsonArray timeSeries = doc["timeSeries"].as<JsonArray>();
        if (timeSeries.isNull() || timeSeries.size() == 0) {
            Serial.println("No time series data available.");
            http.end();
            return;
        }

        int dataCount = timeSeries.size();
        Serial.printf("Found %d time series entries\n", dataCount);

        int forecastIndex = 0; // Current hour is always first in SMHI API data

        // Extract current weather parameters from the data object
        JsonObject currentData = timeSeries[forecastIndex]["data"];

        if (!currentData.isNull()) {
            if (currentData["air_temperature"].is<float>()) {
                weatherData->currentTemp = currentData["air_temperature"].as<float>();
            }
            if (currentData["wind_speed"].is<float>()) {
                weatherData->windSpeed = currentData["wind_speed"].as<float>();
            }
            if (currentData["precipitation_amount_max"].is<float>()) {
                weatherData->precipitation = currentData["precipitation_amount_max"].as<float>();
            }
            if (currentData["symbol_code"].is<int>()) {
                weatherData->weatherCode = currentData["symbol_code"].as<int>();
                weatherData->weatherDescription = getWeatherDescription(weatherData->weatherCode);
            }
            if (currentData["relative_humidity"].is<float>()) {
                float humidity = currentData["relative_humidity"].as<float>();
                weatherData->feelsLike = feelsLikeC(weatherData->currentTemp, humidity, weatherData->windSpeed);
            } else {
                // Without humidity the heat index can't be computed; 0 % limits the
                // calculation to wind chill.
                weatherData->feelsLike = feelsLikeC(weatherData->currentTemp, 0.0f, weatherData->windSpeed);
            }
        }

        // Set default values for sunrise/sunset
        weatherData->sunrise = "06:00";
        weatherData->sunset = "18:00";
        weatherData->uvIndex = -1;  // Not provided
        weatherData->isDay = true;

        // Fill in forecast graph values (next GRAPH_HOURS hours)
        for (int i = 0; i < GRAPH_HOURS && (forecastIndex + i) < dataCount; i++) {
            JsonObject forecastData = timeSeries[forecastIndex + i]["data"];

            if (!forecastData.isNull()) {
                if (forecastData["air_temperature"].is<float>()) {
                    weatherData->hourlyTemps[i] = forecastData["air_temperature"].as<float>();
                }
                if (forecastData["precipitation_amount_min"].is<float>()) {
                    weatherData->hourlyPrecipMin[i] = forecastData["precipitation_amount_min"].as<float>();
                }
                if (forecastData["precipitation_amount_max"].is<float>()) {
                    weatherData->hourlyPrecip[i] = forecastData["precipitation_amount_max"].as<float>();
                }
            }

            const char *timeStr = timeSeries[forecastIndex + i]["time"] | "";
            weatherData->hourlyTimes[i] = extractSun(String(timeStr));
        }

        // Initialize daily forecast aggregates
        for (int d = 0; d < FORECAST_DAYS; d++) {
            weatherData->dailyMinTemp[d] = 999.0f;
            weatherData->dailyMaxTemp[d] = -999.0f;
            weatherData->dailyPrecipMin[d] = 0.0f;
            weatherData->dailyPrecip[d] = 0.0f;
            weatherData->dailyWeatherCodes[d] = 0;
            weatherData->dailyNames[d] = "";
            weatherData->dailyHasData[d] = false;
        }

        int bestSymbolHourDiff[FORECAST_DAYS];
        for (int d = 0; d < FORECAST_DAYS; d++) {
            bestSymbolHourDiff[d] = 999;
        }

        // Determine reference "today" date in local time
        struct tm ref_today;
        bool hasRefToday = getLocalTime(&ref_today);

        for (int i = 0; i < dataCount; i++) {
            const char *timeStr = timeSeries[i]["time"] | "";
            if (strlen(timeStr) < 16) continue;

            int yr = 0, mo = 0, dy = 0, hr = 0, mn = 0;
            if (sscanf(timeStr, "%d-%d-%dT%d:%d", &yr, &mo, &dy, &hr, &mn) != 5) continue;

            time_t utc_sec = utcToEpoch(yr, mo, dy, hr, mn, 0);
            struct tm local_tm;
            localtime_r(&utc_sec, &local_tm);

            if (!hasRefToday && i == 0) {
                ref_today = local_tm;
                hasRefToday = true;
            }

            struct tm noon_entry = local_tm;
            noon_entry.tm_hour = 12;
            noon_entry.tm_min = 0;
            noon_entry.tm_sec = 0;
            time_t entry_noon_sec = mktime(&noon_entry);

            struct tm noon_ref = ref_today;
            noon_ref.tm_hour = 12;
            noon_ref.tm_min = 0;
            noon_ref.tm_sec = 0;
            time_t ref_noon_sec = mktime(&noon_ref);

            int daysAhead = (int)round((difftime(entry_noon_sec, ref_noon_sec)) / 86400.0);

            if (daysAhead >= 1 && daysAhead <= FORECAST_DAYS) {
                int d = daysAhead - 1;
                JsonObject entryData = timeSeries[i]["data"];
                if (!entryData.isNull()) {
                    if (entryData["air_temperature"].is<float>()) {
                        float t = entryData["air_temperature"].as<float>();
                        if (t < weatherData->dailyMinTemp[d]) weatherData->dailyMinTemp[d] = t;
                        if (t > weatherData->dailyMaxTemp[d]) weatherData->dailyMaxTemp[d] = t;
                    }
                    // Summing the hourly min and max gives a range for the day, matching
                    // the min - max label in the hourly graph. A sum of maximums alone
                    // overstates the expected total.
                    if (entryData["precipitation_amount_min"].is<float>()) {
                        weatherData->dailyPrecipMin[d] += entryData["precipitation_amount_min"].as<float>();
                    }
                    if (entryData["precipitation_amount_max"].is<float>()) {
                        weatherData->dailyPrecip[d] += entryData["precipitation_amount_max"].as<float>();
                    }
                    if (entryData["symbol_code"].is<int>()) {
                        int sym = entryData["symbol_code"].as<int>();
                        int hourDiff = abs(local_tm.tm_hour - 13);
                        if (hourDiff < bestSymbolHourDiff[d]) {
                            bestSymbolHourDiff[d] = hourDiff;
                            weatherData->dailyWeatherCodes[d] = sym;
                        }
                    }
                }
            }
        }

        static const char *swedishDays[7] = {
            "S\xf6n", // Söndag (0)
            "M\xe5n", // Måndag (1)
            "Tis",    // Tisdag (2)
            "Ons",    // Onsdag (3)
            "Tor",    // Torsdag (4)
            "Fre",    // Fredag (5)
            "L\xf6r"  // Lördag (6)
        };

        for (int d = 0; d < FORECAST_DAYS; d++) {
            int target_wday = (ref_today.tm_wday + (d + 1)) % 7;
            weatherData->dailyNames[d] = swedishDays[target_wday];
            // A day only counts as forecast if it got at least one temperature value
            weatherData->dailyHasData[d] = weatherData->dailyMinTemp[d] != 999.0f;
            Serial.printf("Day %d (%s): HasData=%d, Min=%.1f, Max=%.1f, Precip=%.1f-%.1f, Symbol=%d\n",
                          d + 1, weatherData->dailyNames[d].c_str(), weatherData->dailyHasData[d],
                          weatherData->dailyMinTemp[d], weatherData->dailyMaxTemp[d],
                          weatherData->dailyPrecipMin[d], weatherData->dailyPrecip[d],
                          weatherData->dailyWeatherCodes[d]);
        }

    } else {
        Serial.printf("HTTP Error: %d\n", httpCode);
    }

    http.end();
}
