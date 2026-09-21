#pragma once

#include "xem/reconstruction/field_sprite.hpp"

namespace xem::reconstruction::field {

struct SpriteModelState {
    std::uint16_t material_page{}, material_palette{}; // 80059308 / 8005930c
    std::uint32_t palette_base{}, palette_mode{};      // 80059314 / 8005010c
    std::uint32_t primitive_count{};                   // 800595c0
    std::uint32_t output{}, shading{}, geometry{}, normals{}, vertices{}, auxiliary{};
    // Original primitive packets retain allocator bytes not written by their
    // constructors. Both frame buffers belong to a single allocation.
    std::vector<SpriteAllocation> buffers;
};

// Resident 8002c59c. Mutates the owned loaded resource in original store order;
// repeated calls preserve already-relocated headers.
void relocate_sprite_model(std::uint32_t model, const SpriteSources &sources);

// F5's 8002cb54 / 8002c8cc path, mode zero. Only independently recovered
// dispatch handlers are supported; no draw, projection or rasterizer is implied.
void construct_sprite_model(SpriteWindow sprite, std::uint32_t model,
                            SpriteEnvironment &environment, const SpriteSources &sources);

} // namespace xem::reconstruction::field
