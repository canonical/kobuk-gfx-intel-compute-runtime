/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/fixtures/mock_execution_environment_gmm_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
using GmmTests = Test<MockExecutionEnvironmentGmmFixture>;
TEST_F(GmmTests, givenResourceUsageTypesCacheableWhenCreateGmmAndFlagEnableCpuCacheForResourcesSetThenFlagCacheableIsTrue) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCpuCacheForResources.set(1);
    StorageInfo storageInfo{};
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = false;
    gmmRequirements.preferCompressed = false;
    for (auto resourceUsageType : {GMM_RESOURCE_USAGE_OCL_IMAGE,
                                   GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER,
                                   GMM_RESOURCE_USAGE_OCL_BUFFER_CONST,
                                   GMM_RESOURCE_USAGE_OCL_BUFFER}) {
        auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 0, 0, resourceUsageType, storageInfo, gmmRequirements);
        EXPECT_FALSE(CacheSettingsHelper::preferNoCpuAccess(resourceUsageType, getGmmHelper()->getRootDeviceEnvironment()));
        EXPECT_TRUE(gmm->resourceParams.Flags.Info.Cacheable);
    }
}

TEST_F(GmmTests, givenResourceUsageTypesCacheableWhenCreateGmmAndFlagEnableCpuCacheForResourcesNotSetThenFlagCacheableIsRelatedToValueFromHelperIsCachingOnCpuAvailable) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCpuCacheForResources.set(0);
    StorageInfo storageInfo{};
    auto releaseHelper = getGmmHelper()->getRootDeviceEnvironment().getReleaseHelper();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = false;
    gmmRequirements.preferCompressed = false;
    for (auto resourceUsageType : {GMM_RESOURCE_USAGE_OCL_IMAGE,
                                   GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER,
                                   GMM_RESOURCE_USAGE_OCL_BUFFER_CONST,
                                   GMM_RESOURCE_USAGE_OCL_BUFFER}) {
        auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 0, 0, resourceUsageType, storageInfo, gmmRequirements);
        bool noCpuAccessPreference = releaseHelper ? !releaseHelper->isCachingOnCpuAvailable() : false;
        EXPECT_EQ(noCpuAccessPreference, CacheSettingsHelper::preferNoCpuAccess(resourceUsageType, getGmmHelper()->getRootDeviceEnvironment()));
        EXPECT_EQ(noCpuAccessPreference, gmm->getPreferNoCpuAccess());
    }
}

TEST_F(GmmTests, givenResourceUsageTypesUnCachedWhenGreateGmmThenFlagCachcableIsFalse) {
    StorageInfo storageInfo{};
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = false;
    gmmRequirements.preferCompressed = false;
    for (auto resourceUsageType : {GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC,
                                   GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED,
                                   GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED}) {
        auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 0, 0, resourceUsageType, storageInfo, gmmRequirements);
        EXPECT_FALSE(gmm->resourceParams.Flags.Info.Cacheable);
    }
}

HWTEST_F(GmmTests, givenIsResourceCacheableOnCpuWhenWslFlagThenReturnProperValue) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCpuCacheForResources.set(false);
    StorageInfo storageInfo{};
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment->rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->isWddmOnLinuxEnable = true;

    GMM_RESOURCE_USAGE_TYPE_ENUM gmmResourceUsageType = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = false;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 0, 0, gmmResourceUsageType, storageInfo, gmmRequirements);
    EXPECT_FALSE(CacheSettingsHelper::preferNoCpuAccess(gmmResourceUsageType, *rootDeviceEnvironment));
    EXPECT_TRUE(gmm->resourceParams.Flags.Info.Cacheable);

    gmmResourceUsageType = GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
    gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 0, 0, gmmResourceUsageType, storageInfo, gmmRequirements);
    EXPECT_FALSE(CacheSettingsHelper::preferNoCpuAccess(gmmResourceUsageType, *rootDeviceEnvironment));
    EXPECT_FALSE(gmm->resourceParams.Flags.Info.Cacheable);
}

HWTEST_F(GmmTests, givenVariousResourceUsageTypeWhenCreateGmmThenFlagCacheableIsSetProperly) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCpuCacheForResources.set(false);
    StorageInfo storageInfo{};
    auto releaseHelper = executionEnvironment->rootDeviceEnvironments[0]->getReleaseHelper();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = false;
    gmmRequirements.preferCompressed = false;

    for (auto regularResourceUsageType : {GMM_RESOURCE_USAGE_OCL_IMAGE,
                                          GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER,
                                          GMM_RESOURCE_USAGE_OCL_BUFFER_CONST,
                                          GMM_RESOURCE_USAGE_OCL_BUFFER}) {
        auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 0, 0, regularResourceUsageType, storageInfo, gmmRequirements);
        if (!releaseHelper) {
            EXPECT_TRUE(gmm->resourceParams.Flags.Info.Cacheable);
        } else {
            EXPECT_EQ(releaseHelper->isCachingOnCpuAvailable(), gmm->resourceParams.Flags.Info.Cacheable);
        }
    }

    for (auto cpuAccessibleResourceUsageType : {GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER}) {
        auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 0, 0, cpuAccessibleResourceUsageType, storageInfo, gmmRequirements);
        EXPECT_TRUE(gmm->resourceParams.Flags.Info.Cacheable);
    }

    for (auto uncacheableResourceUsageType : {GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC,
                                              GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED,
                                              GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED}) {
        auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 0, 0, uncacheableResourceUsageType, storageInfo, gmmRequirements);
        EXPECT_FALSE(gmm->resourceParams.Flags.Info.Cacheable);
    }
    {
        gmmRequirements.overriderCacheable.enableOverride = true;
        gmmRequirements.overriderCacheable.value = true;
        auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);
        EXPECT_TRUE(gmm->resourceParams.Flags.Info.Cacheable);
    }
}

} // namespace NEO
