/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/compiler_interface.h"

#include "opencl/test/unit_test/mocks/mock_cif.h"

#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <functional>
#include <map>
#include <string>

namespace NEO {

struct MockCompilerDebugVars {
    bool forceBuildFailure = false;
    bool forceCreateFailure = false;
    bool forceRegisterFail = false;
    bool internalOptionsExpected = false;
    bool appendOptionsToFileName = true;
    void *debugDataToReturn = nullptr;
    size_t debugDataToReturnSize = 0;
    void *binaryToReturn = nullptr;
    size_t binaryToReturnSize = 0;
    bool failCreatePlatformInterface = false;
    bool failCreateGtSystemInfoInterface = false;
    bool failCreateIgcFeWaInterface = false;
    std::string *receivedInternalOptionsOutput = nullptr;
    std::string *receivedInput = nullptr;

    std::string fileName;
};

struct MockCompilerEnableGuard {
    MockCompilerEnableGuard(bool autoEnable = false);
    ~MockCompilerEnableGuard();

    void Enable();
    void Disable();

    const char *oldFclDllName;
    const char *oldIgcDllName;

    bool enabled = false;
};

void setFclDebugVars(MockCompilerDebugVars &dbgv);
void setIgcDebugVars(MockCompilerDebugVars &dbgv);
void clearFclDebugVars();
void clearIgcDebugVars();

MockCompilerDebugVars getFclDebugVars();
MockCompilerDebugVars getIgcDebugVars();

struct MockCIFPlatform : MockCIF<IGC::PlatformTagOCL> {
    IGC::TypeErasedEnum GetProductFamily() const override {
        return productFamily;
    }
    void SetProductFamily(IGC::TypeErasedEnum v) override {
        productFamily = v;
    }
    IGC::TypeErasedEnum GetRenderCoreFamily() const override {
        return renderCoreFamily;
    }
    void SetRenderCoreFamily(IGC::TypeErasedEnum v) override {
        renderCoreFamily = v;
    }

  protected:
    IGC::TypeErasedEnum productFamily;
    IGC::TypeErasedEnum renderCoreFamily;
};

struct MockGTSystemInfo : MockCIF<IGC::GTSystemInfoTagOCL> {
    uint32_t GetEUCount() const override {
        return this->euCount;
    }
    void SetEUCount(uint32_t v) override {
        euCount = v;
    }
    uint32_t GetThreadCount() const override {
        return this->threadCount;
    }
    void SetThreadCount(uint32_t v) override {
        threadCount = v;
    }
    uint32_t GetSliceCount() const override {
        return this->sliceCount;
    }
    void SetSliceCount(uint32_t v) override {
        sliceCount = v;
    }
    uint32_t GetSubSliceCount() const override {
        return this->subsliceCount;
    }
    void SetSubSliceCount(uint32_t v) override {
        subsliceCount = v;
    }

  protected:
    uint32_t euCount;
    uint32_t threadCount;
    uint32_t sliceCount;
    uint32_t subsliceCount;
};

struct MockIgcFeaturesAndWorkarounds : MockCIF<IGC::IgcFeaturesAndWorkaroundsTagOCL> {
};

struct MockIgcOclTranslationCtx : MockCIF<IGC::IgcOclTranslationCtxTagOCL> {
    using MockCIF<IGC::IgcOclTranslationCtxTagOCL>::TranslateImpl;
    MockIgcOclTranslationCtx();
    ~MockIgcOclTranslationCtx() override;

    IGC::OclTranslationOutputBase *TranslateImpl(
        CIF::Version_t outVersion,
        CIF::Builtins::BufferSimple *src,
        CIF::Builtins::BufferSimple *options,
        CIF::Builtins::BufferSimple *internalOptions,
        CIF::Builtins::BufferSimple *tracingOptions,
        uint32_t tracingOptionsCount) override;

    IGC::OclTranslationOutputBase *TranslateImpl(
        CIF::Version_t outVersion,
        CIF::Builtins::BufferSimple *src,
        CIF::Builtins::BufferSimple *options,
        CIF::Builtins::BufferSimple *internalOptions,
        CIF::Builtins::BufferSimple *tracingOptions,
        uint32_t tracingOptionsCount,
        void *gtpinInput) override;

    bool GetSpecConstantsInfoImpl(
        CIF::Builtins::BufferSimple *src,
        CIF::Builtins::BufferSimple *outSpecConstantsIds,
        CIF::Builtins::BufferSimple *outSpecConstantsSizes) override;

    IGC::OclTranslationOutputBase *TranslateImpl(
        CIF::Version_t outVersion,
        CIF::Builtins::BufferSimple *src,
        CIF::Builtins::BufferSimple *specConstantsIds,
        CIF::Builtins::BufferSimple *specConstantsValues,
        CIF::Builtins::BufferSimple *options,
        CIF::Builtins::BufferSimple *internalOptions,
        CIF::Builtins::BufferSimple *tracingOptions,
        uint32_t tracingOptionsCount,
        void *gtPinInput) override;
};

struct MockOclTranslationOutput : MockCIF<IGC::OclTranslationOutputTagOCL> {
    MockOclTranslationOutput();
    ~MockOclTranslationOutput() override;
    CIF::Builtins::BufferBase *GetBuildLogImpl(CIF::Version_t bufferVersion) override;
    CIF::Builtins::BufferBase *GetOutputImpl(CIF::Version_t bufferVersion) override;
    CIF::Builtins::BufferBase *GetDebugDataImpl(CIF::Version_t bufferVersion) override;

    bool Successful() const override {
        return failed == false;
    }

    void setError() {
        setError("");
    }
    void setError(const std::string &message);
    void setOutput(const void *data, size_t dataLen);
    void setDebugData(const void *data, size_t dataLen);

    bool failed = false;
    MockCIFBuffer *output = nullptr;
    MockCIFBuffer *log = nullptr;
    MockCIFBuffer *debugData = nullptr;
};

struct MockIgcOclDeviceCtx : MockCIF<IGC::IgcOclDeviceCtx<2>> {
    static CIF::ICIF *Create(CIF::InterfaceId_t intId, CIF::Version_t version);

    MockIgcOclDeviceCtx();
    ~MockIgcOclDeviceCtx() override;

    IGC::PlatformBase *GetPlatformHandleImpl(CIF::Version_t ver) override {
        if (getIgcDebugVars().failCreatePlatformInterface) {
            return nullptr;
        }
        return platform;
    }

    IGC::GTSystemInfoBase *GetGTSystemInfoHandleImpl(CIF::Version_t ver) override {
        if (getIgcDebugVars().failCreateGtSystemInfoInterface) {
            return nullptr;
        }
        return gtSystemInfo;
    }

    IGC::IgcFeaturesAndWorkaroundsBase *GetIgcFeaturesAndWorkaroundsHandleImpl(CIF::Version_t ver) override {
        if (getIgcDebugVars().failCreateIgcFeWaInterface) {
            return nullptr;
        }
        return igcFeWa;
    }

    IGC::IgcOclTranslationCtxBase *CreateTranslationCtxImpl(CIF::Version_t ver,
                                                            IGC::CodeType::CodeType_t inType,
                                                            IGC::CodeType::CodeType_t outType) override;

    bool GetSystemRoutine(IGC::SystemRoutineType::SystemRoutineType_t typeOfSystemRoutine,
                          bool bindless,
                          CIF::Builtins::BufferSimple *outSystemRoutineBuffer,
                          CIF::Builtins::BufferSimple *stateSaveAreaHeaderInit) override;

    void SetDebugVars(MockCompilerDebugVars &debugVars) {
        this->debugVars = debugVars;
    }

    MockCIFPlatform *platform = nullptr;
    MockGTSystemInfo *gtSystemInfo = nullptr;
    MockIgcFeaturesAndWorkarounds *igcFeWa = nullptr;
    MockCompilerDebugVars debugVars;

    using TranslationOpT = std::pair<IGC::CodeType::CodeType_t, IGC::CodeType::CodeType_t>;
    std::vector<TranslationOpT> requestedTranslationCtxs;
};

struct MockFclOclTranslationCtx : MockCIF<IGC::FclOclTranslationCtxTagOCL> {
    MockFclOclTranslationCtx();
    ~MockFclOclTranslationCtx() override;

    IGC::OclTranslationOutputBase *TranslateImpl(
        CIF::Version_t outVersion,
        CIF::Builtins::BufferSimple *src,
        CIF::Builtins::BufferSimple *options,
        CIF::Builtins::BufferSimple *internalOptions,
        CIF::Builtins::BufferSimple *tracingOptions,
        uint32_t tracingOptionsCount) override;
};

struct MockFclOclDeviceCtx : MockCIF<IGC::FclOclDeviceCtxTagOCL> {
    MockFclOclDeviceCtx();
    ~MockFclOclDeviceCtx() override;

    static CIF::ICIF *Create(CIF::InterfaceId_t intId, CIF::Version_t version);
    void SetOclApiVersion(uint32_t version) override {
        oclApiVersion = version;
    }

    IGC::FclOclTranslationCtxBase *CreateTranslationCtxImpl(CIF::Version_t ver,
                                                            IGC::CodeType::CodeType_t inType,
                                                            IGC::CodeType::CodeType_t outType) override;

    IGC::FclOclTranslationCtxBase *CreateTranslationCtxImpl(CIF::Version_t ver,
                                                            IGC::CodeType::CodeType_t inType,
                                                            IGC::CodeType::CodeType_t outType,
                                                            CIF::Builtins::BufferSimple *err) override;

    uint32_t oclApiVersion = 120;
};

} // namespace NEO
