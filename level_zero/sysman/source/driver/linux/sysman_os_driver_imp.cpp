/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/driver/linux/sysman_os_driver_imp.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/directory.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "level_zero/sysman/source/device/sysman_hw_device_id.h"
#include "level_zero/sysman/source/device/sysman_os_device.h"
#include "level_zero/sysman/source/driver/os_sysman_driver.h"

namespace L0 {
namespace Sysman {

std::vector<std::string> getFiles(const std::string &path) {
    std::vector<std::string> files;

    DIR *dir = NEO::SysCalls::opendir(path.c_str());
    if (dir == nullptr) {
        return files;
    }

    struct dirent *entry = nullptr;
    while ((entry = NEO::SysCalls::readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        std::string fullPath;
        fullPath += path;
        fullPath += "/";
        fullPath += entry->d_name;

        files.push_back(fullPath);
    }

    NEO::SysCalls::closedir(dir);
    return files;
}

std::vector<std::unique_ptr<NEO::HwDeviceId>> LinuxDriverImp::discoverDevicesWithSurvivabilityMode() {
    std::vector<std::unique_ptr<NEO::HwDeviceId>> hwSurvivabilityDeviceIds;
    const char *pciXeKmdDevicesDirectory = "/sys/bus/pci/drivers/xe";
    const char *survivabilitySysFsNodeName = "/survivability_mode";

    std::vector<std::string> files = getFiles(pciXeKmdDevicesDirectory);

    for (std::vector<std::string>::iterator file = files.begin(); file != files.end(); ++file) {
        std::string devicePathView(file->c_str(), file->size());
        std::size_t loc = devicePathView.find_last_of("/");
        std::string bdfString = devicePathView.substr(loc + 1); // omits / character too

        constexpr int bdfTokensNum = 4;
        uint16_t domain = -1;
        uint8_t bus = -1, device = -1, function = -1;
        if (NEO::parseBdfString(bdfString, domain, bus, device, function) != bdfTokensNum) {
            continue;
        }

        std::string path = std::string(file->c_str()) + std::string(survivabilitySysFsNodeName);
        int fileDescriptor = NEO::SysCalls::open(path.c_str(), O_RDONLY);
        if (fileDescriptor < 0) {
            continue;
        }
        hwSurvivabilityDeviceIds.push_back(std::make_unique<NEO::HwDeviceIdDrm>(fileDescriptor, bdfString.c_str(), file->c_str()));
    }

    return hwSurvivabilityDeviceIds;
}

ze_result_t LinuxDriverImp::initializeInSurvivabilityMode(std::vector<std::unique_ptr<NEO::HwDeviceId>> &&hwDeviceIds, SysmanDriverHandleImp *pSysmanDriverHandleImp) {
    for (auto &hwDeviceId : hwDeviceIds) {
        auto sysmanHwDeviceId = createSysmanHwDeviceId(hwDeviceId);
        auto pSysmanDevice = OsSysmanSurvivabilityDevice::createSurvivabilityDevice(std::move(sysmanHwDeviceId));
        if (pSysmanDevice != nullptr) {
            pSysmanDriverHandleImp->sysmanDevices.push_back(pSysmanDevice);
        }
    }

    if (pSysmanDriverHandleImp->sysmanDevices.size() == 0) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    if (pSysmanDriverHandleImp->pOsSysmanDriver == nullptr) {
        pSysmanDriverHandleImp->pOsSysmanDriver = L0::Sysman::OsSysmanDriver::create();
    }
    pSysmanDriverHandleImp->numDevices = static_cast<uint32_t>(pSysmanDriverHandleImp->sysmanDevices.size());
    return ZE_RESULT_SUCCESS;
}

SysmanDriverHandle *LinuxDriverImp::createInSurvivabilityMode(std::vector<std::unique_ptr<NEO::HwDeviceId>> &&hwDeviceId, ze_result_t *returnValue) {
    SysmanDriverHandleImp *driverHandle = new SysmanDriverHandleImp;
    UNRECOVERABLE_IF(nullptr == driverHandle);
    ze_result_t res = initializeInSurvivabilityMode(std::move(hwDeviceId), driverHandle);
    if (res != ZE_RESULT_SUCCESS) {
        delete driverHandle;
        driverHandle = nullptr;
        *returnValue = res;
        return nullptr;
    }
    globalSysmanDriver = driverHandle;
    *returnValue = res;
    return driverHandle;
}

void LinuxDriverImp::initSurvivabilityDevices(_ze_driver_handle_t *sysmanDriverHandle, ze_result_t *result) {
    std::vector<std::unique_ptr<NEO::HwDeviceId>> hwSurvivabilityDeviceIds = discoverDevicesWithSurvivabilityMode();
    if (!hwSurvivabilityDeviceIds.empty()) {
        SysmanDriverHandleImp *pSysmanDriverHandleImp = static_cast<SysmanDriverHandleImp *>(sysmanDriverHandle);
        *result = initializeInSurvivabilityMode(std::move(hwSurvivabilityDeviceIds), pSysmanDriverHandleImp);
    }
}

SysmanDriverHandle *LinuxDriverImp::initSurvivabilityDevicesWithDriver(ze_result_t *result, uint32_t *driverCount) {
    std::vector<std::unique_ptr<NEO::HwDeviceId>> hwSurvivabilityDeviceIds = discoverDevicesWithSurvivabilityMode();
    SysmanDriverHandle *pSysmanDriverHandle = nullptr;
    if (!hwSurvivabilityDeviceIds.empty()) {
        pSysmanDriverHandle = createInSurvivabilityMode(std::move(hwSurvivabilityDeviceIds), result);
        *driverCount = 1;
    } else {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "%s\n", "No devices found");
        *result = ZE_RESULT_ERROR_UNINITIALIZED;
    }

    return pSysmanDriverHandle;
}

std::unique_ptr<OsDriver> OsDriver::create() {
    std::unique_ptr<LinuxDriverImp> pLinuxDriverImp = std::make_unique<LinuxDriverImp>();
    return pLinuxDriverImp;
}

} // namespace Sysman
} // namespace L0
