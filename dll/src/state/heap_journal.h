#pragma once

#include <cstddef>
#include <cstdint>

namespace th08_platform::state {

class HeapJournal {
public:
    void begin_frame(std::uint64_t frame_number) noexcept;
    void record_alloc(void* ptr, std::size_t size) noexcept;
    void record_free(void* ptr) noexcept;
    void clear() noexcept;
};

}  // namespace th08_platform::state
