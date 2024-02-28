/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe_hpg_core/aub_mapper.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

using Family = NEO::XeHpgCoreFamily;

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/gfx_core_helper_base.inl"
#include "shared/source/helpers/gfx_core_helper_bdw_to_dg2.inl"
#include "shared/source/helpers/gfx_core_helper_dg2_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_tgllp_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xehp_and_later.inl"
#include "shared/source/helpers/local_memory_access_modes.h"

namespace NEO {
template <>
const AuxTranslationMode GfxCoreHelperHw<Family>::defaultAuxTranslationMode = AuxTranslationMode::blit;

template <>
inline bool GfxCoreHelperHw<Family>::isFusedEuDispatchEnabled(const HardwareInfo &hwInfo, bool disableEUFusionForKernel) const {
    auto fusedEuDispatchEnabled = !hwInfo.workaroundTable.flags.waDisableFusedThreadScheduling;
    fusedEuDispatchEnabled &= hwInfo.capabilityTable.fusedEuEnabled;

    if (disableEUFusionForKernel)
        fusedEuDispatchEnabled = false;

    if (debugManager.flags.CFEFusedEUDispatch.get() != -1) {
        fusedEuDispatchEnabled = (debugManager.flags.CFEFusedEUDispatch.get() == 0);
    }
    return fusedEuDispatchEnabled;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::XeHPG);
}

template <>
void GfxCoreHelperHw<Family>::adjustDefaultEngineType(HardwareInfo *pHwInfo, const ProductHelper &productHelper, AILConfiguration *ailConfiguration) {
    if (!pHwInfo->featureTable.flags.ftrCCSNode) {
        pHwInfo->capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
    }
    if (productHelper.isDefaultEngineTypeAdjustmentRequired(*pHwInfo) || (ailConfiguration && ailConfiguration->forceRcs())) {
        pHwInfo->capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
    }
}

template <>
bool GfxCoreHelperHw<Family>::is1MbAlignmentSupported(const HardwareInfo &hwInfo, bool isCompressionEnabled) const {
    return !hwInfo.workaroundTable.flags.waAuxTable64KGranular && isCompressionEnabled;
}

template <>
void GfxCoreHelperHw<Family>::setL1CachePolicy(bool useL1Cache, typename Family::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo) const {
    if (useL1Cache) {
        surfaceState->setL1CachePolicyL1CacheControl(Family::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB);
        if (debugManager.flags.OverrideL1CacheControlInSurfaceStateForScratchSpace.get() != -1) {
            surfaceState->setL1CachePolicyL1CacheControl(static_cast<typename Family::RENDER_SURFACE_STATE::L1_CACHE_POLICY>(debugManager.flags.OverrideL1CacheControlInSurfaceStateForScratchSpace.get()));
        }
    }
}

template <>
bool GfxCoreHelperHw<Family>::isBankOverrideRequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const {

    bool forceOverrideMemoryBankIndex = false;

    if (debugManager.flags.ForceMemoryBankIndexOverride.get() != -1) {
        forceOverrideMemoryBankIndex = static_cast<bool>(debugManager.flags.ForceMemoryBankIndexOverride.get());
    }
    return forceOverrideMemoryBankIndex;
}

template <>
size_t MemorySynchronizationCommands<Family>::getSizeForSingleAdditionalSynchronizationForDirectSubmission(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return EncodeSemaphore<Family>::getSizeMiSemaphoreWait();
}

template <>
void MemorySynchronizationCommands<Family>::addAdditionalSynchronizationForDirectSubmission(LinearStream &commandStream, uint64_t gpuAddress, bool acquire, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using COMPARE_OPERATION = typename Family::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    EncodeSemaphore<Family>::addMiSemaphoreWaitCommand(commandStream, gpuAddress, EncodeSemaphore<Family>::invalidHardwareTag, COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, false, false, false, nullptr);
}

template <>
bool GfxCoreHelperHw<Family>::isBufferSizeSuitableForCompression(const size_t size) const {
    if (debugManager.flags.OverrideBufferSuitableForRenderCompression.get() != -1) {
        return !!debugManager.flags.OverrideBufferSuitableForRenderCompression.get();
    }

    if (CompressionSelector::allowStatelessCompression()) {
        return true;
    } else {
        return false;
    }
}

template <>
uint32_t GfxCoreHelperHw<Family>::computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) const {
    using SHARED_LOCAL_MEMORY_SIZE = typename Family::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    auto slmValue = std::max(slmSize, 1024u);
    slmValue = Math::nextPowerOfTwo(slmValue);
    slmValue = Math::getMinLsbSet(slmValue);
    slmValue = slmValue - 9;
    DEBUG_BREAK_IF(slmValue > 7);
    slmValue *= !!slmSize;
    return slmValue;
}

template <>
bool GfxCoreHelperHw<Family>::copyThroughLockedPtrEnabled(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const {
    if (debugManager.flags.ExperimentalCopyThroughLock.get() != -1) {
        return debugManager.flags.ExperimentalCopyThroughLock.get() == 1;
    }

    return this->isLocalMemoryEnabled(hwInfo) && !productHelper.isUnlockingLockedPtrNecessary(hwInfo);
}
template <>
uint32_t GfxCoreHelperHw<Family>::calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount) const {
    if (grfCount > GrfConfig::defaultGrfNumber) {
        return hwInfo.gtSystemInfo.ThreadCount / 2u;
    }
    return hwInfo.gtSystemInfo.ThreadCount;
}

template <>
void GfxCoreHelperHw<Family>::setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

    if (LocalMemoryAccessMode::cpuAccessDisallowed == productHelper.getLocalMemoryAccessMode(hwInfo)) {
        if (properties.allocationType == AllocationType::linearStream ||
            properties.allocationType == AllocationType::internalHeap ||
            properties.allocationType == AllocationType::printfSurface ||
            properties.allocationType == AllocationType::assertBuffer ||
            properties.allocationType == AllocationType::gpuTimestampDeviceBuffer ||
            properties.allocationType == AllocationType::ringBuffer ||
            properties.allocationType == AllocationType::semaphoreBuffer) {
            allocationData.flags.useSystemMemory = true;
        }
        if (!allocationData.flags.useSystemMemory) {
            allocationData.flags.requiresCpuAccess = false;
            allocationData.storageInfo.isLockable = false;
        }
    } else if (hwInfo.featureTable.flags.ftrLocalMemory &&
               (properties.allocationType == AllocationType::commandBuffer ||
                properties.allocationType == AllocationType::ringBuffer ||
                properties.allocationType == AllocationType::semaphoreBuffer)) {
        allocationData.flags.useSystemMemory = false;
        allocationData.flags.requiresCpuAccess = true;
    }

    if (CompressionSelector::allowStatelessCompression()) {
        if (properties.allocationType == AllocationType::globalSurface ||
            properties.allocationType == AllocationType::constantSurface ||
            properties.allocationType == AllocationType::printfSurface) {
            allocationData.flags.requiresCpuAccess = false;
            allocationData.storageInfo.isLockable = false;
        }
    }

    if (productHelper.isStorageInfoAdjustmentRequired()) {
        if (properties.allocationType == AllocationType::buffer && !properties.flags.preferCompressed && !properties.flags.shareable) {
            allocationData.storageInfo.isLockable = true;
        }
    }
}

template <>
bool MemorySynchronizationCommands<Family>::isBarrierPriorToPipelineSelectWaRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return rootDeviceEnvironment.getReleaseHelper()->isPipeControlPriorToPipelineSelectWaRequired();
}

template class GfxCoreHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO
