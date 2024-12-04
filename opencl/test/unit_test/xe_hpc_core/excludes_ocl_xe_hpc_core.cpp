/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenAlignedCacheableReadOnlyBufferThenChoseOclBufferPolicy, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryIsUnalignedToCachelineButReadOnlyThenL3CacheShouldBeStillOn, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(QueueFamilyNameTest, givenRcsWhenGettingQueueFamilyNameThenReturnProperValue, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(EnqueueCopyBufferToImageStatelessTest, givenBigBufferWhenCopyingBufferToImageStatelessThenSuccessIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(EnqueueCopyImageToBufferHwStatelessTest, givenBigBufferWhenCopyingImageToBufferStatelessThenSuccessIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyImageToSystemBufferStatelessIsUsedThenParamsAreCorrect, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(BuiltInTests, givenBigOffsetAndSizeWhenBuilderCopyImageToLocalBufferStatelessIsUsedThenParamsAreCorrect, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(CommandStreamReceiverHwTestDg2AndLater, givenGen12AndLaterWhenRayTracingEnabledThenCommandIsAddedToBatchBuffer_MatcherIsRTCapable, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(CommandStreamReceiverHwTestDg2AndLater, givenGen12AndLaterWhenRayTracingEnabledButAlreadySentThenCommandIsNotAddedToBatchBuffer_MatcherIsRTCapable, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(DeviceGetCapsTest, givenDeviceWhenAskingForSubGroupSizesThenReturnCorrectValues, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ClGfxCoreHelperTest, givenKernelInfoWhenCheckingRequiresAuxResolvesThenCorrectValuesAreReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(EnqueueReadBufferRectStatefulTest, WhenReadingBufferRectStatefulThenSuccessIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(EnqueueSvmMemCopyHwTest, givenEnqueueSVMMemCopyWhenUsingCopyBufferToBufferStatefulBuilderThenSuccessIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(EnqueueSvmMemFillHwTest, givenEnqueueSVMMemFillWhenUsingCopyBufferToLocalBufferStatefulBuilderThenSuccessIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(EnqueueFillBufferStatefulTest, givenBuffersWhenFillingBufferStatefulThenSuccessIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(EnqueueCopyBufferStatefulTest, givenBuffersWhenCopyingBufferStatefulThenSuccessIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(EnqueueWriteBufferStatefulTest, WhenWritingBufferStatefulThenSuccessIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(EnqueueReadBufferStatefulTest, WhenReadingBufferStatefulThenSuccessIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(EnqueueWriteBufferRectStatefulTest, WhenWritingBufferRectStatefulThenSuccessIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(CommandStreamReceiverFlushTaskTests, givenOverrideThreadArbitrationPolicyDebugVariableSetWhenFlushingThenRequestRequiredMode, IGFX_XE_HPC_CORE);
