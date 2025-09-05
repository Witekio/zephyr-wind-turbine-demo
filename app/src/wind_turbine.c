/**
 * @file      wind_turbine.c
 * @brief     Simulation of the wind turbine
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
LOG_MODULE_REGISTER(wind_turbine_wind_turbine, LOG_LEVEL_INF);

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/zbus/zbus.h>

#include "messages.h"

/**
 * @brief Wind turbine thread stack size (bytes)
 */
#define WIND_TURBINE_THREAD_STACK_SIZE (4096)

/**
 * @brief Wind turbine thread priority
 */
#define WIND_TURBINE_THREAD_PRIORITY (5)

/**
 * @brief ADC channel index
 */
#define WIND_TURBINE_ADC_CHANNEL_INDEX (0)

/**
 * @brief Wind turbine status channel
 */
ZBUS_CHAN_DEFINE(wind_turbine_status_chan, struct wind_turbine_status_msg, NULL, NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));

/**
 * @brief Ensure ADC channel has been defined in the device tree
 */
#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

/**
 * @brief Data of ADC io-channels specified in device tree
 */
#define DT_SPEC_AND_COMMA(node_id, prop, idx) ADC_DT_SPEC_GET_BY_IDX(node_id, idx),
static const struct adc_dt_spec wind_turbine_adc_channels[] = { DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, DT_SPEC_AND_COMMA) };

#ifdef CONFIG_PWM

/**
 * @brief PWM channel used to control wind turbine motor specified in device tree
 */
#define WIND_TURBINE_MOTOR_TIMER_PWM_CHANNEL (1) /* FIXME: improve this, should be from the devicetree using pwm_dt_spec */

/**
 * @brief PWM instance used to control wind turbine motor specified in device tree
 * @note This is constructed using the devicetree_generated.h file and the devicetree APIs.
 * @note DT_ALIAS(wind_turbine_motor_timer) => DT_N_ALIAS_wind_turbine_motor_timer = DT_N_S_soc_S_timers_40000400
 * @note DT_CHILD(DT_ALIAS(wind_turbine_motor_timer), pwm) => DT_N_S_soc_S_timers_40000400_S_pwm
 */
const struct device *wind_turbine_motor_timer_pwm = DEVICE_DT_GET(DT_CHILD(DT_ALIAS(wind_turbine_motor_timer), pwm));

#endif /* CONFIG_PWM */

/**
 * @brief Thread used to periodically read ADC input channel
 */
static void
wind_turbine_thread(void) {

    int                 err;
    uint16_t            adc_value;
    struct adc_sequence sequence = {
        .buffer      = &adc_value,
        .buffer_size = sizeof(adc_value),
    };
    struct wind_turbine_status_msg wind_turbine_status_msg = { 0 };
    double                         raw_value, voltage;

    LOG_INF("Initializing wind turbine...");

    /* Configure ADC channel prior to sampling */
    if (!adc_is_ready_dt(&wind_turbine_adc_channels[WIND_TURBINE_ADC_CHANNEL_INDEX])) {
        LOG_ERR("ADC controller device %s not ready", wind_turbine_adc_channels[WIND_TURBINE_ADC_CHANNEL_INDEX].dev->name);
        return;
    }
    if ((err = adc_channel_setup_dt(&wind_turbine_adc_channels[WIND_TURBINE_ADC_CHANNEL_INDEX])) < 0) {
        LOG_ERR("Could not setup ADC channel %d (%d)", WIND_TURBINE_ADC_CHANNEL_INDEX, err);
        return;
    }

    LOG_INF("Initializing wind turbine: DONE");

    /* Infinite loop */
    while (1) {

        /* Sample the ADC channel */
        if ((err = adc_sequence_init_dt(&wind_turbine_adc_channels[WIND_TURBINE_ADC_CHANNEL_INDEX], &sequence)) < 0) {
            LOG_ERR("Could not init ADC channel %d (%d)", WIND_TURBINE_ADC_CHANNEL_INDEX, err);
            continue;
        }
        if ((err = adc_read_dt(&wind_turbine_adc_channels[WIND_TURBINE_ADC_CHANNEL_INDEX], &sequence)) < 0) {
            LOG_ERR("Could not read ADC channel %d (%d)", WIND_TURBINE_ADC_CHANNEL_INDEX, err);
            continue;
        }

        /* Simulate the wind turbine parameters (numbers are chosen to have a nice and coherent display on the demo) */
        raw_value = (double)adc_value;
        if (raw_value < 64)
            raw_value = 0;
        voltage = (raw_value < 1340) ? (raw_value / 2) : (670 + (120 * ((raw_value / 2) - 670)) / 4096);

#ifdef CONFIG_PWM

        /* Drive motor according to current wind turbine simulated parameters */
        pwm_set(wind_turbine_motor_timer_pwm, WIND_TURBINE_MOTOR_TIMER_PWM_CHANNEL, 1000000, (raw_value * 1000000) / 4096, 0);

#endif /* CONFIG_PWM */

        /* Send wind turbine status */
        wind_turbine_status_msg.wind_speed     = (uint16_t)((raw_value * 100) / 4096);
        wind_turbine_status_msg.generator_rpm  = (uint16_t)((raw_value * 30) / 4096);
        wind_turbine_status_msg.output_power   = (uint16_t)raw_value;
        wind_turbine_status_msg.output_voltage = (uint16_t)voltage;
        zbus_chan_pub(&wind_turbine_status_chan, &wind_turbine_status_msg, K_MSEC(10));

        /* Sleeping before next sampling */
        k_msleep(100);
    }
}

/**
 * @brief Create wind turbine thread
 */
K_THREAD_DEFINE(wind_turbine_thread_id, WIND_TURBINE_THREAD_STACK_SIZE, wind_turbine_thread, NULL, NULL, NULL, WIND_TURBINE_THREAD_PRIORITY, 0, 0);
