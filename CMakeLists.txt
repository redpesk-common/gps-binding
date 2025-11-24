###########################################################################
# Copyright 2015-2020 IoT.bzh Company
#
# Author: Romain Forlot <romain.forlot@iot.bzh>
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

CMAKE_MINIMUM_REQUIRED(VERSION 3.16)

project(gps-binding
    VERSION 2.0.0
    DESCRIPTION "Binding for handling GPS data from gpsd"
    HOMEPAGE_URL "https://github.com/redpesk-common/gps-binding"
    LANGUAGES C
)

# Declare options
set(AFM_APP_DIR ${CMAKE_INSTALL_PREFIX}/redpesk CACHE PATH "Applications directory")
set(APP_DIR ${AFM_APP_DIR}/${PROJECT_NAME})

# Check dependencies
include(FindPkgConfig)
pkg_check_modules(deps REQUIRED
    json-c
    afb-binding>=4.1.0
    libgps
    liburcu
    afb-helpers4
)

# Build
add_library(gps-binding SHARED
                        binding/rp-gps-binding.c
                        binding/rp-gps-binding.h
                        binding/json_info.c)
target_include_directories(gps-binding PRIVATE ${deps_INCLUDE_DIRS})
set_target_properties(gps-binding PROPERTIES PREFIX "")
target_link_options(gps-binding PRIVATE ${deps_LDFLAGS} -lm)

# This version script is a linker script which exports all symbols named "afbBinding*" and makes all the other symbols local only
pkg_get_variable(vscript afb-binding version_script)
if(vscript)
    target_link_options(gps-binding PRIVATE -Wl,--version-script=${vscript})
endif(vscript)

# Install
install(TARGETS gps-binding DESTINATION ${APP_DIR}/lib)
install(FILES manifest.yml DESTINATION ${APP_DIR}/.rpconfig)
install(PROGRAMS ${CMAKE_SOURCE_DIR}/redtest/run-redtest
	DESTINATION /usr/libexec/redtest/${PROJECT_NAME}/)
install(FILES ${CMAKE_SOURCE_DIR}/test/tests.py
    DESTINATION /usr/libexec/redtest/${PROJECT_NAME}/)
install(FILES ${CMAKE_SOURCE_DIR}/test/lorient.nmea
    DESTINATION /usr/libexec/redtest/${PROJECT_NAME}/)
