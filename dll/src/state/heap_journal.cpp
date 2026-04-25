#include "heap_journal.h"

#include "../logging.h"

namespace th08_platform::state {

void HeapJournal::begin_frame(std::uint64_t frame_number) noexcept
{
    th08_platform::log_line("heap_journal: begin_frame(%llu) - stub",
                            static_cast<unsigned long long>(frame_number));
}

void HeapJournal::record_alloc(void* ptr, std::size_t size) noexcept
{
    th08_platform::log_line("heap_journal: record_alloc(%p, %zu) - stub", ptr, size);
}

void HeapJournal::record_free(void* ptr) noexcept
{
    th08_platform::log_line("heap_journal: record_free(%p) - stub", ptr);
}

void HeapJournal::clear() noexcept
{
    th08_platform::log_line("heap_journal: clear() - stub");
}

}  // namespace th08_platform::state
