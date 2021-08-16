/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/diagnostics/windows/os_diagnostics_imp.h"

namespace L0 {

void WddmDiagnosticsImp::osGetDiagProperties(zes_diag_properties_t *pProperties){};

ze_result_t WddmDiagnosticsImp::osGetDiagTests(uint32_t *pCount, zes_diag_test_t *pTests) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmDiagnosticsImp::osRunDiagTests(uint32_t start, uint32_t end, zes_diag_result_t *pResult) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

std::unique_ptr<OsDiagnostics> OsDiagnostics::create(OsSysman *pOsSysman, const std::string &DiagTests, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    std::unique_ptr<WddmDiagnosticsImp> pWddmDiagnosticsImp = std::make_unique<WddmDiagnosticsImp>();
    return pWddmDiagnosticsImp;
}

void OsDiagnostics::getSupportedDiagTests(std::vector<std::string> &supportedDiagTests, OsSysman *pOsSysman) {
}

void OsDiagnostics::getSupportedDiagTestsFromFW(void *pFwInterface, std::vector<std::string> &supportedDiagTests) {
}
} // namespace L0
