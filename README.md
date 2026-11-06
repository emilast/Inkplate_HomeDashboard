# Inkplate_HomeDashboard

![Dashboard Preview](docs/images/dashboard.png)

This is a home dashboard for the [Inkplate 5 V2 and Inkplate 10 e-paper displays](https://www.inkplate.io/inkplate) by Soldered Electronics.
It is based on the [Inkplate-Arduino-library](https://github.com/SolderedElectronics/Inkplate-Arduino-library) and uses the following APIs:
- [SMHI Open Data](https://opendata.smhi.se/) Meteorological Forecasts (SNOW1gv1) for Swedish weather data
- The [Trafiklab SL Transport APIs](https://www.trafiklab.se/api/our-apis/sl/transport/) to get realtime departure times for public transport in Stockholm

## Highlights
- Displays current weather conditions, a temperature and precipitation graph for the next 24 hours, and a 3-day forecast
- Shows realtime departure times for public transport in Stockholm, including delays, cancellations and deviation messages
- Updates the traffic data every 20 seconds and the weather data every 200 seconds
- The temperature graph is drawn with graphic primitives
- Grayscale (3-bit) mode by default, with a full refresh on every update. In monochrome mode (`MONOCHROME 1` in `src/config.h`) it uses partial updates to reduce flickering, with a full refresh every 10th update to remove ghosting.
- Keeps running unattended: Wi-Fi is reconnected automatically (the clock and the last weather data stay on screen while offline), a task watchdog reboots the board if it hangs, and it restarts once a day at 04:00.

## Configuration
All user settings are in [`src/config.h`](src/config.h): the position for the weather forecast, the SL stops and their titles, the time zone, the display mode and the refresh timing.

On first start, or when the saved Wi-Fi network can't be reached, the board opens a temporary Wi-Fi network ("Inkplate10 Home Dashboard" or "Inkplate5 Home Dashboard"). Connect to it to choose the network to use.

## Building
The project is a [PlatformIO](https://platformio.org/) project with one debug and one release environment per board:

```
pio run -e release_inkplate10v2 -t upload
pio run -e release_inkplate5v2 -t upload
```

Unit tests for the text and time helpers run on the development machine:

```
pio test -e native
```

GitHub Actions builds both boards and runs the tests on every push.

## Overview
The code is structured in the following way:
- `src/` contains the main code for the dashboard
  - `main.ino` is the main entry point for the application
  - `config.h` holds the user settings
  - `Clock/` - a simple clock display
  - `Fonts/` contains the bitmap fonts used in the dashboard ([Acari Sans](https://github.com/StefanPeev/acari-sans) by Stefan Peev)
  - `PublicTransport/` contains the code to fetch and display public transport data from the Trafiklab SL Transport APIs for public transport in the Stockholm area in Sweden.
  - `Utils/` contains drawing helpers and the text and time helpers
  - `Weather/` contains the code to fetch and display weather data from SMHI, the Swedish Meteorological and Hydrological Institute.
- `test/` contains the unit tests

Source formatting is defined in `.clang-format`.
