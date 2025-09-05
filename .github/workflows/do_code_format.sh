#!/bin/bash
# @file      do_code_format.sh
# @brief     Update all source files according to wanted code format
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

# Update source and header files
for source_file in `git ls-tree -r HEAD --name-only | grep -E '(.*\.c$|.*\.h$|.*\.cpp$|.*\.hpp$)' | grep -vFf .clang-format-ignore`
do
    clang-format -i ${source_file} || exit 1
done

exit 0

