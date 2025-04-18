#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_thread_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/eu_thread_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_debug_session.h
               ${CMAKE_CURRENT_SOURCE_DIR}/test_debug_api.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_common.h
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_common.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_registers_access.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_registers_access.h
)
if(SUPPORT_XE3_AND_LATER)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_tests_xe3_and_later.cpp
  )
endif()

add_subdirectories()
