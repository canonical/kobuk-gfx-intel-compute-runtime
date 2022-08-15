/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(ProfilingTests, GivenCommandQueueBlockedWithProfilingWhenWalkerIsDispatchedThenMiStoreRegisterMemIsPresentInCS, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(ProfilingTests, GivenCommandQueueWithProflingWhenWalkerIsDispatchedThenMiStoreRegisterMemIsPresentInCS, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(EventProfilingTest, givenEventWhenCompleteIsZeroThenCalcProfilingDataSetsEndTimestampInCompleteTimestampAndDoesntCallOsTimeMethods, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(EventProfilingTests, givenRawTimestampsDebugModeWhenDataIsQueriedThenRawDataIsReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(EventProfilingTest, givenRawTimestampsDebugModeWhenStartTimeStampLTQueueTimeStampThenIncreaseStartTimeStamp, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(ProfilingWithPerfCountersTests, GivenCommandQueueWithProfilingPerfCountersWhenWalkerIsDispatchedThenRegisterStoresArePresentInCS, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(ProfilingCommandsTest, givenKernelWhenProfilingCommandStartIsTakenThenTimeStampAddressIsProgrammedCorrectly, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(ProfilingCommandsTest, givenKernelWhenProfilingCommandStartIsNotTakenThenTimeStampAddressIsProgrammedCorrectly, IGFX_GEN12LP_CORE);
