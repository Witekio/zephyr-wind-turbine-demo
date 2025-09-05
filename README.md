# README

The source code proposed in this repository is to demonstrate the usage of Zephyr using a nice setup with a wind turbine.
It demonstrates the usage of various modules from the Zephyr ecosystem: Wi-Fi connectivity, LVGL display, Zbus, PWM, ADC, mcuboot...
The device connects to Kamea from [TheEmbeddedKit](https://theembeddedkit.io/) to display the wind turbine status on a dashboard.

To start using Zephyr, we recommend that you begin with the Getting started section in [the Zephyr documentation](https://docs.zephyrproject.org/latest/develop/getting_started/index.html).
It is highly recommended to be familiar with Zephyr environment and tools to use this example.

## Wi-Fi configuration

The project makes use of the ESP32 with ESP-AT firwmare to provide a Wi-Fi interface.
Setup of the ESP32 is described in [ESP-AT Gett Started](https://docs.espressif.com/projects/esp-at/en/latest/esp32/Get_Started/index.html).
Version to be installed for compatibility with Zephyr driver is v2.1.0.0.
Wiring is achieved with Tx/Rx and Reset signals. CTS/RTS are not used.

Wi-Fi credentials are integrated at build time.

Local configuration file `local.conf` should be created in the `app` directory with the following content.

```
CONFIG_EXAMPLE_WIFI_SSID="<ssid>"
CONFIG_EXAMPLE_WIFI_PSK="<password>"
```

## Kamea Configuration and Credentials

Please refer to the README in the `app/creds` directory to generate and provision the device on Kamea platform.

Local configuration file `local.conf` should be completed in the `app` directory with the following content.

```
CONFIG_KAMEA_CHANNEL_MQTT_URL="<url of the kamea server>"
```

## Building

Use the following command to build the application.

```
west build -b stm32f746g_disco app --sysbuild -- -DEXTRA_CONF_FILE=local.conf
```

Use the following command to flash the board.

```
west flash
```
