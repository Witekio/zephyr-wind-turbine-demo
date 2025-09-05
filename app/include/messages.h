/**
 * @file      messages.h
 * @brief     Definition of zbus message types
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

#ifndef __MESSAGES_H__
#define __MESSAGES_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

/**
 * @brief Button status
 */
struct button_status_msg {
    char *name;  /**< Name */
    bool  state; /**< State */
};

/**
 * @brief Wind turbine status
 */
struct wind_turbine_status_msg {
    uint16_t wind_speed;     /**< Wind speed (km/h) */
    uint16_t generator_rpm;  /**< Generator speed (rpm) */
    uint16_t output_voltage; /**< Output voltage (volts) */
    uint16_t output_power;   /**< Output power (kilo-watts) */
};

/**
 * @brief Inverter status
 */
struct inverter_status_msg {
    uint16_t output_voltage; /**< Output voltage (volts) */
    uint16_t output_power;   /**< Output power (kilo-watts) */
    double   frequency;      /**< Network frequency (Hertz) */
};

/**
 * @brief Network status
 */
struct network_status_msg {
    bool connected;      /**< Connection status */
    char ip_address[32]; /**< IPv4 address, if connected is true */
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MESSAGES_H__ */
