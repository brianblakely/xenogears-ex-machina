#pragma once

#include "xem/reconstruction/disc_stream.hpp"
#include "xem/reconstruction/field_control.hpp"
#include "xem/reconstruction/field_return.hpp"
#include "xem/reconstruction/field_sprite_factory.hpp"
#include "xem/reconstruction/packed_field.hpp"

#include <memory>
#include <optional>
#include <string>

namespace xem::reconstruction {

inline constexpr std::string_view resident_executable_sha256 =
    "dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119";

// Source correlation for debugging. A script PC is never a machine-code address.
struct SourcePoint {
    std::string_view operation;
    std::uint32_t machine_address{};
    std::optional<std::size_t> actor;
    std::optional<std::uint32_t> event_pc;
    std::optional<field::FieldSpriteArguments> sprite_arguments{};
    std::optional<std::uint32_t> sprite_pc{};
};

class MissingDependency : public std::runtime_error {
  public:
    MissingDependency(SourcePoint source, std::string id, bool available, const char *reason)
        : std::runtime_error(reason), point(source), dependency(std::move(id)),
          recovered(available) {}
    SourcePoint point;
    std::string dependency;
    bool recovered;
};

struct FieldActor {
    // Original-layout correlations used by the recovered sprite/return source.
    // These bytes are owned, not pointers into an emulator or a native save format.
    field::original::Block<0x138> storage{};
    field::original::Block<0x5c> descriptor{};
    field::SpriteConstruction sprite{};
    field::original::Block<48> checkpoint{};
    std::optional<field::original::Block<12>> extension_110;
    std::optional<field::original::Block<16>> extension_114;

    [[nodiscard]] field::EventActor events() const;
    [[nodiscard]] field::ControlActor control() const;
};

struct ResidentState {
    field::EventVariables variables;
    std::uint32_t random_seed{};
    field::BattleRequestState battle_request;
    field::MusicLoadState music;
    field::DiscStreamState disc_stream;
    field::SpriteEnvironment sprite{};
    field::SpriteTaskState sprite_tasks{};
    std::uint16_t allocation_class{};
    std::uint32_t class_eight_context{};
    std::uint32_t allocation_cursor{};
    std::uint32_t field_return_mode{};
    // Original snapshot and original resource identities, not native persistence.
    field::original::Bytes field_snapshot;
    std::vector<std::uint32_t> party_sprite_resources;
};

struct FieldState {
    std::vector<FieldActor> actors;
    field::EventPackage event_package;
    field::CollisionPackage collision;
    field::original::ReturnGlobals globals{};
    std::uint32_t descriptor_count{};
    std::size_t snapshot_bytes_used{};
    std::uint32_t sprite_bundle_address{};
    std::int16_t sprite_gate{};
    std::uint32_t initialized_sprites{};
    std::uint32_t party_reassignment{};
    std::vector<field::SpriteAllocation> resources;
    std::vector<field::SpriteAllocation> frame_list;
    std::vector<std::uint8_t> trigonometry;
    std::vector<std::uint8_t> replay_widths;
    field::EventControl event_control;
    field::FieldPassState pass;
    field::ControlInputs control_inputs;
    field::ControlState control_state;
    std::uint8_t battle_mode_source{};
    std::int32_t single_actor_mode{};
    std::uint8_t party_processing_mode{};
    std::array<std::int32_t, 3> party_indices{255, 255, 255};
};

class Program;
// Read-only observation. Hosts may interrupt at a boundary; no callback supplies
// a computed game result. References expire when the callback returns.
using ProgramObserver = std::function<void(const Program &, SourcePoint, bool completed)>;

// A single owner for reusable recovered behavior. No case files, expectations,
// host clocks, presentation, or CPU emulation belong here. All borrowed views are
// constructed for a call; moving this object cannot leave internal dangling spans.
class Program {
  public:
    ResidentState resident;
    std::unique_ptr<FieldState> field;

    [[nodiscard]] field::FieldSpriteEnvironment sprite_environment() const;
    void set_sprite_environment(const field::FieldSpriteEnvironment &environment);

    // Narrow entry and the broader original 800a28d4 return branch use this same
    // 800a3474 implementation. Existing live sprites require original cleanup.
    void restore_field_data(const field::original::RestoreAllocation &allocate = {},
                            const ProgramObserver &observe = {});
    // Data restore -> all factories, in original order -> optional reassignment.
    // The later 800a3c8c checkpoint pass is NOT part of this original function.
    void restore_field(const field::SpriteAllocator &allocate, const field::SpriteReleaser &release,
                       const field::original::RestoreAllocation &allocate_extension = {},
                       const ProgramObserver &observe = {});

    // Semantic operations used by hosts and future native control. An event pass
    // is not a field update, a frame, or a guarantee of player-control readiness.
    [[nodiscard]] field::ScheduleResult event_pass(const ProgramObserver &observe = {});
    [[nodiscard]] field::BatchResult event_batch(std::size_t actor, std::int32_t limit,
                                                 const ProgramObserver &observe = {});

  private:
    void dispatch(field::EventContext &context, std::uint8_t opcode,
                  const ProgramObserver &observe);
};

} // namespace xem::reconstruction
