/**
 * @file      display.c
 * @brief     Management of the display
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
LOG_MODULE_REGISTER(wind_turbine_display, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/zbus/zbus.h>
#include <lvgl_zephyr.h>
#include <lvgl.h>

#include "display/background.h"
#include "messages.h"

/**
 * @brief Work queue stack size (Bytes)
 */
#define DISPLAY_WORK_QUEUE_STACK_SIZE (8192)

/**
 * @brief Work queue priority
 */
#define DISPLAY_WORK_QUEUE_PRIORITY (5)

/**
 * @brief Display initialization
 * @return 0 if the function succeeds, error code otherwise
 */
/*static*/ int display_init(void); /* FIXME: this should be a static function */

/**
 * @brief Function used to handle work context timer when it expires
 * @param handle Timer handler
 */
static void display_timer_callback(struct k_timer *handle);

/**
 * @brief Function used to handle work
 * @param handle Work handler
 */
static void display_work_handler(struct k_work *handle);

/**
 * @brief Create screen 1
 */
static void display_create_screen1(void);

/**
 * @brief Create screen 2
 */
static void display_create_screen2(void);

/**
 * @brief Wind turbine status button callback
 * @param event Event value
 */
static void display_screen1_wind_turbine_status_button_callback(lv_event_t *event);

/**
 * @brief Wind turbine current animation callback
 * @param var Wind turbine current animation object
 * @param v Animation value
 */
static void display_screen1_animation_wind_turbine_current_exec_callback(void *var, int32_t v);

/**
 * @brief Screen2 back button callback
 * @param event Event value
 */
static void display_screen2_back_button_callback(lv_event_t *event);

/**
 * @brief Wind turbine status callback
 * @note This callback is used to refresh the wind turbine status on the display
 * @param chan Wind turbine status channel
 */
static void display_wind_turbine_status_callback(const struct zbus_channel *chan);

/**
 * @brief Inverter status callback
 * @note This callback is used to refresh the inverter status on the display
 * @param chan Inverter status channel
 */
static void display_inverter_status_callback(const struct zbus_channel *chan);

/**
 * @brief Network status callback
 * @note This callback is used to refresh the network status on the display
 * @param chan Network status channel
 */
static void display_network_status_callback(const struct zbus_channel *chan);

/**
 * @brief Display work queue stack
 */
K_THREAD_STACK_DEFINE(display_work_queue_stack, DISPLAY_WORK_QUEUE_STACK_SIZE);

/**
 * @brief Timer used to periodically schedule the refresh of the screen
 */
static struct k_timer display_timer_handle;

/**
 * @brief Work queue used to periodically refresh the screen
 */
static struct k_work_q display_work_queue_handle;

/**
 * @brief Work handler used to periodically refresh the screen
 */
static struct k_work display_work_handle;

/**
 * @brief Styles
 */
static lv_style_t display_style_transp;

/**
 * @brief Screen 1
 */
static lv_obj_t *display_screen1                            = NULL;
static lv_obj_t *display_screen1_wind_turbine_status_button = NULL;
static lv_obj_t *display_screen1_wind_turbine_status_label  = NULL;
static lv_obj_t *display_screen1_inverter_status_label      = NULL;
static lv_obj_t *display_screen1_network_status_label       = NULL;

/**
 * @brief Screen 2
 */
static lv_obj_t *display_screen2                   = NULL;
static lv_obj_t *display_screen2_back_button       = NULL;
static lv_obj_t *display_screen2_back_button_label = NULL;
static lv_obj_t *display_screen2_chart             = NULL;

/**
 * @brief Wind turbine current animations and objects
 */
#define DISPLAY_ANIMATION_WIND_TURBINE_CURRENT_OBJECTS_COUNT (12)
static lv_anim_t display_animation_wind_turbine_current[DISPLAY_ANIMATION_WIND_TURBINE_CURRENT_OBJECTS_COUNT];
static lv_obj_t *display_animation_wind_turbine_current_objs[DISPLAY_ANIMATION_WIND_TURBINE_CURRENT_OBJECTS_COUNT];

/**
 * @brief Zbus channels
 */
ZBUS_CHAN_DECLARE(wind_turbine_status_chan);
ZBUS_CHAN_DECLARE(inverter_status_chan);
ZBUS_CHAN_DECLARE(network_status_chan);

/**
 * @brief Zbus listeners
 */
ZBUS_LISTENER_DEFINE(display_wind_turbine_status_listenner, display_wind_turbine_status_callback);
ZBUS_LISTENER_DEFINE(display_inverter_status_listenner, display_inverter_status_callback);
ZBUS_LISTENER_DEFINE(display_network_status_listenner, display_network_status_callback);

/* FIXME: this should be a static function */
/*static*/ int
display_init(void) {

    const struct device *display_dev;

    LOG_INF("Initializing display...");

    /* Check if display is available */
    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Device not ready, aborting test");
        return 0; /* FIXME: we should catch the error */
    }

    /* Create work and initialize timer to periodically refresh the display */
    k_work_queue_init(&display_work_queue_handle);
    k_work_queue_start(&display_work_queue_handle, display_work_queue_stack, DISPLAY_WORK_QUEUE_STACK_SIZE, DISPLAY_WORK_QUEUE_PRIORITY, NULL);
    k_thread_name_set(k_work_queue_thread_get(&display_work_queue_handle), "display_work_queue");
    k_work_init(&display_work_handle, display_work_handler);
    k_timer_init(&display_timer_handle, display_timer_callback, NULL);

    /* Create styles */
    lv_style_init(&display_style_transp);
    lv_style_set_bg_opa(&display_style_transp, LV_OPA_TRANSP);
    lv_style_set_border_width(&display_style_transp, 0);

    /* Create screens */
    display_create_screen1();
    display_create_screen2();

    /* Display landing screen */
    lv_scr_load(display_screen1);

    /* Refresh display */
    lv_timer_handler();

    /* Switch ON display */
    display_blanking_off(display_dev);

    /* Start periodic refresh of the display */
    k_timer_start(&display_timer_handle, K_NO_WAIT, K_MSEC(10));

    /* Register to Zbus channels */
    zbus_chan_add_obs(&wind_turbine_status_chan, &display_wind_turbine_status_listenner, K_MSEC(10));
    zbus_chan_add_obs(&inverter_status_chan, &display_inverter_status_listenner, K_MSEC(10));
    zbus_chan_add_obs(&network_status_chan, &display_network_status_listenner, K_MSEC(10));

    LOG_INF("Initializing display: DONE");

    return 0;
}

static void
display_timer_callback(struct k_timer *handle) {

    ARG_UNUSED(handle);

    /* Submit the work to the work queue */
    if (k_work_submit_to_queue(&display_work_queue_handle, &display_work_handle) < 0) {
        LOG_ERR("Unable to submit work to the work queue");
    }
}

static void
display_work_handler(struct k_work *handle) {

    ARG_UNUSED(handle);

    /* Refresh display */
    lv_timer_handler();
}

static void
display_create_screen1(void) {

    int index;

    /* Create screen */
    display_screen1 = lv_obj_create(NULL);

    /* Create background */
    lv_obj_t *background = lv_image_create(display_screen1);
    lv_obj_align(background, LV_ALIGN_CENTER, 0, 0);
    lv_image_set_src(background, &background_screen1);

    /* Create wind turbine status label */
    display_screen1_wind_turbine_status_button = lv_button_create(display_screen1);
    lv_obj_add_event_cb(display_screen1_wind_turbine_status_button, display_screen1_wind_turbine_status_button_callback, LV_EVENT_PRESSED, NULL);
    lv_obj_align(display_screen1_wind_turbine_status_button, LV_ALIGN_CENTER, -175, 35);
    lv_obj_remove_flag(display_screen1_wind_turbine_status_button, LV_OBJ_FLAG_PRESS_LOCK);
    //lv_obj_add_style(display_screen1_wind_turbine_status_button, &display_style_transp, 0); /* FIXME: button used to display the wind turbine status should be transparent */
    display_screen1_wind_turbine_status_label = lv_label_create(display_screen1_wind_turbine_status_button);
    lv_label_set_text(display_screen1_wind_turbine_status_label, "--V\n--kW");
    lv_obj_set_style_text_align(display_screen1_wind_turbine_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(display_screen1_wind_turbine_status_label);

    /* Create inverter status label */
    display_screen1_inverter_status_label = lv_label_create(display_screen1);
    lv_obj_align(display_screen1_inverter_status_label, LV_ALIGN_CENTER, -35, -50);
    lv_label_set_text(display_screen1_inverter_status_label, "--kV\n--kW\n--Hz");
    lv_obj_set_style_text_align(display_screen1_inverter_status_label, LV_TEXT_ALIGN_CENTER, 0);

    /* Create network status label */
    display_screen1_network_status_label = lv_label_create(display_screen1);
    lv_obj_align(display_screen1_network_status_label, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_label_set_text(display_screen1_network_status_label, "IP Address: --");
    lv_obj_set_style_text_align(display_screen1_network_status_label, LV_TEXT_ALIGN_CENTER, 0);

    /* Create wind turbine current animations */
    for (index = 0; index < DISPLAY_ANIMATION_WIND_TURBINE_CURRENT_OBJECTS_COUNT; index++) {
        display_animation_wind_turbine_current_objs[index] = lv_obj_create(display_screen1);
        lv_obj_remove_flag(display_animation_wind_turbine_current_objs[index], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(display_animation_wind_turbine_current_objs[index], lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_radius(display_animation_wind_turbine_current_objs[index], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_size(display_animation_wind_turbine_current_objs[index], 15, 15);
        lv_obj_align(display_animation_wind_turbine_current_objs[index], LV_ALIGN_CENTER, -175, 78);
        lv_anim_init(&display_animation_wind_turbine_current[index]);
        lv_anim_set_var(&display_animation_wind_turbine_current[index], display_animation_wind_turbine_current_objs[index]);
        lv_anim_set_values(&display_animation_wind_turbine_current[index], 0, 165);     /* 'v' range from 0 to 165 */
        lv_anim_set_delay(&display_animation_wind_turbine_current[index], 500 * index); /* Create a small distance between the objects */
        lv_anim_set_duration(&display_animation_wind_turbine_current[index], 12288);
        lv_anim_set_reverse_delay(&display_animation_wind_turbine_current[index], 0);
        lv_anim_set_reverse_duration(&display_animation_wind_turbine_current[index], 12288);
        lv_anim_set_repeat_delay(&display_animation_wind_turbine_current[index], 0);
        lv_anim_set_repeat_count(&display_animation_wind_turbine_current[index], LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&display_animation_wind_turbine_current[index], lv_anim_path_ease_in_out);
        lv_anim_set_exec_cb(&display_animation_wind_turbine_current[index], display_screen1_animation_wind_turbine_current_exec_callback);
        lv_anim_start(&display_animation_wind_turbine_current[index]);
    }
}

static void
display_create_screen2(void) {

    /* Create screen */
    display_screen2 = lv_obj_create(NULL);

    /* Create back button */
    display_screen2_back_button = lv_button_create(display_screen2);
    lv_obj_add_event_cb(display_screen2_back_button, display_screen2_back_button_callback, LV_EVENT_PRESSED, NULL);
    lv_obj_align(display_screen2_back_button, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_remove_flag(display_screen2_back_button, LV_OBJ_FLAG_PRESS_LOCK);
    display_screen2_back_button_label = lv_label_create(display_screen2_back_button);
    lv_label_set_text(display_screen2_back_button_label, "Back");
    lv_obj_set_style_text_align(display_screen2_back_button_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(display_screen2_back_button_label);

    /* Create chart */
    display_screen2_chart = lv_chart_create(display_screen2);
    lv_chart_set_update_mode(display_screen2_chart, LV_CHART_UPDATE_MODE_CIRCULAR);
    lv_obj_set_style_size(display_screen2_chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_size(display_screen2_chart, 480, 180);
    lv_obj_center(display_screen2_chart);
    lv_chart_set_point_count(display_screen2_chart, 80);
    lv_chart_add_series(display_screen2_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
}

static void
display_screen1_wind_turbine_status_button_callback(lv_event_t *event) {

    ARG_UNUSED(event);

    /* Load screen2 */
    lv_scr_load(display_screen2);
}

static void
display_screen1_animation_wind_turbine_current_exec_callback(void *var, int32_t v) {

    /* Compute coordinates based on the animation value 'v' */
    if (v < 25) {
        lv_obj_set_y((lv_obj_t *)var, v + 78);
        lv_obj_set_x((lv_obj_t *)var, -175);
    } else if (v > 140) {
        lv_obj_set_y((lv_obj_t *)var, 243 - v);
        lv_obj_set_x((lv_obj_t *)var, -60);
    } else {
        lv_obj_set_y((lv_obj_t *)var, 103);
        lv_obj_set_x((lv_obj_t *)var, v - 200);
    }

    /* Change color when the object reach the endinds of the animation */
    if (0 == v) {
        lv_obj_set_style_bg_color((lv_obj_t *)var, lv_palette_main(LV_PALETTE_RED), 0);
    }
    if (165 == v) {
        lv_obj_set_style_bg_color((lv_obj_t *)var, lv_palette_main(LV_PALETTE_BROWN), 0);
    }
}

static void
display_screen2_back_button_callback(lv_event_t *event) {

    ARG_UNUSED(event);

    /* Load screen1 */
    lv_scr_load(display_screen1);
}

static void
display_wind_turbine_status_callback(const struct zbus_channel *chan) {

    char                                  str[64];
    const struct wind_turbine_status_msg *wind_turbine_status_msg = zbus_chan_const_msg(chan);
    int                                   index;

    /* Format and display status */
    lv_snprintf(str, sizeof(str), "%dV\n%dkW", wind_turbine_status_msg->output_voltage, wind_turbine_status_msg->output_power);
    lv_label_set_text(display_screen1_wind_turbine_status_label, str);

    /* Modify the duration of the wind turbine current animation based on the output power value */
    uint32_t duration = 12288 - 2 * wind_turbine_status_msg->output_power;
    for (index = 0; index < DISPLAY_ANIMATION_WIND_TURBINE_CURRENT_OBJECTS_COUNT; index++) {
        lv_anim_set_duration(&display_animation_wind_turbine_current[index], duration);
        lv_anim_set_reverse_duration(&display_animation_wind_turbine_current[index], duration);
    }

    /* Update wind turbine output power chart */
    lv_chart_series_t *ser = lv_chart_get_series_next(display_screen2_chart, NULL);
    lv_chart_set_next_value(display_screen2_chart, ser, (100 * wind_turbine_status_msg->output_power) / 4096);
    uint32_t p     = lv_chart_get_point_count(display_screen2_chart);
    uint32_t s     = lv_chart_get_x_start_point(display_screen2_chart, ser);
    int32_t *a     = lv_chart_get_series_y_array(display_screen2_chart, ser);
    a[(s + 1) % p] = LV_CHART_POINT_NONE;
    a[(s + 2) % p] = LV_CHART_POINT_NONE;
    a[(s + 3) % p] = LV_CHART_POINT_NONE;
    lv_chart_refresh(display_screen2_chart);
}

static void
display_inverter_status_callback(const struct zbus_channel *chan) {

    char                              str[64];
    const struct inverter_status_msg *inverter_status_msg = zbus_chan_const_msg(chan);

    /* Format and display status */
    lv_snprintf(str,
                sizeof(str),
                "%.1fkV\n%dkW\n%.1fHz",
                ((double)(inverter_status_msg->output_voltage)) / 1000,
                inverter_status_msg->output_power,
                inverter_status_msg->frequency);
    lv_label_set_text(display_screen1_inverter_status_label, str);
}

static void
display_network_status_callback(const struct zbus_channel *chan) {

    char                             str[64];
    const struct network_status_msg *network_status_msg = zbus_chan_const_msg(chan);

    /* Format and display status */
    if (!network_status_msg->connected) {
        lv_snprintf(str, sizeof(str), "IP Address: --");
    } else {
        lv_snprintf(str, sizeof(str), "IP Address: %s", network_status_msg->ip_address);
    }
    lv_label_set_text(display_screen1_network_status_label, str);
}

/**
 * @brief Initialization of display
 */
//SYS_INIT(display_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY); /* FIXME: this should be initialized here */
