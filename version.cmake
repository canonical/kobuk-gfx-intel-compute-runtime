#
# Copyright (C) 2018-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_OCL_VERSION_MAJOR 24)
set(NEO_OCL_VERSION_MINOR 45)

if(NOT DEFINED NEO_VERSION_BUILD)
  set(NEO_VERSION_BUILD 031740)
  set(NEO_REVISION 031740)
else()
  set(NEO_REVISION ${NEO_VERSION_BUILD})
endif()

if(NOT DEFINED NEO_VERSION_HOTFIX)
  set(NEO_VERSION_HOTFIX 0)
endif()

# OpenCL package version
set(NEO_OCL_DRIVER_VERSION "${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}")

# Level-Zero package version
set(NEO_L0_VERSION_MAJOR 1)
set(NEO_L0_VERSION_MINOR 6)

# Remove leading zeros
string(REGEX REPLACE "^0+([0-9]+)" "\\1" NEO_VERSION_BUILD "${NEO_VERSION_BUILD}")
string(REGEX REPLACE "^0+([0-9]+)" "\\1" NEO_VERSION_HOTFIX "${NEO_VERSION_HOTFIX}")
