/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "platforms.h"
#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release1271;

template <>
bool ReleaseHelperHw<release>::isPipeControlPriorToNonPipelinedStateCommandsWARequired() const {
    return hardwareIpVersion.value == AOT::MTL_P_A0;
}

template <>
int ReleaseHelperHw<release>::getProductMaxPreferredSlmSize(int preferredEnumValue) const {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    if (hardwareIpVersion.value == AOT::MTL_P_A0) {
        return static_cast<int>(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K);
    }
    return preferredEnumValue;
}

} // namespace NEO

#include "shared/source/release_helper/release_helper_common_xe_lpg.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
