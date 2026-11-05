/*
   Inkplate10_Grayscale example for Soldered Inkplate 10
   For this example you will need only USB cable and Inkplate 10
   Select "e-radionica Inkplate10" or "Soldered Inkplate10" from Tools -> Board menu.
   Don't have "e-radionica Inkplate10" or "Soldered Inkplate10" option? Follow our tutorial and add it:
   https://soldered.com/learn/add-inkplate-6-board-definition-to-arduino-ide/

   This example will show you how you can draw some simple graphics using
   Adafruit GFX functions. Yes, Inkplate library is 100% compatible with GFX lib!
   Learn more about Adafruit GFX: https://learn.adafruit.com/adafruit-gfx-graphics-library
   Inkplate will be used in gray mode which is 3 bit, so you can have up to 8 different colors (black, 6 gray
   colors and white) Color is represented by number, where number 0 means black and number 7 means white, while
   everything in between are shades of gray.

   Want to learn more about Inkplate? Visit www.inkplate.io
   Looking to get support? Write on our forums: https://forum.soldered.com/
   2 December 2022 by Soldered
*/

#include "Inkplate.h" //Include Inkplate library to the sketch

#include "HTTPClient.h" //Include library for HTTPClient
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "constants.h"

#if MONOCHROME // Defined in config.h
  #define DISPLAY_MODE INKPLATE_1BIT
#else
  #define DISPLAY_MODE INKPLATE_3BIT
#endif

Inkplate display(DISPLAY_MODE); // Create object on Inkplate library and set library to work in gray mode (3-bit)
                                 // Other option is BW mode, which is demonstrated in next example
                                 // "Inkplate_basic_BW"

#include "Utils/utils.h"
#include "Utils/DisplayManager.h"

#include "Clock/TimeUtils.h"
#include "Weather/WeatherDisplay.h"
#include "PublicTransport/DeparturesDisplay.h"



#if defined(ARDUINO_INKPLATE10) || defined(ARDUINO_INKPLATE10V2)
const char *networkName = "Inkplate10 Home Dashboard";

#include "Fonts/AcariSans12.h"
#include "Fonts/AcariSans16.h"
#include "Fonts/AcariSansbd16.h"
#include "Fonts/AcariSansbd50.h"

static const FontCollection fonts = {
    AcariSans_Regular16pt8b,
    AcariSans_Bold16pt8b,
    AcariSans_Bold50pt7b,
    AcariSans_Medium12pt8b
};

#elif defined(ARDUINO_INKPLATE5) || defined(ARDUINO_INKPLATE5V2)
const char *networkName = "Inkplate5 Home Dashboard";

#include "Fonts/AcariSansbd12.h"
#include "Fonts/AcariSans20.h"
#include "Fonts/AcariSansbd20.h"
#include "Fonts/AcariSansbd50.h"

static const FontCollection fonts = {
    AcariSans_Regular20pt8b,
    AcariSans_Bold20pt8b,
    AcariSans_Bold50pt7b,
    AcariSans_Bold12pt8b
};

#else
#error "Wrong board selection!"
#endif

// Trafiklab transport modes
const char *TrainType = "TRAIN";
const char *BusType = "BUS";

// Layout constants
const int leftXpos = 35;   // Left column start position
const int rightXpos = 430; // right column start position

const int row_height = 50;

const int busLineWidth = 80;
const int trainLineWidth = 60;

int framesDrawn = 0;
bool weatherFetchMissed = false;

void setup()
{
  Serial.begin(115200);

  // display.changeWaveform(INKPLATE10_WAVEFORM1);

  display.begin();        // Init library (you should call this function ONLY ONCE)
  display.clearDisplay(); // Clear any data that may have been in (software) frame buffer.
                          //(NOTE! This does not clean image on screen, it only clears it in the frame buffer inside
                          // ESP32).

  printStartupMessage(display, fonts, 100, 200);

  display.display();      // Clear everything that has previously been on a screen

  // Connect to WiFi
  WiFiManager wm;

  // Ensure the WiFi interface starts cleanly on first boot
  WiFi.mode(WIFI_OFF);
  delay(1000);

  wm.setCleanConnect(true);
  wm.setConnectTimeout(30); // seconds to try reconnecting
  wm.setConnectRetries(5);  // retries before giving up
  wm.setConfigPortalTimeout(300); // give up on the setup portal after 5 minutes and retry from loop()

  bool res = wm.autoConnect(networkName);
  if (!res)
  {
    Serial.println("Failed to connect");
  }
  else
  {
    Serial.println("Connected to WiFi.");
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
  }

  // Start the Task Watchdog only after WiFiManager is done: autoConnect() and the
  // configuration portal can block for minutes without feeding the watchdog.
  esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
  esp_task_wdt_add(NULL);

  // Get current time from time server
  configTime(0, 0, NTP_SERVER);

  // Set the local time zone (see TIME_ZONE in config.h)
  setenv("TZ", TIME_ZONE, 1);
  tzset();
}

void loop()
{
  esp_task_wdt_reset();

  // Active Wi-Fi check & automatic reconnect
  bool online = true;
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi disconnected! Attempting reconnection...");
    WiFi.reconnect();
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000)
    {
      delay(500);
      esp_task_wdt_reset();
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED)
    {
      // Keep the clock and the last weather data on screen while offline
      Serial.println("WiFi reconnection failed. Skipping network operations this cycle.");
      online = false;
    }
    else
    {
      Serial.println("WiFi reconnected successfully.");
    }
  }

  // Scheduled daily restart (04:00 by default) to eliminate long-term heap fragmentation.
  // The uptime check keeps the board from restarting again when it comes back up
  // while the clock still shows the restart time.
  struct tm timeinfo;
  if (millis() > MIN_UPTIME_BEFORE_RESTART_MS && getLocalTime(&timeinfo))
  {
    if (timeinfo.tm_hour == DAILY_RESTART_HOUR && timeinfo.tm_min == 0)
    {
      Serial.println("Performing scheduled daily maintenance restart...");
      delay(1000);
      ESP.restart();
    }
  }

  display.clearDisplay();        // Clear everytning that is inside frame buffer in ESP32
  printLocalTime(display, fonts, leftXpos, 100);

  // Fetch weather every FULL_REFRESH_EVERY_N_UPDATES updates, and as soon as Wi-Fi
  // is back if a scheduled fetch was skipped while offline.
  bool fetchWeather = framesDrawn == 0 || weatherFetchMissed;
  weatherFetchMissed = fetchWeather && !online;
  drawWeather(display, fonts, leftXpos, 180, row_height, online && fetchWeather);

  if (online)
  {
    Serial.println("Bus information from API...");
    if (!drawDepartures(display, fonts, BUS_SITE_ID, BusType, BUS_TITLE, rightXpos, 65, busLineWidth, true, row_height))
    {
      drawDepartures(display, fonts, BUS_SITE_ID, BusType, BUS_TITLE, rightXpos, 65, busLineWidth, true, row_height);
    }

    Serial.println("Train information from API...");
    if (!drawDepartures(display, fonts, TRAIN_SITE_ID, TrainType, TRAIN_TITLE, rightXpos, 385, trainLineWidth, false, row_height))
    {
      drawDepartures(display, fonts, TRAIN_SITE_ID, TrainType, TRAIN_TITLE, rightXpos, 385, trainLineWidth, false, row_height);
    }
  }
  else
  {
    drawOfflineMessage(display, fonts, BUS_TITLE, rightXpos, 65, row_height);
    drawOfflineMessage(display, fonts, TRAIN_TITLE, rightXpos, 385, row_height);
  }

  esp_task_wdt_reset();

#if MONOCHROME
  // For monochrome mode, always use partial display update to reduce flickering
  if (framesDrawn == 0)
  {
    display.display();
  }
  else
  {
    display.partialUpdate();
  }
#else
  // For grayscale mode, use normal display update since partial mode is not yet supported i grayscale
  display.display();
#endif

  framesDrawn = (framesDrawn + 1) % FULL_REFRESH_EVERY_N_UPDATES;

  Serial.printf("Waiting for next update in %d seconds...\n", REFRESH_INTERVAL_MS / 1000);
  esp_task_wdt_reset();
  delay(REFRESH_INTERVAL_MS);
  esp_task_wdt_reset();
}

void printStartupMessage(Inkplate &display, const FontCollection &fonts, int xpos, int ypos)
{
  display.setTextColor(_BLACK, _WHITE);

  display.setCursor(xpos, ypos);
  display.setFont(&fonts.boldTextFont);
  display.println("Ansluter till WiFi...");
  ypos += row_height;

  display.setCursor(xpos, ypos);
  display.setFont(&fonts.normalTextFont);
  int wrappedLines = drawWrappedText(display, fonts.normalTextFont, "Vid problem, st\xe4ll in WiFi via det tillf\xe4lliga n\xe4tverket:", xpos, ypos, E_INK_WIDTH - xpos - 25);
  ypos += row_height * wrappedLines;

  display.setCursor(xpos, ypos);
  display.setFont(&fonts.boldTextFont);
  display.println(networkName);
}