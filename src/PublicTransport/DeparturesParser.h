#pragma once

// Turns SL Transport API responses into DepartureList data. Has no Arduino or
// Inkplate dependencies, so it can be unit tested on the host (see test/).

#include <stddef.h>
#include <ArduinoJson.h>
#include "DeparturesData.h"

// Fills departures with the entries in doc that match transportType ("BUS",
// "TRAIN"), up to MAX_DEPARTURES, and sets the status to Ok.
void parseDepartures(JsonDocument &doc, const char *transportType, DepartureList &departures);

// Extracts a readable error message from an error response body (JSON with a
// "message", "error" or "description" field, or plain text) into message.
void parseErrorMessage(const char *body, char *message, size_t messageSize);
