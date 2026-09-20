#pragma once

#include "xem/reconstruction/field_sprite.hpp"

namespace xem::reconstruction::field {
using SpriteReleaser = std::function<void(std::uint32_t address)>;

// Resident 8001f5bc's three scaled output words in original argument order.
[[nodiscard]] std::array<std::int32_t, 3> initial_sprite_bounds(SpriteWindow sprite,
                                                                const SpriteSources &sources);
struct SpriteTaskState {
    std::uint32_t wait_count; // 80059428
    std::uint16_t wait_flag;  // 80059494
    std::uint32_t current;    // 800594c0
    std::uint32_t head;       // 8005958c
    std::uint32_t next;       // 80059590
};
// Resident 8001c964; nonzero task callbacks remain explicit source blockers.
void advance_sprite_tasks(SpriteTaskState &state, const SpriteSources &sources);
struct FieldSpriteEnvironment {
    SpriteEnvironment sprite;
    std::uint32_t return_mode;         // 8004f30c
    std::int16_t field_gate;           // 800b218e
    std::uint32_t initialized_count;   // 800afc74
    std::uint16_t allocation_class;    // 8005931c
    std::uint32_t class_eight_context; // 80059fc4
    std::uint32_t allocation_cursor;   // 80059330
    SpriteTaskState tasks;
};
struct FieldSpriteArguments {
    std::uint32_t actor_index;
    std::uint32_t resource_slot;
    std::uint32_t resource;
    std::uint32_t mode;
    std::uint32_t part_variant;
    std::uint8_t tag;
    std::uint32_t defer_initial_step;
};
// Field 80076ac0. Borrows the actual recreated actor and its descriptor; publishes
// owned sprite/parts into result as they are created, retaining partial state on
// failure. Result must initially own no allocations. Mode-zero coordinates are read at
// 800b1f78+slot*8 from sources. Existing-sprite destruction, alternate part variants, unknown VM
// commands and task callbacks fail explicitly, never invoke behavior callbacks.
void create_field_sprite(SpriteConstruction &result, std::span<std::uint8_t> actor,
                         std::span<std::uint8_t> descriptor, const FieldSpriteArguments &arguments,
                         FieldSpriteEnvironment &environment, const SpriteSources &sources,
                         const SpriteAllocator &allocate, const SpriteReleaser &release,
                         const SpriteConstructionObserver &observe = {});
} // namespace xem::reconstruction::field
