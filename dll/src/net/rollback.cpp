#include "rollback.h"

#include "crc32.h"
#include "lockstep.h"

#include "../hooks/game_loop.h"
#include "../logging.h"
#include "../state/globals.h"

#include <windows.h>
#include <MinHook.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>

namespace th08_platform::net::rollback {

namespace {

constexpr DWORD kPlaySoundByIdxRVA = 0x45d550 - 0x400000;
constexpr DWORD kPlaySoundPositionedByIdxRVA = 0x45d660 - 0x400000;

using PlaySoundByIdx_t = void(__thiscall*)(void*, int, unsigned int);
using PlaySoundPositionedByIdx_t = void(__thiscall*)(void*, int, float);

PlaySoundByIdx_t g_original_PlaySoundByIdx = nullptr;
PlaySoundPositionedByIdx_t g_original_PlaySoundPositionedByIdx = nullptr;

thread_local bool s_in_rollback = false;

struct FrameMeta {
    std::uint64_t frame = 0;
    std::uint16_t local_input = 0;
    std::uint16_t predicted_remote_input = 0;
    std::uint16_t confirmed_remote_input = 0;
    std::uint32_t local_checksum = 0;
    std::uint32_t remote_checksum = 0;
    bool confirmed = false;
    bool desync_logged = false;
};

RollbackBuffer g_rollback;
std::map<std::uint64_t, FrameMeta> g_frames;
bool g_desync_detected = false;

std::size_t slot_for(std::uint64_t frame_number) noexcept
{
    return static_cast<std::size_t>(frame_number % kMaxRollbackFrames);
}

state::FrameState* snapshot_for(std::uint64_t frame_number) noexcept
{
    auto& snapshot = g_rollback.history[slot_for(frame_number)];
    return snapshot.frame_number == frame_number ? &snapshot : nullptr;
}

FrameMeta& frame_meta(std::uint64_t frame_number)
{
    auto& meta = g_frames[frame_number];
    meta.frame = frame_number;
    return meta;
}

void prune_old_frames(std::uint64_t current_frame)
{
    const std::uint64_t min_frame =
        current_frame > (kMaxRollbackFrames * 2) ? current_frame - (kMaxRollbackFrames * 2) : 0;

    for (auto it = g_frames.begin(); it != g_frames.end();) {
        if (it->first < min_frame) {
            it = g_frames.erase(it);
        } else {
            ++it;
        }
    }
}

int __fastcall hooked_PlaySoundByIdx(void* self, void*, int idx, unsigned int unused)
{
    if (s_in_rollback) {
        return 0;
    }

    g_original_PlaySoundByIdx(self, idx, unused);
    return 0;
}

int __fastcall hooked_PlaySoundPositionedByIdx(void* self, void*, int idx, float pan)
{
    if (s_in_rollback) {
        return 0;
    }

    g_original_PlaySoundPositionedByIdx(self, idx, pan);
    return 0;
}

void* resolve_rva(DWORD rva)
{
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) {
        return nullptr;
    }

    return reinterpret_cast<void*>(reinterpret_cast<std::uint8_t*>(base) + rva);
}

bool restore_and_replay(void* game_manager, std::uint64_t target_frame,
                        std::uint64_t current_frame)
{
    state::FrameState* snapshot = snapshot_for(target_frame);
    if (!snapshot) {
        th08_platform::log_line("rollback skipped at frame %llu: history expired",
                                static_cast<unsigned long long>(target_frame));
        return false;
    }

    th08_platform::state::restore(*snapshot);

    s_in_rollback = true;
    for (std::uint64_t frame = target_frame; frame <= current_frame; ++frame) {
        th08_platform::hooks::run_original_update(game_manager);

        const std::uint64_t next_frame = frame + 1;
        th08_platform::state::capture(g_rollback.history[slot_for(next_frame)], next_frame);
        if (next_frame <= current_frame) {
            auto& next_meta = frame_meta(next_frame);
            next_meta.local_checksum =
                th08_platform::net::checksum(g_rollback.history[slot_for(next_frame)]);
        }
    }
    s_in_rollback = false;

    return true;
}

void log_confirmed_frame(const FrameMeta& meta)
{
    th08_platform::log_line("synced frame %llu: local=0x%04x remote=0x%04x",
                            static_cast<unsigned long long>(meta.frame),
                            meta.local_input, meta.confirmed_remote_input);
}

}  // namespace

bool install_audio_hooks()
{
    void* play_sound_target = resolve_rva(kPlaySoundByIdxRVA);
    void* play_sound_positioned_target = resolve_rva(kPlaySoundPositionedByIdxRVA);
    if (!play_sound_target || !play_sound_positioned_target) {
        th08_platform::log_line("rollback audio hook: resolve_target failed");
        return false;
    }

    if (MH_CreateHook(play_sound_target, reinterpret_cast<void*>(&hooked_PlaySoundByIdx),
                      reinterpret_cast<LPVOID*>(&g_original_PlaySoundByIdx)) != MH_OK) {
        th08_platform::log_line("rollback audio hook: MH_CreateHook PlaySoundByIdx failed");
        return false;
    }
    if (MH_EnableHook(play_sound_target) != MH_OK) {
        th08_platform::log_line("rollback audio hook: MH_EnableHook PlaySoundByIdx failed");
        return false;
    }

    if (MH_CreateHook(play_sound_positioned_target,
                      reinterpret_cast<void*>(&hooked_PlaySoundPositionedByIdx),
                      reinterpret_cast<LPVOID*>(&g_original_PlaySoundPositionedByIdx)) != MH_OK) {
        th08_platform::log_line(
            "rollback audio hook: MH_CreateHook PlaySoundPositionedByIdx failed");
        return false;
    }
    if (MH_EnableHook(play_sound_positioned_target) != MH_OK) {
        th08_platform::log_line(
            "rollback audio hook: MH_EnableHook PlaySoundPositionedByIdx failed");
        return false;
    }

    return true;
}

void reset()
{
    g_rollback = {};
    g_frames.clear();
    g_desync_detected = false;
}

void on_frame_start(std::uint64_t frame_number)
{
    g_rollback.local_frame = frame_number;

    auto& snapshot = g_rollback.history[slot_for(frame_number)];
    th08_platform::state::capture(snapshot, frame_number);

    auto& meta = frame_meta(frame_number);
    meta.local_input = *th08_platform::globals::at<std::uint16_t>(
        th08_platform::globals::kAddr_g_CurFrameInput);

    if (const auto confirmed = th08_platform::net::peek_confirmed_frame(frame_number);
        confirmed.has_value()) {
        meta.predicted_remote_input = confirmed->input;
    } else {
        meta.predicted_remote_input = th08_platform::net::last_known_remote_input();
    }
}

void on_frame_end(void* game_manager, std::uint64_t frame_number)
{
    auto& meta = frame_meta(frame_number);
    meta.local_checksum = th08_platform::net::checksum(
        g_rollback.history[slot_for(frame_number)]);

    th08_platform::net::send_local_input(frame_number, meta.local_input, meta.local_checksum);

    const auto confirmed_frames =
        th08_platform::net::consume_confirmed_frames_up_to(frame_number);

    std::optional<std::uint64_t> rollback_target;

    for (const auto& confirmed : confirmed_frames) {
        auto& confirmed_meta = frame_meta(confirmed.frame);
        confirmed_meta.confirmed = true;
        confirmed_meta.confirmed_remote_input = confirmed.input;
        confirmed_meta.remote_checksum = confirmed.checksum;
        g_rollback.confirmed_frame =
            std::max(g_rollback.confirmed_frame, static_cast<std::uint64_t>(confirmed.frame));

        log_confirmed_frame(confirmed_meta);

        if (!confirmed_meta.desync_logged && confirmed_meta.local_checksum != 0 &&
            confirmed_meta.local_checksum != confirmed.checksum) {
            confirmed_meta.desync_logged = true;
            g_desync_detected = true;
            th08_platform::log_line(
                "desync detected at frame %llu: local_crc=0x%08x remote_crc=0x%08x",
                static_cast<unsigned long long>(confirmed.frame),
                confirmed_meta.local_checksum, confirmed.checksum);
        }

        if (!g_desync_detected &&
            confirmed_meta.predicted_remote_input != confirmed.input &&
            !rollback_target.has_value()) {
            rollback_target = confirmed.frame;
            th08_platform::log_line(
                "rollback at frame %llu (predicted=0x%04x, actual=0x%04x)",
                static_cast<unsigned long long>(confirmed.frame),
                confirmed_meta.predicted_remote_input, confirmed.input);
        }
    }

    if (rollback_target.has_value()) {
        restore_and_replay(game_manager, *rollback_target, frame_number);
    }

    prune_old_frames(frame_number);
}

bool is_replaying()
{
    return s_in_rollback;
}

}  // namespace th08_platform::net::rollback
