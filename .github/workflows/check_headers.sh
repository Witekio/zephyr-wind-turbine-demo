#!/bin/bash
# @file      check_headers.sh
# @brief     Check headers
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

# Function used to check headers
# Arg1: Source file path
# Arg2: First line, "" if it is not present
# Arg3: String added in front of each new line except the first and last lines
# Arg4: Last line, "" if it is not present
check_header() {
    source_file=$1
    first_line=$2
    new_line=$3
    last_line=$4
    filename=$(echo $(basename ${source_file}) | sed -r 's/\+/\\+/g')
    pcregrep -Me "${first_line}${first_line:+\n}${new_line} @file      ${filename}\n${new_line} @brief     [[:print:]]*\n${new_line}\n${new_line} Copyright \(C\) Witekio\n${new_line}\n${new_line} This file is part of Zephyr Wind Turbine demonstration.\n${new_line}\n${new_line} This demonstration is free software: you can redistribute it and\/or modify\n${new_line} it under the terms of the GNU General Public License as published by\n${new_line} the Free Software Foundation, either version 3 of the License, or\n${new_line} \(at your option\) any later version.\n${new_line}\n${new_line} This demonstration is distributed in the hope that it will be useful,\n${new_line} but WITHOUT ANY WARRANTY; without even the implied warranty of\n${new_line} MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n${new_line} GNU General Public License for more details.\n${new_line}\n${new_line} You should have received a copy of the GNU General Public License\n${new_line} along with This demonstration. If not, see <http:\/\/www.gnu.org\/licenses\/>.${last_line:+\n}${last_line}\n" ${source_file} > /dev/null 2>&1
    return $?
}

# Initialize list of files that are not properly formatted
result=""

# Check source, header, ASM and linker files
for source_file in `git ls-tree -r HEAD --name-only | grep -E '(.*\.c$|.*\.h$|.*\.cpp$|.*\.hpp$|.*\.s$|.*\.ld$|.*\.dtsi|.*\.overlay)' | grep -vFf .clang-format-ignore`
do
    check_header ${source_file} "/\*\*" " \*" " \*/"
    if [[ ! $? -eq 0 ]]; then
        result="${result}\n${source_file}"
    fi
done

# Check YAML, Python, CMake and configuration files
for source_file in `git ls-tree -r HEAD --name-only | grep -E '(.*\.yml$|.*\.yaml$|.*\.py$|CMakeLists.*\.txt$|.*\.cmake$|.*\.cfg$|.*\.conf$|.clang-format$|Kconfig.*$|.gitignore$)' | grep -vFf .clang-format-ignore`
do
    check_header ${source_file} "" "#" ""
    if [[ ! $? -eq 0 ]]; then
        result="${result}\n${source_file}"
    fi
done

# Check bash files
for source_file in `git ls-tree -r HEAD --name-only | grep -E '(.*\.sh$)' | grep -vFf .clang-format-ignore`
do
    check_header ${source_file} "#!/bin/bash" "#" ""
    if [[ ! $? -eq 0 ]]; then
        result="${result}\n${source_file}"
    fi
done

# Check result
if [[ ${result} != "" ]]; then
    echo "The following files have wrong header format:"
    echo -e $result
    exit 1
fi

exit 0

