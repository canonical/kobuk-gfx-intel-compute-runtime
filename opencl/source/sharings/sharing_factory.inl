/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/sharing_factory.h"

namespace NEO {

template <typename F, typename T>
SharingFactory::RegisterSharing<F, T>::RegisterSharing() {
    sharingContextBuilder[T::sharingId] = new F;
};
} // namespace NEO
