#include "DeparturesClient.h"
#include "DeparturesParser.h"
#include <HTTPClient.h>

static const char *const apiUrl = "https://transport.integration.sl.se/v1/sites/";

bool fetchDepartures(DepartureList &departures, const char *siteId, const char *transportType)
{
    departures.count = 0;

    HTTPClient http;
    http.useHTTP10(true); // Disable chunked transfer encoding, required when parsing from getStream()
    char requestUrl[256];
    snprintf(requestUrl, sizeof(requestUrl), "%s%s/departures?transport=%s&nocache=%u",
             apiUrl, siteId, transportType, (unsigned int)random(1000, 9999));
    http.begin(requestUrl);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK)
    {
        String response = http.getString();
        Serial.println("Error on HTTP request: " + String(httpCode) + " " + String(requestUrl) + " " + response);
        departures.status = DepartureStatus::HttpError;
        departures.httpCode = httpCode;
        parseErrorMessage(response.c_str(), departures.errorMessage, sizeof(departures.errorMessage));
        http.end();
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    http.end();
    if (error)
    {
        Serial.printf("JSON parse error for departures: %s\n", error.c_str());
        departures.status = DepartureStatus::ParseError;
        return false;
    }

    parseDepartures(doc, transportType, departures);
    for (int i = 0; i < departures.count; i++)
    {
        const Departure &d = departures.departures[i];
        Serial.printf("Departure for %s: Line %s, %s\n", d.destination, d.line, d.displayTime);
    }
    return true;
}
