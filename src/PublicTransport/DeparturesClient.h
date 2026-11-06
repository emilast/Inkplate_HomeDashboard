#pragma once

#include "DeparturesData.h"

// Fetches the departures of one transport type ("BUS", "TRAIN") from a stop. On
// failure, returns false with the status set to HttpError or ParseError.
bool fetchDepartures(DepartureList &departures, const char *siteId, const char *transportType);
