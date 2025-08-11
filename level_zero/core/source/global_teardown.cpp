/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/global_teardown.h"

#include "shared/source/os_interface/sys_calls_common.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/ddi/ze_ddi_tables.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"

namespace L0 {

decltype(&zelLoaderTranslateHandle) loaderTranslateHandleFunc = nullptr;
decltype(&zelSetDriverTeardown) setDriverTeardownFunc = nullptr;

void globalDriverSetup() {
    if (!globalDriverHandles) {
        globalDriverHandles = new std::vector<_ze_driver_handle_t *>;
    }
    NEO::OsLibraryCreateProperties loaderLibraryProperties("ze_loader.dll");
    loaderLibraryProperties.performSelfLoad = true;
    std::unique_ptr<NEO::OsLibrary> loaderLibrary = std::unique_ptr<NEO::OsLibrary>{NEO::OsLibrary::loadFunc(loaderLibraryProperties)};
    if (loaderLibrary) {
        loaderTranslateHandleFunc = reinterpret_cast<decltype(&zelLoaderTranslateHandle)>(loaderLibrary->getProcAddress("zelLoaderTranslateHandle"));
    }
}

void globalDriverTeardown(bool processTermination) {
    globalDriverDispatch.core.isValidFlag = false;
    globalDriverDispatch.tools.isValidFlag = false;
    globalDriverDispatch.sysman.isValidFlag = false;

    if (processTermination) {
        if (globalDriverHandles == nullptr) {
            return;
        }
        for (auto &globalDriverHandle : *globalDriverHandles) {
            auto globalDriver = static_cast<L0::DriverHandleImp *>(DriverHandle::fromHandle(globalDriverHandle));
            if (globalDriver == nullptr) {
                continue;
            }
            for (auto *device : globalDriver->devices) {
                device->getNEODevice()->stopDirectSubmissionAndWaitForCompletion();
            }
        }
        return;
    }

    if (levelZeroDriverInitialized) {
        NEO::OsLibraryCreateProperties loaderLibraryProperties("ze_loader.dll");
        loaderLibraryProperties.performSelfLoad = true;
        std::unique_ptr<NEO::OsLibrary> loaderLibrary = std::unique_ptr<NEO::OsLibrary>{NEO::OsLibrary::loadFunc(loaderLibraryProperties)};
        if (loaderLibrary) {
            setDriverTeardownFunc = reinterpret_cast<decltype(&zelSetDriverTeardown)>(loaderLibrary->getProcAddress("zelSetDriverTeardown"));
            if (setDriverTeardownFunc) {
                setDriverTeardownFunc();
            }
        } else {
            loaderTranslateHandleFunc = nullptr;
        }
    }

    if (globalDriverHandles) {
        for (auto &globalDriverHandle : *globalDriverHandles) {
            auto globalDriver = static_cast<L0::DriverHandleImp *>(DriverHandle::fromHandle(globalDriverHandle));
            if (globalDriver != nullptr) {

                if (globalDriver->pid == NEO::SysCalls::getCurrentProcessId()) {
                    delete static_cast<BaseDriver *>(globalDriver);
                }
                globalDriver = nullptr;
            }
        }
        delete globalDriverHandles;
        globalDriverHandles = nullptr;
    }
    if (Sysman::globalSysmanDriver != nullptr) {
        delete Sysman::globalSysmanDriver;
        Sysman::globalSysmanDriver = nullptr;
    }
}
} // namespace L0
