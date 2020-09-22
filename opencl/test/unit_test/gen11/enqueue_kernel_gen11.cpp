/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/cmd_parse/hw_parse.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/helpers/static_size3.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

#include "reg_configs_common.h"

namespace NEO {

using Gen11EnqueueTest = Test<ClDeviceFixture>;
GEN11TEST_F(Gen11EnqueueTest, givenKernelRequiringIndependentForwardProgressWhenKernelIsSubmittedThenDefaultPolicyIsProgrammed) {
    MockContext mc;
    CommandQueueHw<FamilyType> cmdQ{&mc, pClDevice, 0, false};
    SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.SubgroupIndependentForwardProgressRequired = true;
    MockKernelWithInternals mockKernel(*pClDevice, executionEnvironment);

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, StatickSize3<1, 1, 1>(), nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ);

    auto cmd = findMmioCmd<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), RowChickenReg4::address);
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(RowChickenReg4::regDataForArbitrationPolicy[HwHelperHw<FamilyType>::get().getDefaultThreadArbitrationPolicy()], cmd->getDataDword());
    EXPECT_EQ(1U, countMmio<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), RowChickenReg4::address));
}

GEN11TEST_F(Gen11EnqueueTest, givenKernelNotRequiringIndependentForwardProgressWhenKernelIsSubmittedThenAgeBasedPolicyIsProgrammed) {
    MockContext mc;
    CommandQueueHw<FamilyType> cmdQ{&mc, pClDevice, 0, false};
    SPatchExecutionEnvironment executionEnvironment = {};
    executionEnvironment.SubgroupIndependentForwardProgressRequired = false;
    MockKernelWithInternals mockKernel(*pClDevice, executionEnvironment);

    cmdQ.enqueueKernel(mockKernel.mockKernel, 1, nullptr, StatickSize3<1, 1, 1>(), nullptr, 0, nullptr, nullptr);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdQ);

    auto cmd = findMmioCmd<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), RowChickenReg4::address);
    ASSERT_NE(nullptr, cmd);
    EXPECT_EQ(RowChickenReg4::regDataForArbitrationPolicy[ThreadArbitrationPolicy::AgeBased], cmd->getDataDword());
    EXPECT_EQ(1U, countMmio<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), RowChickenReg4::address));
}
} // namespace NEO
