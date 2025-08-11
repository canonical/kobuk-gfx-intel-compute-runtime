/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace NEO {
class Device;
} // namespace NEO

namespace L0 {
namespace ult {

using KernelPropertyTest = Test<DeviceFixture>;

HWTEST2_F(KernelPropertyTest, givenKernelExtendedPropertiesStructureWhenKernelPropertiesCalledThenPropertiesAreCorrectlySet, IsXe3Core) {
    ze_device_module_properties_t kernelProperties = {};
    ze_float_atomic_ext_properties_t kernelExtendedProperties = {};
    kernelExtendedProperties.stype = ZE_STRUCTURE_TYPE_FLOAT_ATOMIC_EXT_PROPERTIES;
    kernelProperties.pNext = &kernelExtendedProperties;
    ze_result_t res = device->getKernelProperties(&kernelProperties);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    const auto &fp16Properties = kernelExtendedProperties.fp16Flags;
    EXPECT_TRUE(fp16Properties & FpAtomicExtFlags::globalLoadStore);
    EXPECT_TRUE(fp16Properties & FpAtomicExtFlags::localLoadStore);
    EXPECT_TRUE(fp16Properties & FpAtomicExtFlags::globalMinMax);
    EXPECT_TRUE(fp16Properties & FpAtomicExtFlags::localMinMax);

    EXPECT_FALSE(fp16Properties & FpAtomicExtFlags::globalAdd);
    EXPECT_FALSE(fp16Properties & FpAtomicExtFlags::localAdd);

    const auto &fp32Properties = kernelExtendedProperties.fp32Flags;
    EXPECT_TRUE(fp32Properties & FpAtomicExtFlags::globalLoadStore);
    EXPECT_TRUE(fp32Properties & FpAtomicExtFlags::localLoadStore);
    EXPECT_TRUE(fp32Properties & FpAtomicExtFlags::globalMinMax);
    EXPECT_TRUE(fp32Properties & FpAtomicExtFlags::localMinMax);
    EXPECT_TRUE(fp32Properties & FpAtomicExtFlags::globalAdd);
    EXPECT_TRUE(fp32Properties & FpAtomicExtFlags::localAdd);

    const auto &fp64Properties = kernelExtendedProperties.fp64Flags;
    EXPECT_TRUE(fp64Properties & FpAtomicExtFlags::globalLoadStore);
    EXPECT_TRUE(fp64Properties & FpAtomicExtFlags::localLoadStore);
    EXPECT_TRUE(fp64Properties & FpAtomicExtFlags::globalMinMax);
    EXPECT_TRUE(fp64Properties & FpAtomicExtFlags::localMinMax);
    EXPECT_TRUE(fp64Properties & FpAtomicExtFlags::globalAdd);
    EXPECT_TRUE(fp64Properties & FpAtomicExtFlags::localAdd);
}

using Xe3KernelSetupTests = ::testing::Test;

XE3_CORETEST_F(Xe3KernelSetupTests, givenParamsWhenSetupGroupSizeThenNumThreadsPerThreadGroupAreCorrectly) {
    DebugManagerStateRestore restore;
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());

    {
        NEO::Device *mockNeoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
        MockDeviceImp l0Device(mockNeoDevice);

        Mock<KernelImp> kernel;
        kernel.descriptor.kernelAttributes.numGrfRequired = 128u;
        kernel.enableForcingOfGenerateLocalIdByHw = true;
        Mock<Module> module(&l0Device, nullptr);
        module.getMaxGroupSizeResult = UINT32_MAX;
        kernel.module = &module;

        std::array<std::array<uint32_t, 2>, 2> values = {{
            {16u, 64u}, // SIMT Size, Max Num of threads
            {32u, 32u},

        }};

        for (auto &[simtSize, expectedNumThreadsPerThreadGroup] : values) {
            kernel.descriptor.kernelAttributes.simdSize = simtSize;
            kernel.setGroupSize(1024u, 1024u, 1024u);
            EXPECT_EQ(expectedNumThreadsPerThreadGroup, kernel.state.numThreadsPerThreadGroup);
            kernel.state.groupSize[0] = kernel.state.groupSize[1] = kernel.state.groupSize[2] = 0;
        }
    }
    {
        NEO::Device *mockNeoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
        MockDeviceImp l0Device(mockNeoDevice);

        Mock<KernelImp> kernel;
        kernel.descriptor.kernelAttributes.numGrfRequired = 160u;
        kernel.enableForcingOfGenerateLocalIdByHw = true;
        Mock<Module> module(&l0Device, nullptr);
        module.getMaxGroupSizeResult = UINT32_MAX;
        kernel.module = &module;

        std::array<std::array<uint32_t, 2>, 2> values = {{
            {16u, 48u}, // SIMT Size,  Max Num of threads
            {32u, 32u},
        }};

        for (auto &[simtSize, expectedNumThreadsPerThreadGroup] : values) {
            kernel.descriptor.kernelAttributes.simdSize = simtSize;
            kernel.setGroupSize(1024u, 1024u, 1024u);
            EXPECT_EQ(expectedNumThreadsPerThreadGroup, kernel.state.numThreadsPerThreadGroup);
            kernel.state.groupSize[0] = kernel.state.groupSize[1] = kernel.state.groupSize[2] = 0;
        }
    }
    {
        NEO::Device *mockNeoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
        MockDeviceImp l0Device(mockNeoDevice);

        Mock<KernelImp> kernel;
        kernel.descriptor.kernelAttributes.numGrfRequired = 192u;
        kernel.enableForcingOfGenerateLocalIdByHw = true;
        Mock<Module> module(&l0Device, nullptr);
        module.getMaxGroupSizeResult = UINT32_MAX;
        kernel.module = &module;

        std::array<std::array<uint32_t, 2>, 2> values = {{
            {16u, 40u}, // SIMT Size, Max Num of threads
            {32u, 32u},
        }};

        for (auto &[simtSize, expectedNumThreadsPerThreadGroup] : values) {
            kernel.descriptor.kernelAttributes.simdSize = simtSize;
            kernel.setGroupSize(1024u, 1024u, 1024u);
            EXPECT_EQ(expectedNumThreadsPerThreadGroup, kernel.state.numThreadsPerThreadGroup);
            kernel.state.groupSize[0] = kernel.state.groupSize[1] = kernel.state.groupSize[2] = 0;
        }
    }
    {
        NEO::Device *mockNeoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
        MockDeviceImp l0Device(mockNeoDevice);

        Mock<KernelImp> kernel;
        kernel.descriptor.kernelAttributes.numGrfRequired = 256u;
        kernel.enableForcingOfGenerateLocalIdByHw = true;
        Mock<Module> module(&l0Device, nullptr);
        module.getMaxGroupSizeResult = UINT32_MAX;
        kernel.module = &module;

        std::array<std::array<uint32_t, 2>, 2> values = {{
            {16u, 32u}, // SIMT Size, Max Num of threads
            {32u, 32u},
        }};

        for (auto &[simtSize, expectedNumThreadsPerThreadGroup] : values) {
            kernel.descriptor.kernelAttributes.simdSize = simtSize;
            kernel.setGroupSize(1024u, 1024u, 1024u);
            EXPECT_EQ(expectedNumThreadsPerThreadGroup, kernel.state.numThreadsPerThreadGroup);
            kernel.state.groupSize[0] = kernel.state.groupSize[1] = kernel.state.groupSize[2] = 0;
        }
    }
    {
        NEO::Device *mockNeoDevice(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), 0));
        MockDeviceImp l0Device(mockNeoDevice);

        Mock<KernelImp> kernel;
        kernel.descriptor.kernelAttributes.numGrfRequired = 512u;
        kernel.enableForcingOfGenerateLocalIdByHw = true;
        Mock<Module> module(&l0Device, nullptr);
        module.getMaxGroupSizeResult = UINT32_MAX;
        kernel.module = &module;

        std::array<std::array<uint32_t, 2>, 2> values = {{
            {16u, 16u}, // SIMT Size, Max Num of threads
            {32u, 16u},
        }};

        for (auto &[simtSize, expectedNumThreadsPerThreadGroup] : values) {
            kernel.descriptor.kernelAttributes.simdSize = simtSize;
            kernel.setGroupSize(1024u, 1024u, 1024u);
            EXPECT_EQ(expectedNumThreadsPerThreadGroup, kernel.state.numThreadsPerThreadGroup);
            kernel.state.groupSize[0] = kernel.state.groupSize[1] = kernel.state.groupSize[2] = 0;
        }
    }
}

} // namespace ult
} // namespace L0
