#!/bin/bash
# @file      check_include_guards.sh
# @brief     Check include guards
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

# Check header files
for source_file in `git ls-tree -r HEAD --name-only | grep -E '(.*\.h$|.*\.hpp$)' | grep -vFf .clang-format-ignore`
do
    uppercase=$(echo $(basename ${source_file^^}) | tr '.' '_' | tr '-' '_')
    pcregrep -Me "#ifndef __${uppercase}__\n#define __${uppercase}__\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif /\* __cplusplus \*/" ${source_file} > /dev/null 2>&1
    if [[ ! $? -eq 0 ]]; then
        result="${result}\n${source_file}"
    else
        pcregrep -Me "#ifdef __cplusplus\n}\n#endif /\* __cplusplus \*/\n\n#endif /\* __${uppercase}__ \*/" ${source_file} > /dev/null 2>&1
        if [[ ! $? -eq 0 ]]; then
            result="${result}\n${source_file}"
        fi
   fi
done

# Check result
if [[ ${result} != "" ]]; then
    echo "The following files have wrong or missing include guards:"
    echo -e $result
    exit 1
fi

exit 0

