/**
 * @file      kamea_mqtt.c
 * @brief     Kamea MQTT channel implementation
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

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(kamea_mqtt, CONFIG_KAMEA_LOG_LEVEL);

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/random/random.h>

#include "app/subsys/kamea.h"

/**
 * @brief Kamea MMQTT client thread stack size (bytes)
 */
#define KAMEA_MQTT_THREAD_STACK_SIZE (8192)

/**
 * @brief Kamea MQTT tread priority
 */
#define KAMEA_MQTT_THREAD_PRIORITY (10)

/**
 * @brief MQTT client instance
 */
static struct mqtt_client kamea_mqtt_client;

/**
 * @brief MQTT client ID
 */
static char kamea_client_id[32];

/**
 * @brief MQTT client buffers
 */
static uint8_t kamea_mqtt_rx_buffer[CONFIG_KAMEA_MQTT_RX_BUFFER_SIZE];
static uint8_t kamea_mqtt_tx_buffer[CONFIG_KAMEA_MQTT_TX_BUFFER_SIZE];

/**
 * @brief MQTT broker configuration
 */
static struct sockaddr_storage kamea_mqtt_broker;

/**
 * @brief MQTT connected flag
 */
volatile static bool kamea_mqtt_connected = false;

/**
 * @brief Kamea MQTT callbacks
 */
static kamea_mqtt_callbacks_t kamea_callbacks;

/**
 * @brief Thread used to connect and handle data with Kamea server
 */
static void kamea_mqtt_thread(void);

/**
 * @brief MQTT event handler
 * @param client MQTT client instance
 * @param evt MQTT event
 */
static void kamea_mqtt_event_handler(struct mqtt_client *const client, const struct mqtt_evt *evt);

#ifdef CONFIG_KAMEA_USE_CONNECTION_MANAGER

/**
 * @brief Network status
 */
volatile static bool kamea_mqtt_network_connected = false;

/**
 * @brief Connection manager event handler
 * @param mgmt_event Event type
 * @param iface Network interface
 * @param info A valid pointer on a data understood by the handler
 * @param info_length Length in bytes of the memory pointed by @p info
 * @param user_data User data
 */
static void kamea_mqtt_l4_event_handler(uint64_t mgmt_event, struct net_if *iface, void *info, size_t info_length, void *user_data);

#endif /* CONFIG_KAMEA_USE_CONNECTION_MANAGER */

int
kamea_mqtt_init(char                   *client_id,
                uint8_t                *public_cert,
                uint32_t                public_cert_len,
                uint8_t                *private_key,
                uint32_t                private_key_len,
                uint8_t                *ca_cert,
                uint32_t                ca_cert_len,
                kamea_mqtt_callbacks_t *callbacks) {

    assert(NULL != client_id);
    assert(NULL != public_cert);
    assert(NULL != private_key);
    assert(NULL != ca_cert);
    assert(NULL != callbacks);
    int result;

    /* Copy client ID */
    strncpy(kamea_client_id, client_id, sizeof(kamea_client_id));
    kamea_client_id[sizeof(kamea_client_id) - 1] = '\0';

    /* Register device certificate */
    if (0
        != (result = tls_credential_add(
                CONFIG_KAMEA_TLS_CREDENTIAL_DEVICE_KEY_AND_CERTIFICATE_TAG, TLS_CREDENTIAL_PUBLIC_CERTIFICATE, public_cert, public_cert_len))) {
        LOG_ERR("Unable to register device certificate, result = %d", result);
        goto END;
    }

    /* Register device private key */
    if (0
        != (result
            = tls_credential_add(CONFIG_KAMEA_TLS_CREDENTIAL_DEVICE_KEY_AND_CERTIFICATE_TAG, TLS_CREDENTIAL_PRIVATE_KEY, private_key, private_key_len))) {
        LOG_ERR("Unable to register device private key, result = %d", result);
        goto END;
    }

    /* Register server CA certificate */
    if (0 != (result = tls_credential_add(CONFIG_KAMEA_TLS_CREDENTIAL_SERVER_CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE, ca_cert, ca_cert_len))) {
        LOG_ERR("Unable to register server CA certificate, result = %d", result);
        goto END;
    }

    /* Save callbacks */
    memcpy(&kamea_callbacks, callbacks, sizeof(kamea_mqtt_callbacks_t));

END:

    return result;
}

int
kamea_mqtt_connect(void) {

    /* FIXME: to be completed to handle the case without CONFIG_KAMEA_USE_CONNECTION_MANAGER */
    return -1;
}

int
kamea_mqtt_publish_telemetry(uint8_t *data, uint32_t len, enum mqtt_qos qos) {

    int                       result;
    struct mqtt_publish_param param;
    char                      topic[64];

    /* Check if client is connected */
    if (false == kamea_mqtt_connected) {
        LOG_DBG("Unable to publish data, client is not connected");
        return -1;
    }

    /* Set publish param */
    param.message.topic.qos = qos;
    snprintf(topic, sizeof(topic), "device/%s/telemetries", kamea_client_id);
    param.message.topic.topic.utf8 = (uint8_t *)topic;
    param.message.topic.topic.size = strlen(topic);
    param.message.payload.data     = data;
    param.message.payload.len      = len;
    param.message_id               = sys_rand16_get();
    param.dup_flag                 = 0U;
    param.retain_flag              = 0U;

    /* Publish data */
    /* FIXME: the message should be passed to a queue handled by a separated thread */
    /* FIXME: actually it blocks the calling function and cause issue if the calling function is an interrupt */
    if (0 != (result = mqtt_publish(&kamea_mqtt_client, &param))) {
        LOG_ERR("Unable to publish data, result = %d, errno = %d", result, errno);
        goto END;
    }

END:

    /* Invoked published callback */
    if (NULL != kamea_callbacks.published) {
        kamea_callbacks.published(param.message_id, result);
    }

    return result;
}

int
kamea_mqtt_disconnect(void) {

    /* FIXME: to be completed to handle the case without CONFIG_KAMEA_USE_CONNECTION_MANAGER */
    return -1;
}

static void
kamea_mqtt_thread(void) {

    int                    result;
    struct zsock_addrinfo  hints;
    struct zsock_addrinfo *addr = NULL;
    char                   port[6];
    struct pollfd          fds[1];

    /* Wait until the network is connected */
    while (false == kamea_mqtt_network_connected) {
        /* Wait before trying again */
        k_sleep(K_SECONDS(CONFIG_KAMEA_MQTT_RECONNECT_INTERVAL));
    }
    LOG_INF("Trying to resolve Kamea MQTT broker address...");

    /* Set hints */
    memset(&hints, 0, sizeof(hints));
    if (IS_ENABLED(CONFIG_NET_IPV6)) {
        hints.ai_family = AF_INET6;
    } else if (IS_ENABLED(CONFIG_NET_IPV4)) {
        hints.ai_family = AF_INET;
    }
    hints.ai_socktype = SOCK_STREAM;

    /* Perform DNS resolution of the host */
    snprintf(port, sizeof(port), "%d", CONFIG_KAMEA_CHANNEL_MQTT_PORT);
    while (0 != (result = zsock_getaddrinfo(CONFIG_KAMEA_CHANNEL_MQTT_URL, port, &hints, &addr))) {
        LOG_ERR("Unable to resolve host name '%s:%d', result = %d, errno = %d", CONFIG_KAMEA_CHANNEL_MQTT_URL, CONFIG_KAMEA_CHANNEL_MQTT_PORT, result, errno);
        /* Wait before trying again */
        k_sleep(K_SECONDS(CONFIG_KAMEA_MQTT_RECONNECT_INTERVAL));
    }
    LOG_INF("Resolved Kamea MQTT broker address");

    /* MQTT broker configuration */
    if (IS_ENABLED(CONFIG_NET_IPV6)) {
        struct sockaddr_in6 *broker6 = (struct sockaddr_in6 *)&kamea_mqtt_broker;
        broker6->sin6_family         = AF_INET6;
        broker6->sin6_port           = htons(CONFIG_KAMEA_CHANNEL_MQTT_PORT);
        net_ipaddr_copy(&broker6->sin6_addr, &net_sin6(addr->ai_addr)->sin6_addr);
    } else if (IS_ENABLED(CONFIG_NET_IPV4)) {
        struct sockaddr_in *broker4 = (struct sockaddr_in *)&kamea_mqtt_broker;
        broker4->sin_family         = AF_INET;
        broker4->sin_port           = htons(CONFIG_KAMEA_CHANNEL_MQTT_PORT);
        net_ipaddr_copy(&broker4->sin_addr, &net_sin(addr->ai_addr)->sin_addr);
    }

    /* Release memory */
    zsock_freeaddrinfo(addr);

    /* Infinite loop */
    while (1) {

        /* Wait until the network is connected */
        while (false == kamea_mqtt_network_connected) {
            /* Wait before trying again */
            k_sleep(K_SECONDS(CONFIG_KAMEA_MQTT_RECONNECT_INTERVAL));
        }
        LOG_INF("Initializing Kamea MQTT client...");

        /* Initialize MQTT client */
        mqtt_client_init(&kamea_mqtt_client);

        /* MQTT client configuration */
        kamea_mqtt_client.broker           = &kamea_mqtt_broker;
        kamea_mqtt_client.evt_cb           = kamea_mqtt_event_handler;
        kamea_mqtt_client.client_id.utf8   = (uint8_t *)kamea_client_id;
        kamea_mqtt_client.client_id.size   = strlen(kamea_client_id);
        kamea_mqtt_client.protocol_version = MQTT_VERSION_3_1_1;

        /* MQTT buffers configuration */
        kamea_mqtt_client.rx_buf      = kamea_mqtt_rx_buffer;
        kamea_mqtt_client.rx_buf_size = CONFIG_KAMEA_MQTT_RX_BUFFER_SIZE;
        kamea_mqtt_client.tx_buf      = kamea_mqtt_tx_buffer;
        kamea_mqtt_client.tx_buf_size = CONFIG_KAMEA_MQTT_TX_BUFFER_SIZE;

        /* Username and password */
        kamea_mqtt_client.password  = NULL;
        kamea_mqtt_client.user_name = NULL;

        /* MQTT transport configuration */
        kamea_mqtt_client.transport.type = MQTT_TRANSPORT_SECURE;

        /* MQTT TLS configuration */
        kamea_mqtt_client.transport.tls.config.peer_verify = TLS_PEER_VERIFY_REQUIRED;
        kamea_mqtt_client.transport.tls.config.cipher_list = NULL;
        static const sec_tag_t sec_tag_list[]
            = { CONFIG_KAMEA_TLS_CREDENTIAL_DEVICE_KEY_AND_CERTIFICATE_TAG, CONFIG_KAMEA_TLS_CREDENTIAL_SERVER_CA_CERTIFICATE_TAG };
        kamea_mqtt_client.transport.tls.config.sec_tag_list  = sec_tag_list;
        kamea_mqtt_client.transport.tls.config.sec_tag_count = ARRAY_SIZE(sec_tag_list);
        kamea_mqtt_client.transport.tls.config.hostname      = CONFIG_KAMEA_CHANNEL_MQTT_URL;

        /* Connect to MQTT broker */
        if (0 != (result = mqtt_connect(&kamea_mqtt_client))) {
            LOG_ERR("Unable to connect to the MQTT broker '%s:%d', result = %d (%s), errno = %d",
                    CONFIG_KAMEA_CHANNEL_MQTT_URL,
                    CONFIG_KAMEA_CHANNEL_MQTT_PORT,
                    result,
                    zsock_gai_strerror(result),
                    errno);
            goto END;
        }
        if (NULL != kamea_callbacks.connected) {
            kamea_callbacks.connected();
        }
        LOG_INF("Kamea client connected to MQTT broker");

        /* Prepare MQTT file descriptor */
        if (MQTT_TRANSPORT_SECURE == kamea_mqtt_client.transport.type) {
            fds[0].fd = kamea_mqtt_client.transport.tls.sock;
        }
        fds[0].events = POLLIN;

        /* Poll file descriptor */
        if ((result = poll(fds, 1, 10000)) < 0) {
            goto END;
        }
        mqtt_input(&kamea_mqtt_client);

        /* Check if connection is established */
        if (false == kamea_mqtt_connected) {
            goto END;
        }

        /* Loop while connection is established */
        while ((true == kamea_mqtt_network_connected) && (true == kamea_mqtt_connected)) {
            if ((result = poll(fds, 1, SYS_FOREVER_MS)) < 0) {
                goto END;
            }
            mqtt_input(&kamea_mqtt_client);
        }

    END:
        /* Abort connection */
        mqtt_abort(&kamea_mqtt_client);

        /* Client disconnected */
        if (NULL != kamea_callbacks.disconnected) {
            kamea_callbacks.disconnected();
        }
        LOG_ERR("Kamea client disconnected, waiting before trying to connect again to the broker");

        /* Wait before trying again */
        k_sleep(K_SECONDS(CONFIG_KAMEA_MQTT_RECONNECT_INTERVAL));
    }
}

static void
kamea_mqtt_event_handler(struct mqtt_client *const client, const struct mqtt_evt *evt) {

    uint8_t                  data[33]; /* FIXME: buffer size to be checked */
    int                      len, bytes_read;
    struct mqtt_puback_param puback;

    /* Treatment depending of the event */
    switch (evt->type) {
        case MQTT_EVT_SUBACK:
            LOG_INF("SUBACK packet id: %u", evt->param.suback.message_id);
            break;
        case MQTT_EVT_UNSUBACK:
            LOG_INF("UNSUBACK packet id: %u", evt->param.suback.message_id);
            break;
        case MQTT_EVT_CONNACK:
            if (evt->result) {
                LOG_ERR("MQTT connect failed %d", evt->result);
                break;
            }
            kamea_mqtt_connected = true;
            LOG_DBG("MQTT client connected!");
            break;
        case MQTT_EVT_DISCONNECT:
            LOG_DBG("MQTT client disconnected %d", evt->result);
            kamea_mqtt_connected = false;
            break;
        case MQTT_EVT_PUBACK:
            if (evt->result) {
                LOG_ERR("MQTT PUBACK error %d", evt->result);
                break;
            }
            LOG_DBG("PUBACK packet id: %u\n", evt->param.puback.message_id);
            break;
        case MQTT_EVT_PUBLISH:
            len = evt->param.publish.message.payload.len;
            LOG_INF("MQTT publish received %d, %d bytes", evt->result, len);
            LOG_INF(" id: %d, qos: %d", evt->param.publish.message_id, evt->param.publish.message.topic.qos);
            while (len) {
                bytes_read = mqtt_read_publish_payload(client, data, (len >= sizeof(data) - 1) ? (sizeof(data) - 1) : len);
                if (bytes_read < 0 && bytes_read != -EAGAIN) {
                    LOG_ERR("failure to read payload");
                    break;
                }
                data[bytes_read] = '\0';
                LOG_INF("   payload: %s", data);
                len -= bytes_read;
            }
            puback.message_id = evt->param.publish.message_id;
            mqtt_publish_qos1_ack(client, &puback);
            break;
        default:
            LOG_DBG("Unhandled MQTT event %d", evt->type);
            break;
    }
}

#ifdef CONFIG_KAMEA_USE_CONNECTION_MANAGER

static void
kamea_mqtt_l4_event_handler(uint64_t mgmt_event, struct net_if *iface, void *info, size_t info_length, void *user_data) {

    ARG_UNUSED(iface);
    ARG_UNUSED(info);
    ARG_UNUSED(info_length);
    ARG_UNUSED(user_data);

    if (NET_EVENT_L4_CONNECTED == mgmt_event) {
        /* Indicate the network is available */
        LOG_INF("Network is connected");
        kamea_mqtt_network_connected = true;
    } else if (NET_EVENT_L4_DISCONNECTED == mgmt_event) {
        LOG_WRN("Network is disconnected");
        kamea_mqtt_network_connected = false;
    }
}

/**
 * Register connection manager handler
 */
NET_MGMT_REGISTER_EVENT_HANDLER(kamea_mqtt_init_event_handler, NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED, &kamea_mqtt_l4_event_handler, NULL);

#endif /* CONFIG_KAMEA_USE_CONNECTION_MANAGER */

/**
 * @brief Create Kamea MQTT client thread
 */
K_THREAD_DEFINE(kamea_mqtt_thread_id, KAMEA_MQTT_THREAD_STACK_SIZE, kamea_mqtt_thread, NULL, NULL, NULL, KAMEA_MQTT_THREAD_PRIORITY, 0, 0);
