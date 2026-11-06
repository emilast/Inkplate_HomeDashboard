#include "DeparturesParser.h"
#include "../Utils/utils.h"
#include <stdio.h>
#include <string.h>

static void parseDeparture(JsonObject departure, Departure &out)
{
    utf8ToIso88591(departure["line"]["designation"] | "", out.line, sizeof(out.line));
    utf8ToIso88591(departure["display"] | "", out.displayTime, sizeof(out.displayTime));

    const char *destination = departure["destination"];
    if (destination)
        utf8ToIso88591(destination, out.destination, sizeof(out.destination));
    else
        snprintf(out.destination, sizeof(out.destination), "Ok\xe4nd destination");

    extractHourAndMinute(departure["scheduled"] | "", out.scheduledHour, out.scheduledMinute);
    extractHourAndMinute(departure["expected"] | "", out.expectedHour, out.expectedMinute);

    const char *state = departure["journey"]["state"] | "";
    out.cancelled = strcmp(state, "CANCELLED") == 0;

    out.deviationCount = 0;
    for (JsonObject deviation : departure["deviations"].as<JsonArray>())
    {
        if (out.deviationCount == MAX_DEVIATIONS_PER_DEPARTURE) break;
        const char *message = deviation["message"] | "";
        if (message[0] == '\0') continue;
        char *iso = out.deviations[out.deviationCount++];
        utf8ToIso88591(message, iso, sizeof(out.deviations[0]));
    }
}

void parseDepartures(JsonDocument &doc, const char *transportType, DepartureList &departures)
{
    departures.status = DepartureStatus::Ok;
    departures.httpCode = 0;
    departures.errorMessage[0] = '\0';
    departures.count = 0;

    for (JsonObject departure : doc["departures"].as<JsonArray>())
    {
        if (departures.count == MAX_DEPARTURES) break;

        // The request already asks for one transport mode, but some stops return others too
        const char *transportMode = departure["line"]["transport_mode"] | "";
        if (transportType == nullptr || strcmp(transportMode, transportType) != 0) continue;

        parseDeparture(departure, departures.departures[departures.count++]);
    }
}

void parseErrorMessage(const char *body, char *message, size_t messageSize)
{
    if (body == nullptr || body[0] == '\0')
    {
        snprintf(message, messageSize, "(Tomt svar)");
        return;
    }

    JsonDocument doc;
    if (!deserializeJson(doc, body))
    {
        for (const char *field : {"message", "error", "description"})
        {
            if (doc[field].is<const char *>())
            {
                utf8ToIso88591(doc[field].as<const char *>(), message, messageSize);
                return;
            }
        }
    }

    // Not a known JSON error format: show the start of the body as is
    char start[201];
    snprintf(start, sizeof(start), "%s", body);
    utf8ToIso88591(start, message, messageSize);
}
