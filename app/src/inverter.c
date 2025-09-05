/**
 * @file      inverter.c
 * @brief     Simulation of the inverter
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
LOG_MODULE_REGISTER(wind_turbine_inverter, LOG_LEVEL_INF);

#include <zephyr/zbus/zbus.h>

#include "messages.h"

/**
 * @brief Inverter initialization
 * @return 0 if the function succeeds, error code otherwise
 */
static int inverter_init(void);

/**
 * @brief Inverter status callback
 * @note This callback is used to simulate inverter parameters from the wind turbine status
 * @param chan Wind turbine status channel
 */
static void inverter_wind_turbine_status_callback(const struct zbus_channel *chan);

/**
 * @brief Inverter status channel
 */
ZBUS_CHAN_DEFINE(inverter_status_chan, struct inverter_status_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));

/**
 * @brief Zbus channels
 */
ZBUS_CHAN_DECLARE(wind_turbine_status_chan);

/**
 * @brief Zbus listener
 */
ZBUS_LISTENER_DEFINE(inverter_wind_turbine_status_listenner, inverter_wind_turbine_status_callback);

static int
inverter_init(void) {

    LOG_INF("Initializing inverter...");

    /* Register to Zbus channels */
    zbus_chan_add_obs(&wind_turbine_status_chan, &inverter_wind_turbine_status_listenner, K_MSEC(10));

    LOG_INF("Initializing inverter: DONE");

    return 0;
}

static void
inverter_wind_turbine_status_callback(const struct zbus_channel *chan) {

    struct inverter_status_msg            inverter_status_msg     = { 0 };
    const struct wind_turbine_status_msg *wind_turbine_status_msg = zbus_chan_const_msg(chan);
    static uint16_t                       previous_output_power   = 0;

    /* Simulate inverter status based on the wind turbine status (numbers are chosen to have a nice and coherent display on the demo) */
    inverter_status_msg.output_power = (99 * wind_turbine_status_msg->output_power) / 100;
    if (wind_turbine_status_msg->output_power > previous_output_power) {
        inverter_status_msg.output_voltage = 20050;
        inverter_status_msg.frequency      = 50.1;
    } else if (wind_turbine_status_msg->output_power < previous_output_power) {
        inverter_status_msg.output_voltage = 19950;
        inverter_status_msg.frequency      = 49.9;
    } else {
        inverter_status_msg.output_voltage = 20000;
        inverter_status_msg.frequency      = 50;
    }
    previous_output_power = wind_turbine_status_msg->output_power;

    /* Send inverter status */
    zbus_chan_pub(&inverter_status_chan, &inverter_status_msg, K_MSEC(10));
}

/**
 * @brief Initialization of inverter
 */
SYS_INIT(inverter_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
