#pragma once

// Turns SL Transport API responses into DepartureList data. Has no Arduino or
// Inkplate dependencies, so it can be unit tested on the host (see test/).

#include <stddef.h>
#include <ArduinoJson.h>
#include "DeparturesData.h"

// Fills filter with the fields parseDepartures() reads, for use with
// DeserializationOption::Filter. Keeps the document small: a full response also
// carries stop, journey and line details the dashboard doesn't use.
void buildDeparturesFilter(JsonDocument &filter);

// Fills departures with the entries in doc that match transportType ("BUS",
// "TRAIN"), up to MAX_DEPARTURES, and sets the status to Ok.
void parseDepartures(JsonDocument &doc, const char *transportType, DepartureList &departures);

// Extracts a readable error message from an error response body (JSON with a
// "message", "error" or "description" field, or plain text) into message.
void parseErrorMessage(const char *body, char *message, size_t messageSize);
