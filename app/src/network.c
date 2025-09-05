/**
 * @file      network.c
 * @brief     Management of the network connection
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

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wind_turbine_network, LOG_LEVEL_INF);

#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/conn_mgr_connectivity_impl.h>
#ifdef CONFIG_WIFI
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/conn_mgr/connectivity_wifi_mgmt.h>
#endif /* CONFIG_WIFI */
#include <zephyr/zbus/zbus.h>

#include "messages.h"

/**
 * @brief Work queue stack size (Bytes)
 */
#define NETWORK_WORK_QUEUE_STACK_SIZE (2048)

/**
 * @brief Work queue priority
 */
#define NETWORK_WORK_QUEUE_PRIORITY (5)

/**
 * @brief Initialize network interface
 * @return 0 if the function succeeds, error code otherwise
 */
static int network_init(void);

/**
 * @brief Function used to handle work context timer when it expires
 * @param handle Timer handler
 */
static void network_timer_callback(struct k_timer *handle);

/**
 * @brief Function used to handle connect work
 * @param handle Work handler
 */
static void network_work_handle_handler(struct k_work *handle);

/**
 * @brief Connection manager event handler
 * @param mgmt_event Event type
 * @param iface Network interface
 * @param info A valid pointer on a data understood by the handler
 * @param info_length Length in bytes of the memory pointed by @p info
 * @param user_data User data
 */
static void network_l4_event_handler(uint64_t mgmt_event, struct net_if *iface, void *info, size_t info_length, void *user_data);

/**
 * @brief print DHCPv4 address information
 * @param iface Interface
 * @param if_addr Interface address
 * @param user_data user data (not used)
 */
static void network_print_dhcpv4_addr(struct net_if *iface, struct net_if_addr *if_addr, void *user_data);

/**
 * @brief Network status channel
 */
ZBUS_CHAN_DEFINE(network_status_chan, struct network_status_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));

/**
 * @brief Network connect work queue stack
 */
K_THREAD_STACK_DEFINE(network_work_queue_stack, NETWORK_WORK_QUEUE_STACK_SIZE);

/**
 * @brief Timer used to periodically schedule the reconnection of the network
 */
static struct k_timer network_timer_handle;

/**
 * @brief Work queue used to periodically schedule the reconnection of the network
 */
static struct k_work_q network_work_queue_handle;

/**
 * @brief Network connect work
 */
static struct k_work network_work_handle;

static int
network_init(void) {

    /* Create work and initialize timer to periodically schedule the reconnection of the network */
    k_work_queue_init(&network_work_queue_handle);
    k_work_queue_start(&network_work_queue_handle, network_work_queue_stack, NETWORK_WORK_QUEUE_STACK_SIZE, NETWORK_WORK_QUEUE_PRIORITY, NULL);
    k_thread_name_set(k_work_queue_thread_get(&network_work_queue_handle), "network_work_queue");
    k_work_init(&network_work_handle, network_work_handle_handler);
    k_timer_init(&network_timer_handle, network_timer_callback, NULL);

    /* Connect to the network */
    k_timer_start(&network_timer_handle, K_NO_WAIT, K_SECONDS(10));

    return 0;
}

static void
network_timer_callback(struct k_timer *handle) {

    ARG_UNUSED(handle);

    /* Submit the work to the work queue */
    if (k_work_submit_to_queue(&network_work_queue_handle, &network_work_handle) < 0) {
        LOG_ERR("Unable to submit work to the work queue");
    }
}

static void
network_work_handle_handler(struct k_work *handle) {

    ARG_UNUSED(handle);

#ifdef CONFIG_WIFI

    /* Set connection request parameters */
    struct wifi_connect_req_params params = { 0 };
    params.band                           = WIFI_FREQ_BAND_UNKNOWN;
    params.channel                        = WIFI_CHANNEL_ANY;
    params.mfp                            = WIFI_MFP_OPTIONAL;
    params.ssid                           = CONFIG_EXAMPLE_WIFI_SSID;
    params.ssid_length                    = strlen(params.ssid);
#if defined(CONFIG_EXAMPLE_WIFI_AUTHENTICATION_MODE_WPA2_PSK)
    params.security   = WIFI_SECURITY_TYPE_PSK;
    params.psk        = CONFIG_EXAMPLE_WIFI_PSK;
    params.psk_length = strlen(CONFIG_EXAMPLE_WIFI_PSK);
#elif defined(CONFIG_EXAMPLE_WIFI_AUTHENTICATION_MODE_WPA3_SAE)
    params.security            = WIFI_SECURITY_TYPE_SAE;
    params.sae_password        = CONFIG_EXAMPLE_WIFI_PSK;
    params.sae_password_length = strlen(CONFIG_EXAMPLE_WIFI_PSK);
#else
    params.security = WIFI_SECURITY_TYPE_NONE;
#endif
    /* Request connection to the network */
    int err;
    int tries = 0;
    while (tries < 10) {
        if (0 == (err = net_mgmt(NET_REQUEST_WIFI_CONNECT, net_if_get_default(), &params, sizeof(struct wifi_connect_req_params)))) {
            break;
        }
        k_msleep(500);
        tries++;
    }
    if (-EINPROGRESS == err) {
        LOG_ERR("Reconnect already in progress");
    } else if (err < 0) {
        LOG_ERR("Reconnect request failed: %d", err);
    } else {
        LOG_INF("Reconnect request accepted");
    }

#endif /* CONFIG_WIFI */
}

static void
network_print_dhcpv4_addr(struct net_if *iface, struct net_if_addr *if_addr, void *user_data) {

    ARG_UNUSED(user_data);
    char                      hr_addr[NET_IPV4_ADDR_LEN];
    struct in_addr            netmask;
    struct network_status_msg network_status_msg = { 0 };

    /* Check address type */
    if (NET_ADDR_DHCP != if_addr->addr_type) {
        return;
    }

    /* Print network information */
    LOG_INF("IPv4 address: %s", net_addr_ntop(AF_INET, &if_addr->address.in_addr, hr_addr, NET_IPV4_ADDR_LEN));
    LOG_INF("Lease time: %u seconds", iface->config.dhcpv4.lease_time);
    netmask = net_if_ipv4_get_netmask_by_addr(iface, &if_addr->address.in_addr);
    LOG_INF("Subnet: %s", net_addr_ntop(AF_INET, &netmask, hr_addr, NET_IPV4_ADDR_LEN));
    LOG_INF("Router: %s", net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, hr_addr, NET_IPV4_ADDR_LEN));

    /* Send network status */
    network_status_msg.connected = true;
    strncpy(
        network_status_msg.ip_address, net_addr_ntop(AF_INET, &if_addr->address.in_addr, hr_addr, NET_IPV4_ADDR_LEN), sizeof(network_status_msg.ip_address));
    zbus_chan_pub(&network_status_chan, &network_status_msg, K_MSEC(10));
}

static void
network_l4_event_handler(uint64_t mgmt_event, struct net_if *iface, void *info, size_t info_length, void *user_data) {

    ARG_UNUSED(iface);
    ARG_UNUSED(info);
    ARG_UNUSED(info_length);
    ARG_UNUSED(user_data);
    struct network_status_msg network_status_msg = { 0 };

    if (NET_EVENT_L4_CONNECTED == mgmt_event) {
        /* Indicate the network is available */
        LOG_INF("Network is connected");
        /* Stop periodic connection request to reconnect the interface */
        k_timer_stop(&network_timer_handle);
        /* Print interface information */
        net_if_ipv4_addr_foreach(iface, network_print_dhcpv4_addr, NULL);
    } else if (NET_EVENT_L4_DISCONNECTED == mgmt_event) {
        LOG_WRN("Network is disconnected");
        /* Start periodic connection request to reconnect the interface */
        k_timer_start(&network_timer_handle, K_NO_WAIT, K_SECONDS(10));
        /* Send network status */
        network_status_msg.connected = false;
        zbus_chan_pub(&network_status_chan, &network_status_msg, K_MSEC(10));
    }
}

/**
 * Register connection manager handler
 */
NET_MGMT_REGISTER_EVENT_HANDLER(network_init_event_handler, NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED, &network_l4_event_handler, NULL);

/**
 * @brief Initialization of network
 */
SYS_INIT(network_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
