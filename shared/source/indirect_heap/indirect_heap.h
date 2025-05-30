/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/indirect_heap/heap_size.h"
#include "shared/source/indirect_heap/indirect_heap_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

class IndirectHeap : public LinearStream {
    using BaseClass = LinearStream;

  public:
    using Type = IndirectHeapType;
    IndirectHeap(void *buffer, size_t bufferSize) : BaseClass(buffer, bufferSize){};
    IndirectHeap(GraphicsAllocation *graphicsAllocation) : BaseClass(graphicsAllocation) {}
    IndirectHeap(GraphicsAllocation *graphicsAllocation, bool canBeUtilizedAs4GbHeap)
        : BaseClass(graphicsAllocation), canBeUtilizedAs4GbHeap(canBeUtilizedAs4GbHeap) {}

    void align(size_t alignment);
    uint64_t getHeapGpuStartOffset() const;
    uint64_t getHeapGpuBase() const;
    virtual uint32_t getHeapSizeInPages() const;

  protected:
    bool canBeUtilizedAs4GbHeap = false;
};

static_assert(NEO::NonCopyableAndNonMovable<IndirectHeap>);

inline void IndirectHeap::align(size_t alignment) {
    auto address = alignUp(ptrOffset(buffer, sizeUsed), alignment);
    sizeUsed = ptrDiff(address, buffer);
}

inline uint32_t IndirectHeap::getHeapSizeInPages() const {
    if (this->canBeUtilizedAs4GbHeap) {
        return MemoryConstants::sizeOf4GBinPageEntities;
    } else {
        return (static_cast<uint32_t>(getMaxAvailableSpace()) + MemoryConstants::pageMask) / MemoryConstants::pageSize;
    }
}

inline uint64_t IndirectHeap::getHeapGpuStartOffset() const {
    if (this->canBeUtilizedAs4GbHeap) {
        return this->graphicsAllocation->getGpuAddressToPatch();
    } else {
        return 0llu;
    }
}

inline uint64_t IndirectHeap::getHeapGpuBase() const {
    if (this->canBeUtilizedAs4GbHeap) {
        return this->graphicsAllocation->getGpuBaseAddress();
    } else {
        return this->graphicsAllocation->getGpuAddress();
    }
}

class ReservedIndirectHeap : public IndirectHeap {
  public:
    ReservedIndirectHeap(void *buffer, size_t bufferSize) : IndirectHeap(buffer, bufferSize) {}
    ReservedIndirectHeap(GraphicsAllocation *graphicsAllocation) : IndirectHeap(graphicsAllocation) {}
    ReservedIndirectHeap(GraphicsAllocation *graphicsAllocation, bool canBeUtilizedAs4GbHeap)
        : IndirectHeap(graphicsAllocation, canBeUtilizedAs4GbHeap) {}

    uint32_t getHeapSizeInPages() const override {
        return parentHeapSizeInPages;
    }
    void setHeapSizeInPages(uint32_t value) {
        parentHeapSizeInPages = value;
    }

  protected:
    uint32_t parentHeapSizeInPages = 0;
};

static_assert(NEO::NonCopyableAndNonMovable<ReservedIndirectHeap>);

} // namespace NEO
