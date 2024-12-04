/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/driver_experimental/public/zex_event.h"

#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/event/event.h"

namespace L0 {

ZE_APIEXPORT ze_result_t ZE_APICALL
zexEventGetDeviceAddress(ze_event_handle_t event, uint64_t *completionValue, uint64_t *address) {
    auto eventObj = Event::fromHandle(toInternalType(event));

    if (!eventObj || !completionValue || !address) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (eventObj->isCounterBased()) {
        if (!eventObj->getInOrderExecInfo()) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        *completionValue = eventObj->getInOrderExecSignalValueWithSubmissionCounter();
        *address = eventObj->getInOrderExecInfo()->getBaseDeviceAddress() + eventObj->getInOrderAllocationOffset();
    } else if (eventObj->isEventTimestampFlagSet()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    } else {
        *completionValue = Event::State::STATE_SIGNALED;
        *address = eventObj->getCompletionFieldGpuAddress(eventObj->peekEventPool()->getDevice());
    }

    return ZE_RESULT_SUCCESS;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCounterBasedEventCreate(ze_context_handle_t hContext, ze_device_handle_t hDevice, uint64_t *deviceAddress, uint64_t *hostAddress, uint64_t completionValue, const ze_event_desc_t *desc, ze_event_handle_t *phEvent) {
    constexpr uint32_t counterBasedFlags = (ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE | ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE);

    auto device = Device::fromHandle(toInternalType(hDevice));

    if (!hDevice || !desc || !phEvent) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    EventDescriptor eventDescriptor = {
        nullptr,                           // eventPoolAllocation
        desc->pNext,                       // extensions
        0,                                 // totalEventSize
        EventPacketsCount::maxKernelSplit, // maxKernelCount
        0,                                 // maxPacketsCount
        counterBasedFlags,                 // counterBasedFlags
        0,                                 // index
        0,                                 // signalScope
        0,                                 // waitScope
        false,                             // timestampPool
        false,                             // kerneMappedTsPoolFlag
        false,                             // importedIpcPool
        false,                             // ipcPool
    };

    zex_counter_based_event_external_sync_alloc_properties_t externalSyncAllocProperties = {ZEX_STRUCTURE_COUTER_BASED_EVENT_EXTERNAL_SYNC_ALLOC_PROPERTIES}; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-12901
    externalSyncAllocProperties.completionValue = completionValue;
    externalSyncAllocProperties.deviceAddress = deviceAddress;
    externalSyncAllocProperties.hostAddress = hostAddress;

    if (externalSyncAllocProperties.deviceAddress && externalSyncAllocProperties.hostAddress) {
        externalSyncAllocProperties.pNext = desc->pNext;
        eventDescriptor.extensions = &externalSyncAllocProperties;
    }

    ze_result_t result = ZE_RESULT_SUCCESS;
    *phEvent = Event::create<uint64_t>(eventDescriptor, device, result);

    return result;
}

ZE_APIEXPORT ze_result_t ZE_APICALL zexIntelAllocateNetworkInterrupt(ze_context_handle_t hContext, uint32_t &networkInterruptId) {
    auto context = static_cast<ContextImp *>(L0::Context::fromHandle(toInternalType(hContext)));

    if (!context) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (!context->getDriverHandle()->getMemoryManager()->allocateInterrupt(networkInterruptId, context->rootDeviceIndices[0])) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    return ZE_RESULT_SUCCESS;
}

ZE_APIEXPORT ze_result_t ZE_APICALL zexIntelReleaseNetworkInterrupt(ze_context_handle_t hContext, uint32_t networkInterruptId) {
    auto context = static_cast<ContextImp *>(L0::Context::fromHandle(toInternalType(hContext)));

    if (!context || !context->getDriverHandle()->getMemoryManager()->releaseInterrupt(networkInterruptId, context->rootDeviceIndices[0])) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return ZE_RESULT_SUCCESS;
}

} // namespace L0
