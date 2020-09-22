/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/engine_imp.h"

#include "shared/source/helpers/debug_helpers.h"
namespace L0 {

ze_result_t EngineImp::engineGetActivity(zes_engine_stats_t *pStats) {
    return pOsEngine->getActivity(pStats);
}

ze_result_t EngineImp::engineGetProperties(zes_engine_properties_t *pProperties) {
    *pProperties = engineProperties;
    return ZE_RESULT_SUCCESS;
}

void EngineImp::init() {
    pOsEngine->getProperties(engineProperties);
}

EngineImp::EngineImp(OsSysman *pOsSysman, zes_engine_group_t engineType, uint32_t engineInstance) {
    pOsEngine = OsEngine::create(pOsSysman, engineType, engineInstance);
    init();
}

EngineImp::~EngineImp() {
    if (nullptr != pOsEngine) {
        delete pOsEngine;
        pOsEngine = nullptr;
    }
}

} // namespace L0
