# @file      CMakeLists.txt
# @brief     wind-turbine project CMakeLists file
#
# Copyright (C) Witekio
#
# This file is part of Zephyr Wind Turbine demonstration.
#
# This demonstration is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This demonstration is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with This demonstration. If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.20.0)

# Declare project
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wind-turbine)

# Includes
target_include_directories(app PRIVATE "include")

# Sources
target_sources(app PRIVATE
    "src/main.c"
    "src/buttons.c"
    "src/inverter.c"
    "src/wind_turbine.c"
)
target_sources_ifdef(CONFIG_NETWORKING app PRIVATE
    "src/network.c"
)
target_sources_ifdef(CONFIG_DISPLAY app PRIVATE
    "src/display.c"
)
target_sources_ifdef(CONFIG_KAMEA app PRIVATE
    "src/kamea.c"
    "creds/key.c"
    "creds/cert.c"
    "creds/ca.c"
)
