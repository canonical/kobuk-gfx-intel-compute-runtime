/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/tbx_command_stream_fixture.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"

#include "gtest/gtest.h"

namespace NEO {

void TbxCommandStreamFixture::setUp(MockDevice *pDevice) {
    // Create our TBX command stream receiver based on HW type
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(CommandStreamReceiverType::tbx));
    debugManager.flags.EnableTbxPageFaultManager.set(true);
    pCommandStreamReceiver = TbxCommandStreamReceiver::create("", false, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    ASSERT_NE(nullptr, pCommandStreamReceiver);
    memoryManager = new OsAgnosticMemoryManager(*pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(pCommandStreamReceiver);
}

void TbxCommandStreamFixture::tearDown() {
    delete memoryManager;
}
} // namespace NEO
