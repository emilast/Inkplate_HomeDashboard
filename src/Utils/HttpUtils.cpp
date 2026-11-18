#include "HttpUtils.h"

// The Arduino core defaults allow a TLS handshake to take 120 s, longer than the
// task watchdog (WDT_TIMEOUT_SECONDS). A stalled connection should fail the
// request, not reboot the board.
static constexpr int connectTimeoutMs = 5000;
static constexpr int handshakeTimeoutSeconds = 10;
static constexpr int readTimeoutMs = 10000;

bool beginHttps(HTTPClient &http, WiFiClientSecure &client, const char *url)
{
    // Same as HTTPClient::begin(url) without a CA certificate: no server verification
    client.setInsecure();
    client.setHandshakeTimeout(handshakeTimeoutSeconds);
    http.setConnectTimeout(connectTimeoutMs);
    http.setTimeout(readTimeoutMs);
    return http.begin(client, url);
}
