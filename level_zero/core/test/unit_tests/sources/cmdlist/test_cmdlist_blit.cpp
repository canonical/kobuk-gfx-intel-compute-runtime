/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/register_offsets.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForMemFill : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    using BaseClass = WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>;

    MockCommandListForMemFill() : BaseClass() {}

    using BaseClass::getAllocationOffsetForAppendBlitFill;

    AlignedAllocationData getAlignedAllocationData(L0::Device *device, const void *buffer, uint64_t bufferSize, bool allowHostCopy, bool copyOffload) override {
        return {0, 0, nullptr, true};
    }
    ze_result_t appendMemoryCopyBlit(uintptr_t dstPtr,
                                     NEO::GraphicsAllocation *dstPtrAlloc,
                                     uint64_t dstOffset, uintptr_t srcPtr,
                                     NEO::GraphicsAllocation *srcPtrAlloc,
                                     uint64_t srcOffset,
                                     uint64_t size, Event *signalEvent) override {
        appendMemoryCopyBlitCalledTimes++;
        return ZE_RESULT_SUCCESS;
    }
    uint32_t appendMemoryCopyBlitCalledTimes = 0;
};
class MockDriverHandle : public L0::DriverHandleImp {
  public:
    bool findAllocationDataForRange(const void *buffer,
                                    size_t size,
                                    NEO::SvmAllocationData *&allocData) override {
        mockAllocation.reset(new NEO::MockGraphicsAllocation(rootDeviceIndex, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount));
        data.gpuAllocations.addAllocation(mockAllocation.get());
        allocData = &data;
        return true;
    }
    const uint32_t rootDeviceIndex = 0u;
    std::unique_ptr<NEO::GraphicsAllocation> mockAllocation;
    NEO::SvmAllocationData data{rootDeviceIndex};
};

using AppendMemoryCopyTests = Test<AppendMemoryCopyFixture>;

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWhenAppenBlitFillCalledWithLargePatternSizeThenMemCopyWasCalled) {
    MockCommandListForMemFill<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    uint64_t pattern[4] = {1, 2, 3, 4};
    void *ptr = reinterpret_cast<void *>(0x1234);
    auto ret = cmdList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, ret);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWhenAppenBlitFillToNotDeviceMemThenInvalidArgumentReturned) {
    MockCommandListForMemFill<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    uint8_t pattern = 1;
    void *ptr = reinterpret_cast<void *>(0x1234);
    auto ret = cmdList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(ret, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

using MemFillPlatforms = IsGen12LP;

HWTEST2_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWhenAppenBlitFillThenCopyBltIsProgrammed, MemFillPlatforms) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    MockCommandListForMemFill<FamilyType::gfxCoreFamily> commandList;
    MockDriverHandle driverHandleMock;
    NEO::DeviceVector neoDevices;
    neoDevices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandleMock.initialize(std::move(neoDevices));
    device->setDriverHandle(&driverHandleMock);
    commandList.initialize(device, NEO::EngineGroupType::copy, 0u);
    uint16_t pattern = 1;
    void *ptr = reinterpret_cast<void *>(0x1234);
    commandList.appendMemoryFill(ptr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 0x1000, nullptr, 0, nullptr, copyParams);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList.getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    device->setDriverHandle(driverHandle.get());
}

HWTEST_F(AppendMemoryCopyTests, givenExternalHostPointerAllocationWhenPassedToAppendBlitFillThenProgramDestinationAddressCorrectly) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;

    L0::Device *device = driverHandle->devices[0];

    size_t size = 1024;
    auto hostPointer = std::make_unique<uint8_t[]>(size);
    auto ret = driverHandle->importExternalPointer(hostPointer.get(), MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto gpuAllocation = device->getDriverHandle()->findHostPointerAllocation(hostPointer.get(), size, 0);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(NEO::AllocationType::externalHostPtr, gpuAllocation->getAllocationType());

    MockCommandListForMemFill<FamilyType::gfxCoreFamily> commandList;
    commandList.initialize(device, NEO::EngineGroupType::copy, 0u);

    uint32_t pattern = 1;
    ze_result_t result = commandList.appendMemoryFill(hostPointer.get(), reinterpret_cast<void *>(&pattern), sizeof(pattern), size, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList.getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList.getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    auto cmd = genCmdCast<XY_COLOR_BLT *>(*itor);
    uint64_t offset = commandList.getAllocationOffsetForAppendBlitFill(hostPointer.get(), *gpuAllocation);
    EXPECT_EQ(cmd->getDestinationBaseAddress(), ptrOffset(gpuAllocation->getGpuAddress(), offset));

    ret = driverHandle->releaseImportedPointer(hostPointer.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListAndHostPointersWhenMemoryCopyCalledThenPipeControlWithDcFlushAddedIsNotAddedAfterBlitCopy) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, copyParams);

    auto &commandContainer = commandList->getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());

    EXPECT_EQ(genCmdList.end(), itor);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListAndHostPointersWhenMemoryCopyRegionCalledThenPipeControlWithDcFlushAddedIsNotAddedAfterBlitCopy) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, copyParams);

    auto &commandContainer = commandList->getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);

    itor = find<PIPE_CONTROL *>(++itor, genCmdList.end());

    EXPECT_EQ(genCmdList.end(), itor);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListThenDcFlushIsNotAddedAfterBlitCopy) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    uintptr_t srcPtr = 0x5001;
    uintptr_t dstPtr = 0x7001;
    uint64_t srcOffset = 0x101;
    uint64_t dstOffset = 0x201;
    uint64_t copySize = 0x301;
    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(srcPtr), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(dstPtr), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    commandList->appendMemoryCopyBlit(ptrOffset(dstPtr, dstOffset), &mockAllocationDst, 0, ptrOffset(srcPtr, srcOffset), &mockAllocationSrc, 0, copySize, nullptr);

    auto &commandContainer = commandList->getCmdContainer();
    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(genCmdList.begin(), genCmdList.end());
    ASSERT_NE(genCmdList.end(), itor);
    auto cmd = genCmdCast<XY_COPY_BLT *>(*itor);
    EXPECT_EQ(cmd->getDestinationBaseAddress(), ptrOffset(dstPtr, dstOffset));
    EXPECT_EQ(cmd->getSourceBaseAddress(), ptrOffset(srcPtr, srcOffset));
}

HWTEST_F(AppendMemoryCopyTests, givenCopyCommandListWhenTimestampPassedToMemoryCopyRegionBlitThenTimeStampRegistersAreAdded) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    AlignedAllocationData srcAllocationData = {mockAllocationSrc.gpuAddress, 0, &mockAllocationSrc, false};
    AlignedAllocationData dstAllocationData = {mockAllocationDst.gpuAddress, 0, &mockAllocationDst, false};
    commandList->appendMemoryCopyBlitRegion(&srcAllocationData, &dstAllocationData, srcRegion, dstRegion, {0, 0, 0}, 0, 0, 0, 0, 0, 0, event.get(), 0, nullptr, false, false);
    GenCmdList cmdList;

    auto baseAddr = event->getGpuAddress(device);
    auto contextStartOffset = event->getContextStartOffset();
    auto globalStartOffset = event->getGlobalStartOffset();
    auto contextEndOffset = event->getContextEndOffset();
    auto globalEndOffset = event->getGlobalEndOffset();

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::globalTimestampLdw);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalStartOffset));
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextStartOffset));
    itor++;
    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::globalTimestampLdw);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalEndOffset));
    itor++;
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextEndOffset));
    itor++;
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyCommandListWhenTimestampPassedToImageCopyBlitThenTimeStampRegistersAreAdded) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    commandList->useAdditionalBlitProperties = false;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    CmdListMemoryCopyParams copyParams = {};
    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, event.get(), 0, nullptr, copyParams);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::bcs0Base + RegisterOffsets::globalTimestampLdw);
}

HWTEST2_F(AppendMemoryCopyTests, givenWaitWhenWhenAppendBlitCalledThenProgramSemaphore, MatchAny) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto cmdStream = commandList->getCmdContainer().getCommandStream();
    auto offset = cmdStream->getUsed();

    CmdListMemoryCopyParams copyParams = {};

    {
        commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, event.get(), 0, nullptr, copyParams);
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(cmdList.end(), itor);
    }

    {
        offset = cmdStream->getUsed();
        auto eventHandle = event->toHandle();

        commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, nullptr, 1, &eventHandle, copyParams);
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));
        auto itor = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        EXPECT_NE(cmdList.end(), itor);
    }
}

using ImageSupport = IsGen12LP;
HWTEST2_F(AppendMemoryCopyTests, givenCopyCommandListWhenCopyFromImagBlitThenCommandAddedToStream, ImageSupport) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendImageCopyRegion(imageHWDst->toHandle(), imageHWSrc->toHandle(), nullptr, nullptr, nullptr, 0, nullptr, copyParams);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

using AppendMemoryCopyFromContext = AppendMemoryCopyTests;

HWTEST_F(AppendMemoryCopyFromContext, givenCommandListThenUpOnPerformingAppendMemoryCopyFromContextSuccessIsReturned) {

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    auto result = commandList->appendMemoryCopyFromContext(dstPtr, nullptr, srcPtr, 8, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

struct IsAtLeastXeHpCoreAndNotXe2HpgCoreWith2DArrayImageSupport {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsAtLeastGfxCore<IGFX_XE_HP_CORE>::isMatched<productFamily>() && !IsXe2HpgCore::isMatched<productFamily>() && NEO::HwMapper<productFamily>::GfxProduct::supportsSampler;
    }
};
HWTEST2_F(AppendMemoryCopyTests, givenCopyCommandListWhenTiled1DArrayImagePassedToImageCopyBlitThenTransformedTo2DArrayCopy, IsAtLeastXeHpCoreAndNotXe2HpgCoreWith2DArrayImageSupport) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);

    auto gmmSrc = std::make_unique<MockGmm>(device->getNEODevice()->getGmmHelper());
    auto resourceInfoSrc = static_cast<MockGmmResourceInfo *>(gmmSrc->gmmResourceInfo.get());
    resourceInfoSrc->getResourceFlags()->Info.Tile64 = 1;
    resourceInfoSrc->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
    resourceInfoSrc->mockResourceCreateParams.ArraySize = 8;

    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    mockAllocationSrc.setGmm(gmmSrc.get(), 0);
    mockAllocationDst.setGmm(gmmSrc.get(), 0);

    size_t arrayLevels = 8;
    size_t depth = 1;
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 4, 4, 4, 4, 1, {1, arrayLevels, depth}, {1, arrayLevels, depth}, {1, arrayLevels, depth}, nullptr, 0, nullptr, copyParams);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<XY_BLOCK_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<XY_BLOCK_COPY_BLT *>(*itor);
    EXPECT_EQ(cmd->getSourceSurfaceDepth(), arrayLevels);
    EXPECT_EQ(cmd->getSourceSurfaceHeight(), depth);
}

HWTEST2_F(AppendMemoryCopyTests, givenCopyCommandListWhenNotTiled1DArrayImagePassedToImageCopyBlitThenNotTransformedTo2DArrayCopy, IsAtLeastXeHpCoreAndNotXe2HpgCoreWith2DArrayImageSupport) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);

    auto gmmSrc = std::make_unique<MockGmm>(device->getNEODevice()->getGmmHelper());
    auto resourceInfoSrc = static_cast<MockGmmResourceInfo *>(gmmSrc->gmmResourceInfo.get());
    resourceInfoSrc->getResourceFlags()->Info.Tile64 = 0;
    resourceInfoSrc->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
    resourceInfoSrc->mockResourceCreateParams.ArraySize = 8;

    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    mockAllocationSrc.setGmm(gmmSrc.get(), 0);
    mockAllocationDst.setGmm(gmmSrc.get(), 0);

    size_t arrayLevels = 8;
    size_t depth = 1;
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, arrayLevels, depth}, {1, arrayLevels, depth}, {1, arrayLevels, depth}, nullptr, 0, nullptr, copyParams);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<XY_BLOCK_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<XY_BLOCK_COPY_BLT *>(*itor);
    EXPECT_EQ(cmd->getSourceSurfaceDepth(), depth);
    EXPECT_EQ(cmd->getSourceSurfaceHeight(), arrayLevels);
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForAdditionalBlitProperties : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    using BaseClass = WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>;
    using BaseClass::setAdditionalBlitProperties;
    using BaseClass::useAdditionalBlitProperties;
};

HWTEST_F(AppendMemoryCopyTests, givenBlitPropertiesWhenCallingSetAdditionalBlitPropertiesThenSyncPropertiesExtRemainsUnchanged) {
    NEO::BlitProperties blitProperties{}, blitProperties2{}, blitPropertiesExpected{};
    EncodePostSyncArgs &postSyncArgs = blitProperties.blitSyncProperties.postSyncArgs;
    EncodePostSyncArgs &postSyncArgs2 = blitProperties2.blitSyncProperties.postSyncArgs;
    EncodePostSyncArgs &postSyncArgsExpected = blitPropertiesExpected.blitSyncProperties.postSyncArgs;

    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties<FamilyType::gfxCoreFamily>>();
    EXPECT_FALSE(commandList->useAdditionalBlitProperties);
    commandList->setAdditionalBlitProperties(blitProperties, nullptr, false);
    EXPECT_EQ(postSyncArgs.isTimestampEvent, postSyncArgsExpected.isTimestampEvent);
    EXPECT_EQ(postSyncArgs.postSyncImmValue, postSyncArgsExpected.postSyncImmValue);
    EXPECT_EQ(postSyncArgs.interruptEvent, postSyncArgsExpected.interruptEvent);
    EXPECT_EQ(postSyncArgs.eventAddress, postSyncArgsExpected.eventAddress);

    commandList->useAdditionalBlitProperties = true;
    commandList->setAdditionalBlitProperties(blitProperties2, nullptr, false);
    EXPECT_EQ(postSyncArgs2.isTimestampEvent, postSyncArgsExpected.isTimestampEvent);
    EXPECT_EQ(postSyncArgs2.postSyncImmValue, postSyncArgsExpected.postSyncImmValue);
    EXPECT_EQ(postSyncArgs2.interruptEvent, postSyncArgsExpected.interruptEvent);
    EXPECT_EQ(postSyncArgs2.eventAddress, postSyncArgsExpected.eventAddress);
    EXPECT_EQ(nullptr, postSyncArgs2.inOrderExecInfo);
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandListForAdditionalBlitProperties2 : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
  public:
    using BaseClass = WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>;
    using BaseClass::useAdditionalBlitProperties;
    void setAdditionalBlitProperties(NEO::BlitProperties &blitProperties, Event *signalEvent, bool useAdditionalTimestamp) override {
        additionalBlitPropertiesCalled++;
        BaseClass::setAdditionalBlitProperties(blitProperties, signalEvent, useAdditionalTimestamp);
    }
    void appendSignalInOrderDependencyCounter(Event *signalEvent, bool copyOffloadOperation, bool stall, bool textureFlushRequired) override {
        appendSignalInOrderDependencyCounterCalled++;
        BaseClass::appendSignalInOrderDependencyCounter(signalEvent, copyOffloadOperation, stall, textureFlushRequired);
    }
    uint32_t additionalBlitPropertiesCalled = 0;
    uint32_t appendSignalInOrderDependencyCounterCalled = 0;
};

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendMemoryCopyBlitThenAdditionalBlitPropertiesCalled) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);

    ze_device_mem_alloc_desc_t deviceDesc = {};

    void *srcBuffer = nullptr;
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &srcBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, srcBuffer);

    void *dstBuffer = nullptr;
    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);

    commandList->useAdditionalBlitProperties = false;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendMemoryCopy(dstBuffer, srcBuffer, 4906u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    commandList->appendMemoryCopy(dstBuffer, srcBuffer, 4906u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(3u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyCopyBlt, commandList->inOrderPatchCmds[2].patchCmdType);

    context->freeMem(dstBuffer);
    context->freeMem(srcBuffer);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendMemoryCopyBlitThenInOrderPatchCmdsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0);

    ze_device_mem_alloc_desc_t deviceDesc = {};

    void *srcBuffer = nullptr;
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &srcBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, srcBuffer);

    void *dstBuffer = nullptr;
    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);

    commandList->useAdditionalBlitProperties = true;
    CmdListMemoryCopyParams copyParams = {};
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendMemoryCopy(dstBuffer, srcBuffer, 4906u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());

    context->freeMem(dstBuffer);
    context->freeMem(srcBuffer);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendMemoryCopyImageBlitThenAdditionalBlitPropertiesCalled) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);
    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    commandList->useAdditionalBlitProperties = false;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());

    commandList->useAdditionalBlitProperties = true;

    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(3u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyBlockCopyBlt, commandList->inOrderPatchCmds.back().patchCmdType);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendMemoryCopyImageBlitThenInOrderPatchCmdsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0);
    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendMemoryCopyRegionThenAdditionalBlitPropertiesCalled) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);
    void *srcBuffer = reinterpret_cast<void *>(0x1234);
    void *dstBuffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    CmdListMemoryCopyParams copyParams = {};

    commandList->useAdditionalBlitProperties = false;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendMemoryCopyRegion(dstBuffer, &dr, width, 0,
                                        srcBuffer, &sr, width, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());

    commandList->useAdditionalBlitProperties = true;
    commandList->appendMemoryCopyRegion(dstBuffer, &dr, width, 0,
                                        srcBuffer, &sr, width, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(3u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyCopyBlt, commandList->inOrderPatchCmds[2].patchCmdType);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendMemoryCopyRegionThenInOrderPatchCmdsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0);
    void *srcBuffer = reinterpret_cast<void *>(0x1234);
    void *dstBuffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    CmdListMemoryCopyParams copyParams = {};

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendMemoryCopyRegion(dstBuffer, &dr, width, 0,
                                        srcBuffer, &sr, width, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendBlitFillThenAdditionalBlitPropertiesCalled) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);

    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    uint32_t one = 1u;
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);
    CmdListMemoryCopyParams copyParams = {};

    commandList->useAdditionalBlitProperties = false;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint8_t), 4096u, nullptr, 0, nullptr, copyParams);

    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());

    commandList->useAdditionalBlitProperties = true;
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint8_t), 4096u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(3u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::memSet, commandList->inOrderPatchCmds[2].patchCmdType);
    context->freeMem(dstBuffer);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendBlitWithTwoBytesPatternFillThenAdditionalBlitPropertiesCalled) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);

    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    uint32_t one = 1u;
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);
    CmdListMemoryCopyParams copyParams = {};

    commandList->maxFillPaternSizeForCopyEngine = 4;

    commandList->useAdditionalBlitProperties = false;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint16_t), 4096u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());

    commandList->useAdditionalBlitProperties = true;
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint16_t), 4096u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(1u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(3u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyColorBlt, commandList->inOrderPatchCmds[2].patchCmdType);
    context->freeMem(dstBuffer);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyRegularCommandListWithUseAdditionalBlitPropertiesWhenCallingAppendBlitFillThenInOrderPatchCmdsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0);

    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    uint32_t one = 1u;
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);
    CmdListMemoryCopyParams copyParams = {};

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint8_t), 4096u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    context->freeMem(dstBuffer);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenPatchingCommandsAfterCallingMemoryCopyRegionThenCommandsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);
    void *srcBuffer = reinterpret_cast<void *>(0x1234);
    void *dstBuffer = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};
    CmdListMemoryCopyParams copyParams = {};

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendMemoryCopyRegion(dstBuffer, &dr, width, 0,
                                        srcBuffer, &sr, width, 0, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyCopyBlt, commandList->inOrderPatchCmds[0].patchCmdType);

    commandList->enablePatching(0);
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COPY_BLT = typename GfxFamily::XY_COPY_BLT;
    auto &inOrderPatchCmd = commandList->inOrderPatchCmds[0];
    EXPECT_NE(nullptr, inOrderPatchCmd.cmd1);
    EXPECT_EQ(nullptr, inOrderPatchCmd.cmd2);
    XY_COPY_BLT copyBlt = *reinterpret_cast<XY_COPY_BLT *>(inOrderPatchCmd.cmd1);
    inOrderPatchCmd.patch(3);
    XY_COPY_BLT *modifiedBlt = reinterpret_cast<XY_COPY_BLT *>(inOrderPatchCmd.cmd1);
    EXPECT_EQ(memcmp(modifiedBlt, &copyBlt, sizeof(XY_COPY_BLT)), 0);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenPatchingCommandsAfterCallingMemoryCopyThenCommandsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);
    NEO::MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                                  reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                  MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    CmdListMemoryCopyParams copyParams = {};
    commandList->appendCopyImageBlit(&mockAllocationDst, &mockAllocationSrc, {0, 0, 0}, {0, 0, 0}, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyBlockCopyBlt, commandList->inOrderPatchCmds[0].patchCmdType);

    commandList->enablePatching(0);
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_BLOCK_COPY_BLT = typename GfxFamily::XY_BLOCK_COPY_BLT;
    auto &inOrderPatchCmd = commandList->inOrderPatchCmds[0];
    EXPECT_NE(nullptr, inOrderPatchCmd.cmd1);
    EXPECT_EQ(nullptr, inOrderPatchCmd.cmd2);
    XY_BLOCK_COPY_BLT copyBlt = *reinterpret_cast<XY_BLOCK_COPY_BLT *>(inOrderPatchCmd.cmd1);
    inOrderPatchCmd.patch(3);
    XY_BLOCK_COPY_BLT *modifiedBlt = reinterpret_cast<XY_BLOCK_COPY_BLT *>(inOrderPatchCmd.cmd1);
    EXPECT_EQ(memcmp(modifiedBlt, &copyBlt, sizeof(XY_BLOCK_COPY_BLT)), 0);
}

HWTEST_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenPatchingCommandsAfterCallingMemoryFillWithTwoBytesPatternThenCommandsRemainsTheSame) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);

    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    uint32_t one = 1u;
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);
    CmdListMemoryCopyParams copyParams = {};

    commandList->maxFillPaternSizeForCopyEngine = 4;

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint16_t), 4096u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::xyColorBlt, commandList->inOrderPatchCmds[0].patchCmdType);

    commandList->enablePatching(0);
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    auto &inOrderPatchCmd = commandList->inOrderPatchCmds[0];
    EXPECT_NE(nullptr, inOrderPatchCmd.cmd1);
    EXPECT_EQ(nullptr, inOrderPatchCmd.cmd2);
    XY_COLOR_BLT copyBlt = *reinterpret_cast<XY_COLOR_BLT *>(inOrderPatchCmd.cmd1);
    inOrderPatchCmd.patch(3);
    XY_COLOR_BLT *modifiedBlt = reinterpret_cast<XY_COLOR_BLT *>(inOrderPatchCmd.cmd1);
    EXPECT_EQ(memcmp(modifiedBlt, &copyBlt, sizeof(XY_COLOR_BLT)), 0);

    context->freeMem(dstBuffer);
}

HWTEST2_F(AppendMemoryCopyTests, givenCopyOnlyCommandListWithUseAdditionalBlitPropertiesWhenPatchingCommandsAfterCallingMemoryFillWithSingleBytePatternThenCommandsRemainsTheSame, IsAtLeastXeHpcCore) {
    auto commandList = std::make_unique<MockCommandListForAdditionalBlitProperties2<FamilyType::gfxCoreFamily>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, ZE_COMMAND_LIST_FLAG_IN_ORDER);

    void *dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstBuffer);
    uint32_t one = 1u;
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, dstBuffer);
    CmdListMemoryCopyParams copyParams = {};

    commandList->maxFillPaternSizeForCopyEngine = 4;

    commandList->useAdditionalBlitProperties = true;
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(0u, commandList->inOrderPatchCmds.size());
    commandList->appendBlitFill(dstBuffer, &one, sizeof(uint8_t), 4096u, nullptr, 0, nullptr, copyParams);
    EXPECT_EQ(1u, commandList->additionalBlitPropertiesCalled);
    EXPECT_EQ(0u, commandList->appendSignalInOrderDependencyCounterCalled);
    EXPECT_EQ(1u, commandList->inOrderPatchCmds.size());
    EXPECT_EQ(InOrderPatchCommandHelpers::PatchCmdType::memSet, commandList->inOrderPatchCmds[0].patchCmdType);

    commandList->enablePatching(0);
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using MEM_SET = typename GfxFamily::MEM_SET;
    auto &inOrderPatchCmd = commandList->inOrderPatchCmds[0];
    EXPECT_NE(nullptr, inOrderPatchCmd.cmd1);
    EXPECT_EQ(nullptr, inOrderPatchCmd.cmd2);
    MEM_SET copyBlt = *reinterpret_cast<MEM_SET *>(inOrderPatchCmd.cmd1);
    inOrderPatchCmd.patch(3);
    MEM_SET *modifiedBlt = reinterpret_cast<MEM_SET *>(inOrderPatchCmd.cmd1);
    EXPECT_EQ(memcmp(modifiedBlt, &copyBlt, sizeof(MEM_SET)), 0);

    context->freeMem(dstBuffer);
}

} // namespace ult
} // namespace L0
