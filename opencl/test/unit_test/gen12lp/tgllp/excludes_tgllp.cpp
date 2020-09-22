/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

namespace NEO {
HWCMDTEST_EXCLUDE_FAMILY(ProfilingTests, GivenCommandQueueBlockedWithProfilingWhenWalkerIsDispatchedThenMiStoreRegisterMemIsPresentInCS, IGFX_TIGERLAKE_LP);

HWCMDTEST_EXCLUDE_FAMILY(ProfilingTests, GivenCommandQueueWithProflingWhenWalkerIsDispatchedThenMiStoreRegisterMemIsPresentInCS, IGFX_TIGERLAKE_LP);

HWCMDTEST_EXCLUDE_FAMILY(EventProfilingTest, givenEventWhenCompleteIsZeroThenCalcProfilingDataSetsEndTimestampInCompleteTimestampAndDoesntCallOsTimeMethods, IGFX_TIGERLAKE_LP);

HWCMDTEST_EXCLUDE_FAMILY(EventProfilingTests, givenRawTimestampsDebugModeWhenDataIsQueriedThenRawDataIsReturned, IGFX_TIGERLAKE_LP);

HWCMDTEST_EXCLUDE_FAMILY(EventProfilingTest, givenRawTimestampsDebugModeWhenStartTimeStampLTQueueTimeStampThenIncreaseStartTimeStamp, IGFX_TIGERLAKE_LP);

HWCMDTEST_EXCLUDE_FAMILY(ProfilingWithPerfCountersTests, GivenCommandQueueWithProfilingPerfCountersWhenWalkerIsDispatchedThenRegisterStoresArePresentInCS, IGFX_TIGERLAKE_LP);

HWCMDTEST_EXCLUDE_FAMILY(TimestampEventCreate, givenEventTimestampsWhenQueryKernelTimestampThenCorrectDataAreSet, IGFX_TIGERLAKE_LP);
} // namespace NEO

HWCMDTEST_EXCLUDE_FAMILY(ProfilingCommandsTest, givenKernelWhenProfilingCommandStartIsTakenThenTimeStampAddressIsProgrammedCorrectly, IGFX_TIGERLAKE_LP);

HWCMDTEST_EXCLUDE_FAMILY(ProfilingCommandsTest, givenKernelWhenProfilingCommandStartIsNotTakenThenTimeStampAddressIsProgrammedCorrectly, IGFX_TIGERLAKE_LP);
