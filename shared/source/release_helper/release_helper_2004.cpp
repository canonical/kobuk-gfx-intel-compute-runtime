/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/release_helper/release_helper_base.inl"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

#include "release_definitions.h"

#include <algorithm>

namespace NEO {
constexpr auto release = ReleaseType::release2004;

template <>
bool ReleaseHelperHw<release>::shouldAdjustDepth() const {
    return true;
}

template <>
inline bool ReleaseHelperHw<release>::isAuxSurfaceModeOverrideRequired() const {
    return true;
}

template <>
int ReleaseHelperHw<release>::getProductMaxPreferredSlmSize(int preferredEnumValue) const {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename Xe2HpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    return std::min(preferredEnumValue, static_cast<int>(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K));
}

template <>
bool ReleaseHelperHw<release>::isLocalOnlyAllowed() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isBindlessAddressingDisabled() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isMidThreadPreemptionDisallowedForRayTracingKernels() const {
    return true;
}

} // namespace NEO

#include "shared/source/release_helper/release_helper_common_xe2_hpg.inl"

template class NEO::ReleaseHelperHw<NEO::release>;
