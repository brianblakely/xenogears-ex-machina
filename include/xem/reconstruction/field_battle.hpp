#pragma once

#include "xem/reconstruction/field_events.hpp"

#include <optional>

namespace xem::reconstruction::field {

// Original primary 71 handler at 80093568, in event_source_overlay_sha256.
// EVID-REF-031 qualifies the source and six original request calls. The request
// queues battle entry; combat, entry/return ownership and gate producers are separate.
struct BattleRequestState {
    std::uint32_t field_active{}; // 800adbdc
    std::uint32_t menu_gate{};    // 800adb2c
    std::uint32_t gate_90{};      // 800adb90; producer/meaning not yet recovered
    std::uint32_t pending{};      // 800adb88
    std::uint8_t selector{};      // 80059508
    std::uint8_t mode{};          // 8005954c
    std::uint8_t resident_flag{}; // 800594f8; downstream meaning remains open
    bool operator==(const BattleRequestState &) const = default;
};

enum class BattleRequestRetry {
    none,
    field_inactive,
    gate_e4_clear,
    gate_ec_clear,
    menu_gate_set,
    music_pending,
    gate_90_set
};

// Snapshot at 8009360c, after mode latching and selector resolution, before
// publication. Copies are observations; EventContext and BattleRequestState own state.
struct BattleRequestResolution {
    BattleRequestState state;
    EventControl control;
    std::uint16_t pc{};
    std::int32_t selector_value{};
};

struct BattleRequestResult {
    BattleRequestRetry retry{BattleRequestRetry::none};
    std::optional<BattleRequestResolution> resolution;
    [[nodiscard]] bool accepted() const noexcept { return retry == BattleRequestRetry::none; }
};

// EventContext owns working PC, break request and ADBE0/ADBE4/ADBEC gates. The
// music result (8004f308) and mode source (800b2356) remain caller-owned inputs.
// Bounded host reads reject unavailable operands/variables. Stores already issued
// before such an error remain visible; this does not emulate invalid PS1 memory.
[[nodiscard]] BattleRequestResult execute_battle_request(EventContext &context,
                                                         BattleRequestState &state,
                                                         std::uint32_t music_result,
                                                         std::uint8_t mode_source);

} // namespace xem::reconstruction::field
