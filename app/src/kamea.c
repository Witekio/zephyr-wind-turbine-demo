/**
 * @file      kamea.c
 * @brief     Communication with kamea server
 *
 * Copyright (C) Witekio
 *
 * This file is part of Zephyr Wind Turbine demonstration.
 *
 * This demonstration is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This demonstration is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with This demonstration. If not, see <http://www.gnu.org/licenses/>.
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wind_turbine_kamea, LOG_LEVEL_INF);

#include <zephyr/drivers/gpio.h>
#include <zephyr/zbus/zbus.h>

#include "app/subsys/kamea.h"
#include "messages.h"

/**
 * @brief Period to send telemetry data (in multiple of the wind turbine sampling, 100ms x 100 = 10s)
 */
#define KAMEA_REAL_TIME_DATA_PERIOD (100)

/**
 * @brief Kamea initialization
 * @return 0 if the function succeeds, error code otherwise
 */
static int kamea_init(void);

/**
 * @brief Kamea connected callback
 */
static void kamea_connected_cb(void);

/**
 * @brief Kamea disconnected callback
 */
static void kamea_disconnected_cb(void);

/**
 * @brief Kamea published callback
 * @param message_id Message ID
 * @param result 0 if the publish succeeds, error code otherwise
 */
static void kamea_published_cb(uint16_t message_id, int result);

/**
 * @brief Buttons status callback
 * @note This callback is used to send the button status to the kamea server
 * @param chan Buttons status channel
 */
static void kamea_buttons_status_cb(const struct zbus_channel *chan);

/**
 * @brief Wind turbine status callback
 * @note This callback is used to send the wind turbine status to the kamea server
 * @param chan Wind turbine status channel
 */
static void kamea_wind_turbine_status_cb(const struct zbus_channel *chan);

/**
 * @brief Inverter status callback
 * @note This callback is used to send the inverter status to the kamea server
 * @param chan Inverter status channel
 */
static void kamea_inverter_status_cb(const struct zbus_channel *chan);

/**
 * @brief Zbus channels
 */
ZBUS_CHAN_DECLARE(buttons_status_chan);
ZBUS_CHAN_DECLARE(wind_turbine_status_chan);
ZBUS_CHAN_DECLARE(inverter_status_chan);

/**
 * @brief Zbus listeners
 */
ZBUS_LISTENER_DEFINE(kamea_buttons_status_listenner, kamea_buttons_status_cb);
ZBUS_LISTENER_DEFINE(kamea_wind_turbine_status_listenner, kamea_wind_turbine_status_cb);
ZBUS_LISTENER_DEFINE(kamea_inverter_status_listenner, kamea_inverter_status_cb);

/**
 * @brief LED
 */
#define WIND_TURBINE_LED_NODE DT_ALIAS(wind_turbine_led)
#if !DT_NODE_HAS_STATUS_OKAY(WIND_TURBINE_LED_NODE)
#error "wind-turbine-led devicetree alias is not defined"
#endif
static struct gpio_dt_spec kamea_status_led = GPIO_DT_SPEC_GET_OR(WIND_TURBINE_LED_NODE, gpios, { 0 });

static int
kamea_init(void) {

    int result = -1;

    LOG_INF("Initializing Kamea client...");

#ifdef CONFIG_KAMEA_CHANNEL_MQTT
    /* FIXME: configuration should not be static, for example we can define this in files in the SD-Card */
    const char            *client_id = "wind_turbine_stm32f746g_disco";
    extern uint8_t         public_cert[];
    extern uint32_t        public_cert_len;
    extern uint8_t         private_key[];
    extern uint32_t        private_key_len;
    extern uint8_t         ca_cert[];
    extern uint32_t        ca_cert_len;
    kamea_mqtt_callbacks_t callbacks;
    callbacks.connected    = kamea_connected_cb;
    callbacks.disconnected = kamea_disconnected_cb;
    callbacks.published    = kamea_published_cb;
    /* Initialize Kamea MQTT channel */
    if (0 != (result = kamea_mqtt_init((char *)client_id, public_cert, public_cert_len, private_key, private_key_len, ca_cert, ca_cert_len, &callbacks))) {
        LOG_ERR("Unable to initialize Kamea MQTT channel, result = %d", result);
        goto END;
    }
#endif /* CONFIG_KAMEA_CHANNEL_MQTT */

    /* Initialize Kamea status LED */
    /* FIXME: could be better to move it to buttons.c and use Zbus to report Kamea status */
    if (!gpio_is_ready_dt(&kamea_status_led)) {
        LOG_ERR("Unable to configure LED '%s', device is not ready", kamea_status_led.port->name);
        result = -1;
        goto END;
    }
    if (0 != (result = gpio_pin_configure_dt(&kamea_status_led, GPIO_OUTPUT))) {
        LOG_ERR("Unable to configure LED '%s', unable to configure pin, result = %d", kamea_status_led.port->name, result);
        goto END;
    }
    gpio_pin_set_dt(&kamea_status_led, 1);

    /* Register to Zbus channels */
    zbus_chan_add_obs(&buttons_status_chan, &kamea_buttons_status_listenner, K_MSEC(10));
    zbus_chan_add_obs(&wind_turbine_status_chan, &kamea_wind_turbine_status_listenner, K_MSEC(10));
    zbus_chan_add_obs(&inverter_status_chan, &kamea_inverter_status_listenner, K_MSEC(10));

END:

    LOG_INF("Initializing Kamea client: DONE");

    return result;
}

static void
kamea_connected_cb(void) {

    /* Switch ON the LED */
    gpio_pin_set_dt(&kamea_status_led, 0);
}

static void
kamea_disconnected_cb(void) {

    /* Switch OFF the LED */
    gpio_pin_set_dt(&kamea_status_led, 1);
}

static void
kamea_published_cb(uint16_t message_id, int result) {

    ARG_UNUSED(message_id);
    ARG_UNUSED(result);

    /* Nothing to do for the moment */
}

static void
kamea_buttons_status_cb(const struct zbus_channel *chan) {

    char                            payload[128]; /* FIXME: should use json library */
    const struct button_status_msg *button_status_msg = zbus_chan_const_msg(chan);

    /* Format payload */
    snprintf(payload, sizeof(payload), "{ \"alert\": { \"name\": \"%s\", \"state\": %d } }", button_status_msg->name, button_status_msg->state);

    /* Publish payload */
#ifdef CONFIG_KAMEA_CHANNEL_MQTT
    kamea_mqtt_publish_telemetry(payload, strlen(payload), MQTT_QOS_1_AT_LEAST_ONCE);
#endif /* CONFIG_KAMEA_CHANNEL_MQTT */
}

static void
kamea_wind_turbine_status_cb(const struct zbus_channel *chan) {

    char                                  payload[128]; /* FIXME: should use json library */
    const struct wind_turbine_status_msg *wind_turbine_status_msg = zbus_chan_const_msg(chan);
    static uint16_t                       wind_speed[KAMEA_REAL_TIME_DATA_PERIOD];
    static uint16_t                       generator_rpm[KAMEA_REAL_TIME_DATA_PERIOD];
    static uint16_t                       output_voltage[KAMEA_REAL_TIME_DATA_PERIOD];
    static uint16_t                       output_power[KAMEA_REAL_TIME_DATA_PERIOD];
    static size_t                         count              = 0;
    uint32_t                              wind_speed_avg     = 0;
    uint32_t                              generator_rpm_avg  = 0;
    uint32_t                              output_voltage_avg = 0;
    uint32_t                              output_power_avg   = 0;
    size_t                                index;

    /* Save wind turbine data */
    wind_speed[count]     = wind_turbine_status_msg->wind_speed;
    generator_rpm[count]  = wind_turbine_status_msg->generator_rpm;
    output_voltage[count] = wind_turbine_status_msg->output_voltage;
    output_power[count]   = wind_turbine_status_msg->output_power;
    count++;

    /* Check if wind turbine data are ready to be sent */
    if (count < KAMEA_REAL_TIME_DATA_PERIOD) {
        return;
    }
    count = 0;

    /* Compute average data */
    for (index = 0; index < KAMEA_REAL_TIME_DATA_PERIOD; index++) {
        wind_speed_avg += wind_speed[index];
        generator_rpm_avg += generator_rpm[index];
        output_voltage_avg += output_voltage[index];
        output_power_avg += output_power[index];
    }
    wind_speed_avg /= KAMEA_REAL_TIME_DATA_PERIOD;
    generator_rpm_avg /= KAMEA_REAL_TIME_DATA_PERIOD;
    output_voltage_avg /= KAMEA_REAL_TIME_DATA_PERIOD;
    output_power_avg /= KAMEA_REAL_TIME_DATA_PERIOD;

    /* Format Wind Turbine payload */
    snprintf(payload, sizeof(payload), "{ \"wind_turbine\": { \"output_voltage\": %d, \"output_power\": %d } }", output_voltage_avg, output_power_avg);

    /* Publish payload */
#ifdef CONFIG_KAMEA_CHANNEL_MQTT
    kamea_mqtt_publish_telemetry(payload, strlen(payload), MQTT_QOS_1_AT_LEAST_ONCE);
#endif /* CONFIG_KAMEA_CHANNEL_MQTT */

    /* Format config payload */ /* FIXME: should be dynamic and depends on configuration given by the user, use static values for now */
    snprintf(payload, sizeof(payload), "{ \"turnedOn\": true, \"isProduction\": true, \"limiter\": 30 }");

    /* Publish payload */
#ifdef CONFIG_KAMEA_CHANNEL_MQTT
    kamea_mqtt_publish_configs(payload, strlen(payload), MQTT_QOS_1_AT_LEAST_ONCE);
#endif /* CONFIG_KAMEA_CHANNEL_MQTT */

    /* Format App payload */
    snprintf(
        payload, sizeof(payload), "{ \"energyProduction\": %d, \"generator\": %d, \"windSpeed\": %d }", output_power_avg, generator_rpm_avg, wind_speed_avg);

    /* Publish payload */
#ifdef CONFIG_KAMEA_CHANNEL_MQTT
    kamea_mqtt_publish_telemetry(payload, strlen(payload), MQTT_QOS_1_AT_LEAST_ONCE);
#endif /* CONFIG_KAMEA_CHANNEL_MQTT */
}

static void
kamea_inverter_status_cb(const struct zbus_channel *chan) {

    char                              payload[128]; /* FIXME: should use json library */
    const struct inverter_status_msg *inverter_status_msg = zbus_chan_const_msg(chan);
    static uint16_t                   output_voltage[KAMEA_REAL_TIME_DATA_PERIOD];
    static uint16_t                   output_power[KAMEA_REAL_TIME_DATA_PERIOD];
    static double                     frequency[KAMEA_REAL_TIME_DATA_PERIOD];
    static size_t                     count              = 0;
    uint32_t                          output_voltage_avg = 0;
    uint32_t                          output_power_avg   = 0;
    double                            frequency_avg      = 0;
    size_t                            index;

    /* Save inverter data */
    output_voltage[count] = inverter_status_msg->output_voltage;
    output_power[count]   = inverter_status_msg->output_power;
    frequency[count]      = inverter_status_msg->frequency;
    count++;

    /* Check if inverter data are ready to be sent */
    if (count < KAMEA_REAL_TIME_DATA_PERIOD) {
        return;
    }
    count = 0;

    /* Compute average data */
    for (index = 0; index < KAMEA_REAL_TIME_DATA_PERIOD; index++) {
        output_voltage_avg += output_voltage[index];
        output_power_avg += output_power[index];
        frequency_avg += frequency[index];
    }
    output_voltage_avg /= KAMEA_REAL_TIME_DATA_PERIOD;
    output_power_avg /= KAMEA_REAL_TIME_DATA_PERIOD;
    frequency_avg /= KAMEA_REAL_TIME_DATA_PERIOD;

    /* Format payload */
    snprintf(payload,
             sizeof(payload),
             "{ \"inverter\": { \"output_voltage\": %d, \"output_power\": %d, \"frequency\": %f } }",
             output_voltage_avg,
             output_power_avg,
             frequency_avg);

    /* Publish payload */
#ifdef CONFIG_KAMEA_CHANNEL_MQTT
    kamea_mqtt_publish_telemetry(payload, strlen(payload), MQTT_QOS_1_AT_LEAST_ONCE);
#endif /* CONFIG_KAMEA_CHANNEL_MQTT */
}

/**
 * @brief Initialization of kamea client
 */
SYS_INIT(kamea_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
