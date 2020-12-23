###########################################################################
# Copyright 2015-2020 IoT.bzh Company
#
# Author: Fulup Ar Foll <fulup@iot.bzh>
# contrib: Romain Forlot <romain.forlot@iot.bzh>
# contrib: Aymeric Aillet <aymeric.aillet@iot.bzh>
#
# $RP_BEGIN_LICENSE$
# Commercial License Usage
#  Licensees holding valid commercial IoT.bzh licenses may use this file in
#  accordance with the commercial license agreement provided with the
#  Software or, alternatively, in accordance with the terms contained in
#  a written agreement between you and The IoT.bzh Company. For licensing terms
#  and conditions see https://www.iot.bzh/terms-conditions. For further
#  information use the contact form at https://www.iot.bzh/contact.
#
# GNU General Public License Usage
#  Alternatively, this file may be used under the terms of the GNU General
#  Public license version 3. This license is as published by the Free Software
#  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
#  of this file. Please review the following information to ensure the GNU
#  General Public License requirements will be met
#  https://www.gnu.org/licenses/gpl-3.0.html.
# $RP_END_LICENSE$
###########################################################################

# Add target to project dependency list
PROJECT_TARGET_ADD(gps-binding)

set( JSON_INFO_C ${CMAKE_CURRENT_BINARY_DIR}/json_info.c)

# Define project Targets
add_library(gps-binding MODULE
		rp-gps-binding.c
		rp-gps-binding.h
		${JSON_INFO_C}
		)

# Binder exposes a unique public entry point
SET_TARGET_PROPERTIES(${TARGET_NAME} PROPERTIES
	PREFIX "lib"
	LABELS "BINDING"
	LINK_FLAGS ${BINDINGS_LINK_FLAG}
	OUTPUT_NAME ${TARGET_NAME}
)

# Library dependencies (include updates automatically)
TARGET_LINK_LIBRARIES(${TARGET_NAME} ${link_libraries})
TARGET_LINK_OPTIONS(${TARGET_NAME} PUBLIC -fprofile-arcs)

add_custom_command(
	OUTPUT json_info.c
	COMMAND echo 'const char * info_verbS=\"\\' > ${JSON_INFO_C}
	COMMAND cat ${CMAKE_CURRENT_SOURCE_DIR}/info_verb.json | sed -e 's/$$/\\\\/' -e 's/\"/\\\\\"/g' >> ${JSON_INFO_C}
	COMMAND echo '\"\;' >> ${JSON_INFO_C}
	DEPENDS info_verb.json
)
