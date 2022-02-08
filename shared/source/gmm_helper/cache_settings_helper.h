/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"

namespace NEO {
enum class AllocationType;

struct CacheSettingsHelper {
    static GMM_RESOURCE_USAGE_TYPE_ENUM getGmmUsageType(AllocationType allocationType, bool forceUncached);

  protected:
    static GMM_RESOURCE_USAGE_TYPE_ENUM getDefaultUsageTypeWithCachingEnabled(AllocationType allocationType);
    static GMM_RESOURCE_USAGE_TYPE_ENUM getDefaultUsageTypeWithCachingDisabled(AllocationType allocationType);
};
} // namespace NEO