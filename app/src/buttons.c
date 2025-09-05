/**
 * @file      buttons.c
 * @brief     Handling of the wind turbine buttons
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
LOG_MODULE_REGISTER(wind_turbine_buttons, LOG_LEVEL_INF);

#include <zephyr/drivers/gpio.h>
#include <zephyr/zbus/zbus.h>

#include "messages.h"

/**
 * @brief Work queue stack size (Bytes)
 */
#define BUTTONS_WORK_QUEUE_STACK_SIZE (2048)

/**
 * @brief Work queue priority
 */
#define BUTTONS_WORK_QUEUE_PRIORITY (5)

/**
 * @brief Buttons initialization
 * @return 0 if the function succeeds, error code otherwise
 */
static int buttons_init(void);

/**
 * @brief Top button pressed callback
 * @param port Device struct for the GPIO device
 * @param cb Original struct gpio_callback owning this handler
 * @param pins Mask of pins that triggers the callback handler
 */
static void buttons_top_button_pressed_cb(const struct device *port, struct gpio_callback *cb, uint32_t pins);

/**
 * @brief Bottom button pressed callback
 * @param port Device struct for the GPIO device
 * @param cb Original struct gpio_callback owning this handler
 * @param pins Mask of pins that triggers the callback handler
 */
static void buttons_bottom_button_pressed_cb(const struct device *port, struct gpio_callback *cb, uint32_t pins);

/**
 * @brief Function used to handle top button work
 * @param handle Work handler
 */
static void buttons_top_button_work_handler(struct k_work *handle);

/**
 * @brief Function used to handle bottom button work
 * @param handle Work handler
 */
static void buttons_bottom_button_work_handler(struct k_work *handle);

/**
 * @brief Buttons work queue stack
 */
K_THREAD_STACK_DEFINE(buttons_work_queue_stack, BUTTONS_WORK_QUEUE_STACK_SIZE);

/**
 * @brief Work queue used to handle button pressed
 */
static struct k_work_q buttons_work_queue_handle;

/**
 * @brief Buttons status channel
 */
ZBUS_CHAN_DEFINE(buttons_status_chan, struct button_status_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));

/**
 * @brief Top button
 */
#define WIND_TURBINE_TOP_BUTTON_NODE DT_ALIAS(wind_turbine_top_button)
#if !DT_NODE_HAS_STATUS_OKAY(WIND_TURBINE_TOP_BUTTON_NODE)
#error "wind-turbine-top-button devicetree alias is not defined"
#endif
static const struct gpio_dt_spec buttons_top_button = GPIO_DT_SPEC_GET_OR(WIND_TURBINE_TOP_BUTTON_NODE, gpios, { 0 });
static struct gpio_callback      buttons_top_button_cb_data;
static bool                      buttons_top_button_state = false;
static struct k_work             buttons_top_button_work_handle;

/**
 * @brief Bottom button
 */
#define WIND_TURBINE_BOTTOM_BUTTON_NODE DT_ALIAS(wind_turbine_bottom_button)
#if !DT_NODE_HAS_STATUS_OKAY(WIND_TURBINE_BOTTOM_BUTTON_NODE)
#error "wind-turbine-bottom-button devicetree alias is not defined"
#endif
static const struct gpio_dt_spec buttons_bottom_button = GPIO_DT_SPEC_GET_OR(WIND_TURBINE_BOTTOM_BUTTON_NODE, gpios, { 0 });
static struct gpio_callback      buttons_bottom_button_cb_data;
static bool                      buttons_bottom_button_state = false;
static struct k_work             buttons_bottom_button_work_handle;

static int
buttons_init(void) {

    int result;

    LOG_INF("Initializing buttons...");

    /* Initialize work queue handler */
    k_work_queue_init(&buttons_work_queue_handle);
    k_work_queue_start(&buttons_work_queue_handle, buttons_work_queue_stack, BUTTONS_WORK_QUEUE_STACK_SIZE, BUTTONS_WORK_QUEUE_PRIORITY, NULL);
    k_thread_name_set(k_work_queue_thread_get(&buttons_work_queue_handle), "buttons_work_queue");
    k_work_init(&buttons_top_button_work_handle, buttons_top_button_work_handler);
    k_work_init(&buttons_bottom_button_work_handle, buttons_bottom_button_work_handler);

    /* Configure top button */
    if (!gpio_is_ready_dt(&buttons_top_button)) {
        LOG_ERR("Unable to configure top button '%s', device is not ready", buttons_top_button.port->name);
        return -1;
    }
    if (0 != (result = gpio_pin_configure_dt(&buttons_top_button, GPIO_INPUT))) {
        LOG_ERR("Unable to configure top button '%s', unable to configure pin, result = %d", buttons_top_button.port->name, result);
        return -1;
    }
    if (0 != (result = gpio_pin_interrupt_configure_dt(&buttons_top_button, GPIO_INT_EDGE_BOTH))) {
        LOG_ERR("Unable to configure top button '%s', unable to configure interrupt, result = %d", buttons_top_button.port->name, result);
        return -1;
    }
    gpio_init_callback(&buttons_top_button_cb_data, buttons_top_button_pressed_cb, BIT(buttons_top_button.pin));
    gpio_add_callback(buttons_top_button.port, &buttons_top_button_cb_data);

    /* Configure bottom button */
    if (!gpio_is_ready_dt(&buttons_bottom_button)) {
        LOG_ERR("Unable to configure bottom button '%s', device is not ready", buttons_bottom_button.port->name);
        return -1;
    }
    if (0 != (result = gpio_pin_configure_dt(&buttons_bottom_button, GPIO_INPUT))) {
        LOG_ERR("Unable to configure bottom button '%s', unable to configure pin, result = %d", buttons_bottom_button.port->name, result);
        return -1;
    }
    if (0 != (result = gpio_pin_interrupt_configure_dt(&buttons_bottom_button, GPIO_INT_EDGE_BOTH))) {
        LOG_ERR("Unable to configure bottom button '%s', unable to configure interrupt, result = %d", buttons_bottom_button.port->name, result);
        return -1;
    }
    gpio_init_callback(&buttons_bottom_button_cb_data, buttons_bottom_button_pressed_cb, BIT(buttons_bottom_button.pin));
    gpio_add_callback(buttons_bottom_button.port, &buttons_bottom_button_cb_data);

    LOG_INF("Initializing buttons: DONE");

    return 0;
}

static void
buttons_top_button_pressed_cb(const struct device *port, struct gpio_callback *cb, uint32_t pins) {

    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    /* Submit the work to the work queue */
    if (k_work_submit_to_queue(&buttons_work_queue_handle, &buttons_top_button_work_handle) < 0) {
        LOG_ERR("Unable to submit work to the work queue");
    }
}

static void
buttons_bottom_button_pressed_cb(const struct device *port, struct gpio_callback *cb, uint32_t pins) {

    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    /* Submit the work to the work queue */
    if (k_work_submit_to_queue(&buttons_work_queue_handle, &buttons_bottom_button_work_handle) < 0) {
        LOG_ERR("Unable to submit work to the work queue");
    }
}

static void
buttons_top_button_work_handler(struct k_work *handle) {

    ARG_UNUSED(handle);
    struct button_status_msg button_status_msg = { 0 };

    /* Check button state */
    if ((false == buttons_top_button_state) && (1 == gpio_pin_get_dt(&buttons_top_button))) {
        buttons_top_button_state = true;
    } else if ((true == buttons_top_button_state) && (0 == gpio_pin_get_dt(&buttons_top_button))) {
        buttons_top_button_state = false;
    } else {
        return;
    }

    /* Send button status */
    button_status_msg.name  = "Wind Turbine Top Button";
    button_status_msg.state = buttons_top_button_state;
    zbus_chan_pub(&buttons_status_chan, &button_status_msg, K_MSEC(10));
}

static void
buttons_bottom_button_work_handler(struct k_work *handle) {

    ARG_UNUSED(handle);
    struct button_status_msg button_status_msg = { 0 };

    /* Check button state */
    if ((false == buttons_bottom_button_state) && (1 == gpio_pin_get_dt(&buttons_bottom_button))) {
        buttons_bottom_button_state = true;
    } else if ((true == buttons_bottom_button_state) && (0 == gpio_pin_get_dt(&buttons_bottom_button))) {
        buttons_bottom_button_state = false;
    } else {
        return;
    }

    /* Send button status */
    button_status_msg.name  = "Wind Turbine Bottom Button";
    button_status_msg.state = buttons_bottom_button_state;
    zbus_chan_pub(&buttons_status_chan, &button_status_msg, K_MSEC(10));
}

/**
 * @brief Initialization of buttons
 */
SYS_INIT(buttons_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
