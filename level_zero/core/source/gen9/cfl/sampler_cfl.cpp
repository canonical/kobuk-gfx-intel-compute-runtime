/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_cfl.h"
#include "shared/source/gen9/hw_info.h"

#include "level_zero/core/source/sampler/sampler_hw.inl"

namespace L0 {

template <>
struct SamplerProductFamily<IGFX_COFFEELAKE> : public SamplerCoreFamily<IGFX_GEN9_CORE> {
    using SamplerCoreFamily::SamplerCoreFamily;
};

static SamplerPopulateFactory<IGFX_COFFEELAKE, SamplerProductFamily<IGFX_COFFEELAKE>> populateCFL;

} // namespace L0
