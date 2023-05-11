#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# Wifi boards
target_sources_ifdef(CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP
		     app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/network_wifi.c)
