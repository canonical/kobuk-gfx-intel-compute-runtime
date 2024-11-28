/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gfx_core_helper.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include <algorithm>

using namespace NEO;

struct KernelHelperMaxWorkGroupsTests : ::testing::Test {
    EngineGroupType engineType = EngineGroupType::compute;
    uint32_t simd = 8;
    uint32_t dssCount = 16;
    uint32_t availableSlm = 64 * MemoryConstants::kiloByte;
    uint32_t usedSlm = 0;
    uint32_t numberOfBarriers = 0;
    uint32_t workDim = 3;
    uint32_t grf = 128;
    uint32_t numSubdevices = 1;
    size_t lws[3] = {10, 10, 10};

    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get(), false, 1u);
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    }

    uint32_t getMaxWorkGroupCount() {
        KernelDescriptor descriptor = {};
        descriptor.kernelAttributes.simdSize = simd;
        descriptor.kernelAttributes.barrierCount = numberOfBarriers;
        descriptor.kernelAttributes.numGrfRequired = grf;

        auto hwInfo = rootDeviceEnvironment->getMutableHardwareInfo();
        hwInfo->gtSystemInfo.DualSubSliceCount = dssCount;
        hwInfo->capabilityTable.slmSize = (availableSlm / MemoryConstants::kiloByte) / dssCount;

        return KernelHelper::getMaxWorkGroupCount(*rootDeviceEnvironment, descriptor, numSubdevices, usedSlm, workDim, lws, engineType);
    }

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
};

TEST_F(KernelHelperMaxWorkGroupsTests, GivenNoBarriersOrSlmUsedWhenCalculatingMaxWorkGroupsCountThenResultIsCalculatedWithSimd) {
    auto &helper = rootDeviceEnvironment->getHelper<NEO::GfxCoreHelper>();

    uint32_t workGroupSize = static_cast<uint32_t>(lws[0] * lws[1] * lws[2]);
    uint32_t expected = helper.calculateAvailableThreadCount(*rootDeviceEnvironment->getHardwareInfo(), grf) / static_cast<uint32_t>(Math::divideAndRoundUp(workGroupSize, simd));

    expected = helper.adjustMaxWorkGroupCount(expected, EngineGroupType::compute, *rootDeviceEnvironment);
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

TEST_F(KernelHelperMaxWorkGroupsTests, GivenDebugFlagSetWhenGetMaxWorkGroupCountCalledThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideMaxWorkGroupCount.set(123);

    EXPECT_EQ(123u, getMaxWorkGroupCount());
}

TEST_F(KernelHelperMaxWorkGroupsTests, givenMultipleSubdevicesWenCalculatingMaxWorkGroupsCountTenMultiply) {
    auto &helper = rootDeviceEnvironment->getHelper<NEO::GfxCoreHelper>();

    auto baseCount = getMaxWorkGroupCount();

    numSubdevices = 4;

    auto countWithSubdevices = getMaxWorkGroupCount();

    if (helper.singleTileExecImplicitScalingRequired(true)) {
        EXPECT_EQ(baseCount, countWithSubdevices);
    } else {
        EXPECT_EQ(baseCount * numSubdevices, countWithSubdevices);
    }
}

HWTEST2_F(KernelHelperMaxWorkGroupsTests, GivenBarriersWhenCalculatingMaxWorkGroupsCountThenResultIsCalculatedWithRegardToBarriersCount, MatchAny) {
    NEO::RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*rootDeviceEnvironment);
    raii.mockProductHelper->isCooperativeEngineSupportedValue = false;
    lws[0] = 1;
    lws[1] = 0;
    lws[2] = 0;
    workDim = 1;
    numberOfBarriers = 0;

    numberOfBarriers = 16;

    auto &helper = rootDeviceEnvironment->getHelper<NEO::GfxCoreHelper>();
    auto maxBarrierCount = helper.getMaxBarrierRegisterPerSlice();

    auto expected = static_cast<uint32_t>(dssCount * (maxBarrierCount / numberOfBarriers));
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

HWTEST2_F(KernelHelperMaxWorkGroupsTests, GivenUsedSlmSizeWhenCalculatingMaxWorkGroupsCountThenResultIsCalculatedWithRegardToUsedSlmSize, MatchAny) {
    NEO::RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*rootDeviceEnvironment);
    raii.mockProductHelper->isCooperativeEngineSupportedValue = false;
    usedSlm = 0;
    lws[0] = 1;
    lws[1] = 0;
    lws[2] = 0;
    workDim = 1;

    usedSlm = 4 * MemoryConstants::kiloByte;

    auto expected = availableSlm / usedSlm;
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

HWTEST_F(KernelHelperMaxWorkGroupsTests, givenUsedSlmSizeWhenCalculatingMaxWorkGroupsCountThenAlignToDssSizeCalled) {
    auto raiiFactory = RAIIGfxCoreHelperFactory<MockGfxCoreHelperHw<FamilyType>>(*rootDeviceEnvironment);
    usedSlm = 4 * MemoryConstants::kiloByte;
    getMaxWorkGroupCount();
    EXPECT_EQ(raiiFactory.mockGfxCoreHelper->alignThreadGroupCountToDssSizeCalledTimes, 1u);
}
HWTEST_F(KernelHelperMaxWorkGroupsTests, givenBarriersWhenCalculatingMaxWorkGroupsCountThenAlignToDssSizeCalled) {
    auto raiiFactory = RAIIGfxCoreHelperFactory<MockGfxCoreHelperHw<FamilyType>>(*rootDeviceEnvironment);
    numberOfBarriers = 1;
    getMaxWorkGroupCount();
    EXPECT_EQ(raiiFactory.mockGfxCoreHelper->alignThreadGroupCountToDssSizeCalledTimes, 1u);
}

HWTEST_F(KernelHelperMaxWorkGroupsTests, givenZeroBarriersAndSlmNotUsedWhenCalculatingMaxWorkGroupsCountThenAlignToDssSizeNotCalled) {
    auto raiiFactory = RAIIGfxCoreHelperFactory<MockGfxCoreHelperHw<FamilyType>>(*rootDeviceEnvironment);
    numberOfBarriers = 0;
    usedSlm = 0;
    getMaxWorkGroupCount();
    EXPECT_EQ(raiiFactory.mockGfxCoreHelper->alignThreadGroupCountToDssSizeCalledTimes, 0u);
}

TEST_F(KernelHelperMaxWorkGroupsTests, GivenVariousValuesWhenCalculatingMaxWorkGroupsCountThenLowestResultIsAlwaysReturned) {
    auto &helper = rootDeviceEnvironment->getHelper<NEO::GfxCoreHelper>();

    engineType = EngineGroupType::cooperativeCompute;
    usedSlm = 1 * MemoryConstants::kiloByte;
    numberOfBarriers = 1;
    dssCount = 1;

    workDim = 1;
    lws[0] = simd;
    auto hwInfo = rootDeviceEnvironment->getMutableHardwareInfo();

    hwInfo->gtSystemInfo.ThreadCount = 1024;
    EXPECT_NE(1u, getMaxWorkGroupCount());

    numberOfBarriers = static_cast<uint32_t>(helper.getMaxBarrierRegisterPerSlice());
    EXPECT_EQ(1u, getMaxWorkGroupCount());

    numberOfBarriers = 1;
    EXPECT_NE(1u, getMaxWorkGroupCount());

    usedSlm = availableSlm;
    EXPECT_EQ(1u, getMaxWorkGroupCount());
}

using KernelHelperTest = Test<DeviceFixture>;

TEST_F(KernelHelperTest, GivenStatelessPrivateSizeGreaterThanGlobalSizeWhenCheckingIfThereIsEnaughSpaceThenOutOfMemReturned) {
    auto globalSize = pDevice->getDeviceInfo().globalMemSize;
    KernelDescriptor::KernelAttributes attributes = {};
    attributes.perHwThreadPrivateMemorySize = (static_cast<uint32_t>((globalSize + pDevice->getDeviceInfo().computeUnitsUsedForScratch) / pDevice->getDeviceInfo().computeUnitsUsedForScratch)) + 100;
    EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::outOfDeviceMemory);
}

TEST_F(KernelHelperTest, GivenScratchSizeGreaterThanGlobalSizeWhenCheckingIfThereIsEnaughSpaceThenOutOfMemReturned) {
    auto globalSize = pDevice->getDeviceInfo().globalMemSize;
    KernelDescriptor::KernelAttributes attributes = {};
    attributes.perThreadScratchSize[0] = (static_cast<uint32_t>((globalSize + pDevice->getDeviceInfo().computeUnitsUsedForScratch) / pDevice->getDeviceInfo().computeUnitsUsedForScratch)) + 100;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    auto &productHelper = pDevice->getProductHelper();
    if (attributes.perThreadScratchSize[0] > gfxCoreHelper.getMaxScratchSize(productHelper)) {
        EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::invalidKernel);
    } else {
        EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::outOfDeviceMemory);
    }
}

TEST_F(KernelHelperTest, GivenScratchPrivateSizeGreaterThanGlobalSizeWhenCheckingIfThereIsEnaughSpaceThenOutOfMemReturned) {
    auto globalSize = pDevice->getDeviceInfo().globalMemSize;
    KernelDescriptor::KernelAttributes attributes = {};
    attributes.perThreadScratchSize[1] = (static_cast<uint32_t>((globalSize + pDevice->getDeviceInfo().computeUnitsUsedForScratch) / pDevice->getDeviceInfo().computeUnitsUsedForScratch)) + 100;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    auto &productHelper = pDevice->getProductHelper();
    if (attributes.perThreadScratchSize[1] > gfxCoreHelper.getMaxScratchSize(productHelper)) {
        EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::invalidKernel);
    } else {
        EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::outOfDeviceMemory);
    }
}

TEST_F(KernelHelperTest, GivenScratchAndPrivateSizeLessThanGlobalSizeWhenCheckingIfThereIsEnaughSpaceThenSuccessReturned) {
    auto globalSize = pDevice->getDeviceInfo().globalMemSize;
    KernelDescriptor::KernelAttributes attributes = {};
    auto size = (static_cast<uint32_t>((globalSize + pDevice->getDeviceInfo().computeUnitsUsedForScratch) / pDevice->getDeviceInfo().computeUnitsUsedForScratch)) - 100;
    attributes.perHwThreadPrivateMemorySize = size;
    auto &gfxCoreHelper = pDevice->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    auto &productHelper = pDevice->getProductHelper();
    uint32_t maxScratchSize = gfxCoreHelper.getMaxScratchSize(productHelper);
    attributes.perThreadScratchSize[0] = (size > maxScratchSize) ? maxScratchSize : size;
    attributes.perThreadScratchSize[1] = (size > maxScratchSize) ? maxScratchSize : size;
    EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::success);
}

TEST_F(KernelHelperTest, GivenScratchSizeGreaterThanMaxScratchSizeWhenCheckingIfThereIsEnaughSpaceThenInvalidKernelIsReturned) {
    KernelDescriptor::KernelAttributes attributes = {};
    auto &gfxCoreHelper = pDevice->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    auto &productHelper = pDevice->getProductHelper();
    uint32_t maxScratchSize = gfxCoreHelper.getMaxScratchSize(productHelper);
    attributes.perHwThreadPrivateMemorySize = 0x10;
    attributes.perThreadScratchSize[0] = maxScratchSize + 1;
    attributes.perThreadScratchSize[1] = 0x10;
    EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::invalidKernel);
}

TEST_F(KernelHelperTest, GivenScratchPrivateSizeGreaterThanMaxScratchSizeWhenCheckingIfThereIsEnaughSpaceThenInvalidKernelIsReturned) {
    KernelDescriptor::KernelAttributes attributes = {};
    auto &gfxCoreHelper = pDevice->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    auto &productHelper = pDevice->getProductHelper();
    uint32_t maxScratchSize = gfxCoreHelper.getMaxScratchSize(productHelper);
    attributes.perHwThreadPrivateMemorySize = 0x10;
    attributes.perThreadScratchSize[0] = 0x10;
    attributes.perThreadScratchSize[1] = maxScratchSize + 1;
    EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::invalidKernel);
}

TEST_F(KernelHelperTest, GivenScratchAndEqualsZeroWhenCheckingIfThereIsEnaughSpaceThenSuccessIsReturned) {
    KernelDescriptor::KernelAttributes attributes = {};
    attributes.perHwThreadPrivateMemorySize = 0;
    attributes.perThreadScratchSize[0] = 0;
    attributes.perThreadScratchSize[1] = 0;
    EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::success);
}

TEST_F(KernelHelperTest, GivenScratchEqualsZeroAndPrivetGreaterThanZeroWhenCheckingIfThereIsEnaughSpaceThenSuccessIsReturned) {
    KernelDescriptor::KernelAttributes attributes = {};
    attributes.perHwThreadPrivateMemorySize = 0x10;
    attributes.perThreadScratchSize[0] = 0;
    attributes.perThreadScratchSize[1] = 0;
    EXPECT_EQ(KernelHelper::checkIfThereIsSpaceForScratchOrPrivate(attributes, pDevice), KernelHelper::ErrorCode::success);
}

TEST_F(KernelHelperTest, GivenNoPtrByValueWhenCheckingIsAnyArgumentPtrByValueThenFalseIsReturned) {
    KernelDescriptor kernelDescriptor;
    auto pointerArg = ArgDescriptor(ArgDescriptor::argTPointer);

    auto valueArg = ArgDescriptor(ArgDescriptor::argTValue);
    ArgDescValue::Element element;
    element.isPtr = false;
    valueArg.as<ArgDescValue>().elements.push_back(element);

    kernelDescriptor.payloadMappings.explicitArgs.push_back(pointerArg);
    kernelDescriptor.payloadMappings.explicitArgs.push_back(valueArg);
    EXPECT_FALSE(KernelHelper::isAnyArgumentPtrByValue(kernelDescriptor));
}

TEST_F(KernelHelperTest, GivenPtrByValueWhenCheckingIsAnyArgumentPtrByValueThenTrueIsReturned) {
    KernelDescriptor kernelDescriptor;
    auto pointerArg = ArgDescriptor(ArgDescriptor::argTPointer);

    auto valueArg = ArgDescriptor(ArgDescriptor::argTValue);
    ArgDescValue::Element element;
    element.isPtr = true;
    valueArg.as<ArgDescValue>().elements.push_back(element);

    kernelDescriptor.payloadMappings.explicitArgs.push_back(pointerArg);
    kernelDescriptor.payloadMappings.explicitArgs.push_back(valueArg);
    EXPECT_TRUE(KernelHelper::isAnyArgumentPtrByValue(kernelDescriptor));
}

TEST_F(KernelHelperTest, GivenThreadGroupCountWhenSyncBufferCreatedThenAllocationIsRetrieved) {
    const size_t requestedNumberOfWorkgroups = 4;
    auto offset = KernelHelper::getSyncBufferSize(requestedNumberOfWorkgroups);

    auto pair = KernelHelper::getSyncBufferAllocationOffset(*pDevice, requestedNumberOfWorkgroups);
    auto allocation = pair.first;

    EXPECT_EQ(0u, pair.second);
    EXPECT_NE(nullptr, allocation);

    pair = KernelHelper::getSyncBufferAllocationOffset(*pDevice, requestedNumberOfWorkgroups);
    EXPECT_EQ(offset, pair.second);
    EXPECT_EQ(allocation, pair.first);
}

TEST_F(KernelHelperTest, GivenThreadGroupCountAndRegionSizeWhenRegionBarrierCreatedThenAllocationIsRetrieved) {
    const size_t requestedNumberOfWorkgroups = 4;
    const size_t localRegionSize = 2;
    auto offset = KernelHelper::getRegionGroupBarrierSize(requestedNumberOfWorkgroups, localRegionSize);

    auto pair = KernelHelper::getRegionGroupBarrierAllocationOffset(*pDevice, requestedNumberOfWorkgroups, localRegionSize);
    auto allocation = pair.first;

    EXPECT_EQ(0u, pair.second);
    EXPECT_NE(nullptr, allocation);

    pair = KernelHelper::getRegionGroupBarrierAllocationOffset(*pDevice, requestedNumberOfWorkgroups, localRegionSize);
    EXPECT_EQ(offset, pair.second);
    EXPECT_EQ(allocation, pair.first);
}
