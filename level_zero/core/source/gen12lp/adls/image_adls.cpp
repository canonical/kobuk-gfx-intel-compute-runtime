/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/image/image_hw.inl"

namespace L0 {

template <>
struct ImageProductFamily<IGFX_ALDERLAKE_S> : public ImageCoreFamily<IGFX_GEN12LP_CORE> {
    using ImageCoreFamily::ImageCoreFamily;

    ze_result_t initialize(Device *device, const ze_image_desc_t *desc) override {
        return ImageCoreFamily<IGFX_GEN12LP_CORE>::initialize(device, desc);
    };
};

static ImagePopulateFactory<IGFX_ALDERLAKE_S, ImageProductFamily<IGFX_ALDERLAKE_S>> populateADLS;

} // namespace L0
