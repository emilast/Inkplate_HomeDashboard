#pragma once

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Starts an HTTPS request with timeouts short enough to stay well within the task
// watchdog. The client must outlive the request, so the caller owns it.
bool beginHttps(HTTPClient &http, WiFiClientSecure &client, const char *url);
