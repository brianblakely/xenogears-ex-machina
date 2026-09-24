#pragma once

#include "xem/reconstruction/field_gte.hpp"
#include "xem/reconstruction/field_sprite.hpp"

namespace xem::reconstruction::field {

// The resident model renderer's state: packet initialization (8002c8cc) and
// drawing (8002c700 and the primitive routines of table 8004fe50).
struct SpriteModelState {
    std::uint16_t material_page{}, material_palette{}; // 80059308 / 8005930c
    std::uint32_t palette_base{}, palette_mode{};      // 80059314 / 8005010c
    std::uint32_t primitive_count{};                   // 800595c0
    std::uint32_t output{};                            // 80059424: next packet
    std::uint32_t shading{};                           // 80059538: model +14
    std::uint32_t geometry{};                          // 80059528: current primitive record
    std::uint32_t normals{};                           // 8005952c: model +c
    std::uint32_t vertices{};                          // 8005953c: model +8
    std::uint32_t auxiliary{};                         // 80059498: model +18
    std::uint32_t table{};                             // 80059568: ordering table being drawn into
    std::uint32_t drawn{};                             // 80059578: primitives past the screen tests
    std::array<std::uint8_t, 3> fog_color{};           // 80059598
    std::uint32_t x_limit{};                           // 800500f8
    std::uint32_t y_limit{};                           // 800500fc: compared with whole SXY words
    std::uint32_t depth_shift{};                       // 80050100
    std::uint32_t lod{};                               // 80050104: nonzero runs 8003101c first
    GteMatrix light_source{};                          // 80059f64 (rotation)
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
