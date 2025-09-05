/**
 * @file      main.c
 * @brief     Main entry point
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
LOG_MODULE_REGISTER(wind_turbine_demo, LOG_LEVEL_INF);

/**
 * @brief Main function
 * @return Always returns 0
 */
int
main(void) {

    /* FIXME: this should be a initialized in display.c instead of here */
    extern int display_init(void);
    display_init();

    return 0;
}
