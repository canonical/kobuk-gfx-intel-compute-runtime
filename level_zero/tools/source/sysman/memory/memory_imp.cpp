/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/memory/memory_imp.h"

namespace L0 {

ze_result_t MemoryImp::memoryGetBandwidth(zes_mem_bandwidth_t *pBandwidth) {
    return pOsMemory->getBandwidth(pBandwidth);
}

ze_result_t MemoryImp::memoryGetState(zes_mem_state_t *pState) {
    return pOsMemory->getState(pState);
}

ze_result_t MemoryImp::memoryGetProperties(zes_mem_properties_t *pProperties) {
    *pProperties = memoryProperties;
    return ZE_RESULT_SUCCESS;
}

void MemoryImp::init() {
    this->initSuccess = pOsMemory->isMemoryModuleSupported();
    if (this->initSuccess == true) {
        pOsMemory->getProperties(&memoryProperties);
    }
}

MemoryImp::MemoryImp(OsSysman *pOsSysman) {
    pOsMemory = OsMemory::create(pOsSysman);
    init();
}

MemoryImp::~MemoryImp() {
    if (nullptr != pOsMemory) {
        delete pOsMemory;
    }
}

} // namespace L0
