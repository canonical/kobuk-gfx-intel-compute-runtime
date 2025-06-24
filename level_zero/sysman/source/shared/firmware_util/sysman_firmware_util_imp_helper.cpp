/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util_imp.h"

static std::vector<std ::string> deviceSupportedFirmwareTypes = {"GSC", "OptionROM", "PSC"};
static constexpr uint8_t eccStateNone = 0xFF;
constexpr uint8_t maxGfspHeciOutBuffer = UINT8_MAX;
constexpr uint8_t maxGfspHeciInBuffer = 4;

namespace L0 {
namespace Sysman {

const std::string fwDeviceIfrGetStatusExt = "igsc_ifr_get_status_ext";
const std::string fwIafPscUpdate = "igsc_iaf_psc_update";
const std::string fwGfspMemoryErrors = "igsc_gfsp_memory_errors";
const std::string fwGfspGetHealthIndicator = "igsc_gfsp_get_health_indicator";
const std::string fwGfspCountTiles = "igsc_gfsp_count_tiles";
const std::string fwDeviceIfrRunMemPPRTest = "igsc_ifr_run_mem_ppr_test";
const std::string fwGfspHeciCmd = "igsc_gfsp_heci_cmd";

pIgscIfrGetStatusExt deviceIfrGetStatusExt;
pIgscIafPscUpdate iafPscUpdate;
pIgscGfspMemoryErrors gfspMemoryErrors;
pIgscGfspCountTiles gfspCountTiles;
pIgscGfspGetHealthIndicator gfspGetHealthIndicator;
pIgscIfrRunMemPPRTest deviceIfrRunMemPPRTest;
pIgscGfspHeciCmd gfspHeciCmd;

ze_result_t FirmwareUtilImp::fwIfrApplied(bool &ifrStatus) {
    uint32_t supportedTests = 0; // Bitmap holding the tests supported on the platform
    uint32_t prevErrors = 0;     // Bitmap holding which errors were seen on this SOC in previous tests
    uint32_t pendingReset = 0;   //  Whether a reset is pending after running the test
    uint32_t ifrApplied = 0;     // Bitmap holding hw capabilities on the platform

    auto result = fwCallGetstatusExt(supportedTests, ifrApplied, prevErrors, pendingReset);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    // check if either DSS or Memory PPR test completed and fix was applied
    ifrStatus = ((ifrApplied & IGSC_IFR_REPAIRS_MASK_DSS_EN_REPAIR) || (ifrApplied & IGSC_IFR_REPAIRS_MASK_ARRAY_REPAIR));
    return ZE_RESULT_SUCCESS;
}

// fwCallGetstatusExt() is a helper function to get the status of IFR after the diagnostics tests are run
ze_result_t FirmwareUtilImp::fwCallGetstatusExt(uint32_t &supportedTests, uint32_t &ifrApplied, uint32_t &prevErrors, uint32_t &pendingReset) {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    uint32_t hwCapabilities = 0;
    ifrApplied = 0;
    prevErrors = 0;
    pendingReset = 0;
    supportedTests = 0;

    int ret = deviceIfrGetStatusExt(&fwDeviceHandle, &supportedTests, &hwCapabilities, &ifrApplied, &prevErrors, &pendingReset);
    if (ret) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::fwGetMemoryErrorCount(zes_ras_error_type_t type, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count) {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    uint32_t numOfTiles = 0;
    int ret = -1;
    gfspCountTiles = reinterpret_cast<pIgscGfspCountTiles>(libraryHandle->getProcAddress(fwGfspCountTiles));
    if (gfspCountTiles != nullptr) {
        ret = gfspCountTiles(&fwDeviceHandle, &numOfTiles);
    }

    if (ret != IGSC_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Error@ %s(): Could not retrieve tile count from igsc\n", __FUNCTION__);
        // igsc_gfsp_count_tiles returns max tile info rather than actual count, igsc behaves in such a way that
        // it expects buffer (igsc_gfsp_mem_err) to be allocated for max tile count and not actual tile count.
        // This is fallback path when igsc_gfsp_count_tiles fails, where buffer for actual tile count is used to
        // get memory error count.
        numOfTiles = (subDeviceCount == 0) ? 1 : subDeviceCount;
    }

    gfspMemoryErrors = reinterpret_cast<pIgscGfspMemoryErrors>(libraryHandle->getProcAddress(fwGfspMemoryErrors));
    if (gfspMemoryErrors != nullptr) {
        auto size = sizeof(igsc_gfsp_mem_err) + numOfTiles * sizeof(igsc_gfsp_tile_mem_err);
        std::vector<uint8_t> buf(size);
        igsc_gfsp_mem_err *tiles = reinterpret_cast<igsc_gfsp_mem_err *>(buf.data());
        tiles->num_of_tiles = numOfTiles; // set the number of tiles in the structure that will be passed as a buffer
        ret = gfspMemoryErrors(&fwDeviceHandle, tiles);
        if (ret != IGSC_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                  "Error@ %s(): Could not retrieve memory errors from igsc (error:0x%x) \n", __FUNCTION__, ret);
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }

        if (tiles->num_of_tiles < subDeviceCount) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                  "Error@ %s(): Inappropriate tile count \n", __FUNCTION__);
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        if (type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
            count = tiles->errors[subDeviceId].corr_err;
        }
        if (type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
            count = tiles->errors[subDeviceId].uncorr_err;
        }
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_UNINITIALIZED;
}

void FirmwareUtilImp::fwGetMemoryHealthIndicator(zes_mem_health_t *health) {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    gfspGetHealthIndicator = reinterpret_cast<pIgscGfspGetHealthIndicator>(libraryHandle->getProcAddress(fwGfspGetHealthIndicator));
    if (gfspGetHealthIndicator != nullptr) {
        uint8_t healthIndicator = 0;
        int ret = gfspGetHealthIndicator(&fwDeviceHandle, &healthIndicator);
        if (ret == IGSC_SUCCESS) {
            switch (healthIndicator) {
            case IGSC_HEALTH_INDICATOR_HEALTHY:
                *health = ZES_MEM_HEALTH_OK;
                break;
            case IGSC_HEALTH_INDICATOR_DEGRADED:
                *health = ZES_MEM_HEALTH_DEGRADED;
                break;
            case IGSC_HEALTH_INDICATOR_CRITICAL:
                *health = ZES_MEM_HEALTH_CRITICAL;
                break;
            case IGSC_HEALTH_INDICATOR_REPLACE:
                *health = ZES_MEM_HEALTH_REPLACE;
                break;
            default:
                *health = ZES_MEM_HEALTH_UNKNOWN;
            }
            return;
        }
    }

    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(); Could not get memory health indicator from igsc\n", __FUNCTION__);
}

ze_result_t FirmwareUtilImp::fwGetEccConfig(uint8_t *currentState, uint8_t *pendingState) {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    gfspHeciCmd = reinterpret_cast<pIgscGfspHeciCmd>(libraryHandle->getProcAddress(fwGfspHeciCmd));
    if (gfspHeciCmd != nullptr) {
        std::vector<uint8_t> outBuf(maxGfspHeciOutBuffer, 0);
        size_t receivedSize = 0;
        // Need to call gfspHeciCmd 0x10 cmd first and if not available, fallback and call 0x9.
        int ret = gfspHeciCmd(&fwDeviceHandle, GfspHeciConstants::Cmd::getEccConfigurationCmd16, nullptr, 0, outBuf.data(), maxGfspHeciOutBuffer, &receivedSize);

        if (ret == IGSC_SUCCESS) {
            *currentState = outBuf[GfspHeciConstants::GetEccCmd16BytePostition::eccCurrentState] & 0x1;
            *pendingState = outBuf[GfspHeciConstants::GetEccCmd16BytePostition::eccPendingState] & 0x1;
            return ZE_RESULT_SUCCESS;
        }
        receivedSize = 0;
        std::fill(outBuf.begin(), outBuf.end(), 0);
        ret = gfspHeciCmd(&fwDeviceHandle, GfspHeciConstants::Cmd::getEccConfigurationCmd9, nullptr, 0, outBuf.data(), maxGfspHeciOutBuffer, &receivedSize);

        if (ret == IGSC_SUCCESS) {
            *currentState = outBuf[GfspHeciConstants::GetEccCmd9BytePostition::currentState];
            *pendingState = outBuf[GfspHeciConstants::GetEccCmd9BytePostition::pendingState];
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t FirmwareUtilImp::fwGetEccAvailable(ze_bool_t *pAvailable) {
    *pAvailable = false;
    const std::lock_guard<std::mutex> lock(this->fwLock);
    gfspHeciCmd = reinterpret_cast<pIgscGfspHeciCmd>(libraryHandle->getProcAddress(fwGfspHeciCmd));
    if (gfspHeciCmd != nullptr) {
        std::vector<uint8_t> outBuf(maxGfspHeciOutBuffer, 0);
        size_t receivedSize = 0;
        // Need to call gfspHeciCmd 0x10 cmd first and if not available, fallback and call 0x9.
        int ret = gfspHeciCmd(&fwDeviceHandle, GfspHeciConstants::Cmd::getEccConfigurationCmd16, nullptr, 0, outBuf.data(), maxGfspHeciOutBuffer, &receivedSize);

        if (ret == IGSC_SUCCESS) {
            *pAvailable = outBuf[GfspHeciConstants::GetEccCmd16BytePostition::eccAvailable] & 0x1;
            return ZE_RESULT_SUCCESS;
        }
        receivedSize = 0;
        std::fill(outBuf.begin(), outBuf.end(), 0);
        ret = gfspHeciCmd(&fwDeviceHandle, GfspHeciConstants::Cmd::getEccConfigurationCmd9, nullptr, 0, outBuf.data(), maxGfspHeciOutBuffer, &receivedSize);

        if (ret == IGSC_SUCCESS) {
            uint8_t currentState = outBuf[GfspHeciConstants::GetEccCmd9BytePostition::currentState];
            uint8_t pendingState = outBuf[GfspHeciConstants::GetEccCmd9BytePostition::pendingState];

            if ((currentState != eccStateNone) && (pendingState != eccStateNone)) {
                *pAvailable = true;
            }
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t FirmwareUtilImp::fwGetEccConfigurable(ze_bool_t *pConfigurable) {
    *pConfigurable = false;
    const std::lock_guard<std::mutex> lock(this->fwLock);
    gfspHeciCmd = reinterpret_cast<pIgscGfspHeciCmd>(libraryHandle->getProcAddress(fwGfspHeciCmd));
    if (gfspHeciCmd != nullptr) {
        std::vector<uint8_t> outBuf(maxGfspHeciOutBuffer, 0);
        size_t receivedSize = 0;
        // Need to call gfspHeciCmd 0x10 cmd first and if not available, fallback and call 0x9.
        int ret = gfspHeciCmd(&fwDeviceHandle, GfspHeciConstants::Cmd::getEccConfigurationCmd16, nullptr, 0, outBuf.data(), maxGfspHeciOutBuffer, &receivedSize);

        if (ret == IGSC_SUCCESS) {
            *pConfigurable = outBuf[GfspHeciConstants::GetEccCmd16BytePostition::eccConfigurable] & 0x1;
            return ZE_RESULT_SUCCESS;
        }
        receivedSize = 0;
        std::fill(outBuf.begin(), outBuf.end(), 0);
        ret = gfspHeciCmd(&fwDeviceHandle, GfspHeciConstants::Cmd::getEccConfigurationCmd9, nullptr, 0, outBuf.data(), maxGfspHeciOutBuffer, &receivedSize);

        if (ret == IGSC_SUCCESS) {
            uint8_t currentState = outBuf[GfspHeciConstants::GetEccCmd9BytePostition::currentState];
            uint8_t pendingState = outBuf[GfspHeciConstants::GetEccCmd9BytePostition::pendingState];

            if ((currentState != eccStateNone) && (pendingState != eccStateNone)) {
                *pConfigurable = true;
            }
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t FirmwareUtilImp::fwSetEccConfig(uint8_t newState, uint8_t *currentState, uint8_t *pendingState) {
    std::unique_lock<std::mutex> lock(this->fwLock);
    gfspHeciCmd = reinterpret_cast<pIgscGfspHeciCmd>(libraryHandle->getProcAddress(fwGfspHeciCmd));
    if (gfspHeciCmd != nullptr) {
        std::vector<uint8_t> inBuf(maxGfspHeciInBuffer, 0);
        std::vector<uint8_t> outBuf(maxGfspHeciOutBuffer, 0);
        size_t receivedSize = 0;
        inBuf[GfspHeciConstants::SetEccCmd15BytePostition::request] = newState;
        // Need to call gfspHeciCmd 0x15 cmd first and if not available, fallback and call 0x8.
        int ret = gfspHeciCmd(&fwDeviceHandle, GfspHeciConstants::Cmd::setEccConfigurationCmd15, inBuf.data(), maxGfspHeciInBuffer, outBuf.data(), maxGfspHeciOutBuffer, &receivedSize);

        if (ret == IGSC_SUCCESS) {
            lock.unlock();
            ze_result_t status = fwGetEccConfig(currentState, pendingState);
            lock.lock();
            if (status != ZE_RESULT_SUCCESS) {
                return status;
            }
            *pendingState = outBuf[GfspHeciConstants::SetEccCmd15BytePostition::response] & 0x1;
            return ZE_RESULT_SUCCESS;
        }
        receivedSize = 0;
        std::fill(inBuf.begin(), inBuf.end(), 0);
        inBuf[GfspHeciConstants::SetEccCmd8BytePostition::setRequest] = newState;
        std::fill(outBuf.begin(), outBuf.end(), 0);
        ret = gfspHeciCmd(&fwDeviceHandle, GfspHeciConstants::Cmd::setEccConfigurationCmd8, inBuf.data(), maxGfspHeciInBuffer, outBuf.data(), maxGfspHeciOutBuffer, &receivedSize);

        if (ret == IGSC_SUCCESS) {
            *currentState = outBuf[GfspHeciConstants::SetEccCmd8BytePostition::responseCurrentState];
            *pendingState = outBuf[GfspHeciConstants::SetEccCmd8BytePostition::responsePendingState];
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t FirmwareUtilImp::fwSupportedDiagTests(std::vector<std::string> &supportedDiagTests) {
    uint32_t supportedTests = 0;
    uint32_t prevErrors = 0;
    uint32_t pendingReset = 0;
    uint32_t ifrApplied = 0;
    auto result = fwCallGetstatusExt(supportedTests, ifrApplied, prevErrors, pendingReset);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    if (supportedTests & IGSC_IFR_SUPPORTED_TESTS_MEMORY_PPR) {
        supportedDiagTests.push_back("MEMORY_PPR");
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::fwRunDiagTests(std::string &osDiagType, zes_diag_result_t *pDiagResult) {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    uint32_t status = 0;
    uint32_t pendingReset = 0;
    uint32_t errorCode = 0;

    if (osDiagType.compare("MEMORY_PPR") == 0) {
        int ret = deviceIfrRunMemPPRTest(&fwDeviceHandle, &status, &pendingReset, &errorCode);
        if (ret) {
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        if (status == 1) {
            *pDiagResult = ZES_DIAG_RESULT_REBOOT_FOR_REPAIR;
        }
        if (status == 0) {
            *pDiagResult = ZES_DIAG_RESULT_NO_ERRORS;
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::pscGetVersion(std::string &fwVersion) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t FirmwareUtilImp::fwFlashIafPsc(void *pImage, uint32_t size) {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    iafPscUpdate = reinterpret_cast<pIgscIafPscUpdate>(libraryHandle->getProcAddress(fwIafPscUpdate));

    if (iafPscUpdate == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    int ret = iafPscUpdate(&fwDeviceHandle, static_cast<const uint8_t *>(pImage), size, firmwareFlashProgressFunc, this);
    if (ret != IGSC_SUCCESS) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::flashFirmware(std::string fwType, void *pImage, uint32_t size) {
    flashProgress.fwProgressLock.lock();
    flashProgress.completionPercent = 0;
    flashProgress.fwProgressLock.unlock();

    if (fwType == deviceSupportedFirmwareTypes[0]) { // GSC
        return fwFlashGSC(pImage, size);
    }
    if (fwType == deviceSupportedFirmwareTypes[1]) { // OPROM
        return fwFlashOprom(pImage, size);
    }
    if (fwType == deviceSupportedFirmwareTypes[2]) { // PSC
        return fwFlashIafPsc(pImage, size);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void FirmwareUtilImp::getDeviceSupportedFwTypes(std::vector<std::string> &fwTypes) {
    fwTypes = deviceSupportedFirmwareTypes;
}

ze_result_t FirmwareUtilImp::getFwVersion(std::string fwType, std::string &firmwareVersion) {
    if (fwType == deviceSupportedFirmwareTypes[0]) { // GSC
        return fwGetVersion(firmwareVersion);
    }
    if (fwType == deviceSupportedFirmwareTypes[1]) { // OPROM
        return opromGetVersion(firmwareVersion);
    }
    if (fwType == deviceSupportedFirmwareTypes[2]) { // PSC
        return pscGetVersion(firmwareVersion);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool FirmwareUtilImp::loadEntryPointsExt() {
    bool ok = getSymbolAddr(fwDeviceIfrGetStatusExt, deviceIfrGetStatusExt);
    ok = ok && getSymbolAddr(fwDeviceIfrRunMemPPRTest, deviceIfrRunMemPPRTest);
    return ok;
}
} // namespace Sysman
} // namespace L0
