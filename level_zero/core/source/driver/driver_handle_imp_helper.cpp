/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/driver_experimental/zex_common.h"
#include "level_zero/ze_intel_gpu.h"
#include "level_zero/zet_intel_gpu_metric.h"

namespace L0 {
const std::vector<std::pair<std::string, uint32_t>> DriverHandleImp::extensionsSupported = {
    {ZE_FLOAT_ATOMICS_EXT_NAME, ZE_FLOAT_ATOMICS_EXT_VERSION_CURRENT},
    {ZE_RELAXED_ALLOCATION_LIMITS_EXP_NAME, ZE_RELAXED_ALLOCATION_LIMITS_EXP_VERSION_CURRENT},
    {ZE_MODULE_PROGRAM_EXP_NAME, ZE_MODULE_PROGRAM_EXP_VERSION_CURRENT},
    {ZE_KERNEL_SCHEDULING_HINTS_EXP_NAME, ZE_SCHEDULING_HINTS_EXP_VERSION_CURRENT},
    {ZE_GLOBAL_OFFSET_EXP_NAME, ZE_GLOBAL_OFFSET_EXP_VERSION_CURRENT},
    {ZE_PCI_PROPERTIES_EXT_NAME, ZE_PCI_PROPERTIES_EXT_VERSION_CURRENT},
    {ZE_MEMORY_COMPRESSION_HINTS_EXT_NAME, ZE_MEMORY_COMPRESSION_HINTS_EXT_VERSION_CURRENT},
    {ZE_MEMORY_FREE_POLICIES_EXT_NAME, ZE_MEMORY_FREE_POLICIES_EXT_VERSION_CURRENT},
    {ZE_DEVICE_MEMORY_PROPERTIES_EXT_NAME, ZE_DEVICE_MEMORY_PROPERTIES_EXT_VERSION_CURRENT},
    {ZE_RAYTRACING_EXT_NAME, ZE_RAYTRACING_EXT_VERSION_CURRENT},
    {ZE_CONTEXT_POWER_SAVING_HINT_EXP_NAME, ZE_POWER_SAVING_HINT_EXP_VERSION_CURRENT},
    {ZE_DEVICE_LUID_EXT_NAME, ZE_DEVICE_LUID_EXT_VERSION_CURRENT},
    {ZE_DEVICE_IP_VERSION_EXT_NAME, ZE_DEVICE_IP_VERSION_VERSION_CURRENT},
    {ZE_CACHE_RESERVATION_EXT_NAME, ZE_CACHE_RESERVATION_EXT_VERSION_CURRENT},
    {ZE_IMAGE_COPY_EXT_NAME, ZE_IMAGE_COPY_EXT_VERSION_CURRENT},
    {ZE_IMAGE_VIEW_EXT_NAME, ZE_IMAGE_VIEW_EXP_VERSION_CURRENT},
    {ZE_IMAGE_VIEW_PLANAR_EXT_NAME, ZE_IMAGE_VIEW_PLANAR_EXP_VERSION_CURRENT},
    {ZE_EVENT_QUERY_KERNEL_TIMESTAMPS_EXT_NAME, ZE_EVENT_QUERY_KERNEL_TIMESTAMPS_EXT_VERSION_CURRENT},
    {ZE_RTAS_BUILDER_EXP_NAME, ZE_RTAS_BUILDER_EXP_VERSION_CURRENT},
    {ZE_KERNEL_MAX_GROUP_SIZE_PROPERTIES_EXT_NAME, ZE_KERNEL_MAX_GROUP_SIZE_PROPERTIES_EXT_VERSION_CURRENT},
    {ZE_LINKAGE_INSPECTION_EXT_NAME, ZE_LINKAGE_INSPECTION_EXT_VERSION_CURRENT},
    {ZE_IMMEDIATE_COMMAND_LIST_APPEND_EXP_NAME, ZE_IMMEDIATE_COMMAND_LIST_APPEND_EXP_VERSION_1_0},
    {ZE_GET_KERNEL_BINARY_EXP_NAME, ZE_KERNEL_GET_BINARY_EXP_VERSION_1_0},
    {ZE_EXTERNAL_SEMAPHORES_EXTENSION_NAME, ZE_EXTERNAL_SEMAPHORE_EXT_VERSION_1_0},

    // Driver experimental extensions
    {ZE_INTEL_DEVICE_MODULE_DP_PROPERTIES_EXP_NAME, ZE_INTEL_DEVICE_MODULE_DP_PROPERTIES_EXP_VERSION_CURRENT},
    {ZE_EVENT_POOL_COUNTER_BASED_EXP_NAME, ZE_EVENT_POOL_COUNTER_BASED_EXP_VERSION_CURRENT},
    {ZEX_COUNTER_BASED_EVENT_EXT_NAME, ZEX_COUNTER_BASED_EVENT_VERSION_1_0},
    {ZE_INTEL_COMMAND_LIST_MEMORY_SYNC, ZE_INTEL_COMMAND_LIST_MEMORY_SYNC_EXP_VERSION_CURRENT},
    {ZEX_INTEL_EVENT_SYNC_MODE_EXP_NAME, ZEX_INTEL_EVENT_SYNC_MODE_EXP_VERSION_CURRENT},
    {ZE_INTEL_GET_DRIVER_VERSION_STRING_EXP_NAME, ZE_INTEL_GET_DRIVER_VERSION_STRING_EXP_VERSION_CURRENT},
    {ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_NAME, ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_PROPERTIES_VERSION_CURRENT},
    {ZE_INTEL_KERNEL_GET_PROGRAM_BINARY_EXP_NAME, ZE_INTEL_KERNEL_GET_PROGRAM_BINARY_EXP_VERSION_CURRENT},

    // Metrics Driver experimental extensions
    {ZET_INTEL_METRIC_APPEND_MARKER_EXP_NAME, ZET_INTEL_METRIC_APPEND_MARKER_EXP_VERSION_CURRENT},
    {ZET_INTEL_METRIC_SOURCE_ID_EXP_NAME, ZET_INTEL_METRIC_SOURCE_ID_EXP_VERSION_CURRENT},
    {ZET_INTEL_METRIC_CALCULATE_EXP_NAME, ZET_INTEL_METRIC_CALCULATE_EXP_VERSION_CURRENT}};
} // namespace L0
