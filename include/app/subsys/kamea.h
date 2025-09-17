/**
 * @file      kamea.h
 * @brief     Kamea sub-system APIs
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

#ifndef __KAMEA_H__
#define __KAMEA_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef CONFIG_KAMEA_CHANNEL_MQTT

#include <zephyr/net/mqtt.h>

/**
 * @brief Kamea MQTT callbacks
 */
typedef struct {
    void (*connected)(void);          /**< Invoked when Kamea client is connected to the server */
    void (*disconnected)(void);       /**< Invoked when Kamea client is disconnected of the server */
    void (*published)(uint16_t, int); /**< Invoked to inform of payload published result */
} kamea_mqtt_callbacks_t;

/**
 * @brief Initialize client
 * @param client_id Client ID
 * @param public_cert Device certificate
 * @param public_cert_len Length of device certificate
 * @param private_key Device private key
 * @param private_key_len Length of device private key
 * @param ca_cert Server CA certificate
 * @param ca_cert_len Length of server CA certificate
 * @param callbacks Kamea callbacks
 * @return 0 if the function succeeds, error code otherwise
 */
int kamea_mqtt_init(char                   *client_id,
                    uint8_t                *public_cert,
                    uint32_t                public_cert_len,
                    uint8_t                *private_key,
                    uint32_t                private_key_len,
                    uint8_t                *ca_cert,
                    uint32_t                ca_cert_len,
                    kamea_mqtt_callbacks_t *callbacks);

/**
 * @brief Open connection with the server
 * @return 0 if the function succeeds, error code otherwise
 */
int kamea_mqtt_connect(void);

/**
 * @brief Publish telemetry to the server
 * @param data Telemetry data
 * @param len Length of data
 * @param qos MQTT QOS
 * @return 0 if the function succeeds, error code otherwise
 */
int kamea_mqtt_publish_telemetry(uint8_t *data, uint32_t len, enum mqtt_qos qos);

/**
 * @brief Publish configs to the server
 * @param data Configs data
 * @param len Length of data
 * @param qos MQTT QOS
 * @return 0 if the function succeeds, error code otherwise
 */
int kamea_mqtt_publish_configs(uint8_t *data, uint32_t len, enum mqtt_qos qos);

/**
 * @brief Close connection with the server
 * @return 0 if the function succeeds, error code otherwise
 */
int kamea_mqtt_disconnect(void);

#endif /* CONFIG_KAMEA_CHANNEL_MQTT */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __KAMEA_H__ */
