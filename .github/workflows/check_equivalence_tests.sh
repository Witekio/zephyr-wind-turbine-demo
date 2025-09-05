#!/bin/bash
# @file      check_equivalence_tests.sh
# @brief     Check equivalence tests
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

# Initialize list of files that are not properly formatted
result=""

# Check source and header files
for source_file in `git ls-tree -r HEAD --name-only | grep -E '(.*\.c$|.*\.h$|.*\.cpp$|.*\.hpp$)' | grep -vFf .clang-format-ignore`
do
    # This grep matches following formats (which are not allowed):
    # - "variable == CONSTANT" / "variable != CONSTANT"
    # - "function(...) == CONSTANT" / "function(...) != CONSTANT"
    # - "table[...] == CONSTANT" / "table[...] != CONSTANT"
    # Where CONSTANT can be a definition or a number
    # Any number of spaces before and after "==" / "!=" is catched
    lines=$(grep "[])a-z][ ]*[!=]=[ ]*[A-Z0-9]" ${source_file} | wc -l)
    if [[ ! $lines -eq 0 ]]; then
        result="${result}\n${source_file}"
    fi
done

# Check result
if [[ ${result} != "" ]]; then
    echo "The following files are not properly formatted:"
    echo -e $result
    exit 1
fi

exit 0

