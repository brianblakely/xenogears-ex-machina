// Field overlay 800752c8: the character pass. The resident sprite system
// switches to this buffer's arena, draws scheduled sprite frames, runs its
// task lists (child models and sprite updates), then the field places and
// draws each actor's billboard sprite, advances actor sprite timers and draws
// shadows. Original addresses name correlations only.
#include "xem/reconstruction/gpu.hpp"
#include "xem/reconstruction/program.hpp"
#include "xem/reconstruction/resident_gte.hpp"

#include <bit>

namespace xem::reconstruction {
namespace {
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::int32_t s8(std::uint32_t value) {
    return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(value));
}
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
// GetClut (80043a58).
std::uint32_t clut(std::int32_t x, std::int32_t y) {
    return (u32(y) << 6U | (u32(x >> 4) & 0x3fU)) & 0xffffU;
}
void observed(const ProgramObserver &observe, const Program &program, std::string_view operation,
              std::uint32_t address) {
    if (observe)
        observe(program, {operation, address, {}, {}}, true);
}
} // namespace

field::GteMatrix Program::memory_matrix(std::uint32_t address) const {
    field::GteMatrix m{};
    for (std::uint32_t i = 0; i < 9; ++i)
        m.r[i] = static_cast<std::int16_t>(s16(memory(address + i * 2, 2)));
    m.pad = static_cast<std::int16_t>(s16(memory(address + 0x12, 2)));
    for (std::uint32_t i = 0; i < 3; ++i)
        m.t[i] = s32(memory(address + 0x14 + i * 4));
    return m;
}

void Program::set_memory_matrix(std::uint32_t address, const field::GteMatrix &m) {
    for (std::uint32_t i = 0; i < 9; ++i)
        set_memory(address + i * 2, static_cast<std::uint16_t>(m.r[i]), 2);
    set_memory(address + 0x12, static_cast<std::uint16_t>(m.pad), 2);
    for (std::uint32_t i = 0; i < 3; ++i)
        set_memory(address + 0x14 + i * 4, u32(m.t[i]));
}

void Program::with_sprite_sources(const std::function<void(const field::SpriteSources &)> &call,
                                  std::optional<std::uint32_t> actor) {
    auto &state = *field;
    std::vector<field::SpriteResource> resources;
    for (const auto &item : state.resources)
        resources.push_back({item.address, item.bytes});
    // Allocated heap blocks no record interprets (the party sprite files a
    // reload keeps) are readable sprite data too.
    for (const auto &[address, bytes] : resident.heap_contents)
        resources.push_back({address, bytes});
    std::vector<field::SpriteResource> frames;
    for (const auto &item : state.frame_list)
        frames.push_back({item.address, item.bytes});
    for (const auto &owned : state.actors)
        if (!owned.sprite.sprite.bytes.empty())
            frames.push_back({owned.sprite.sprite.address, owned.sprite.sprite.bytes});
    field::SpriteSources sources{};
    sources.resources = resources;
    sources.frame_list = frames;
    sources.trigonometry = resident.math.trigonometry;
    sources.replay_widths = state.replay_widths;
    std::vector<field::SpriteWindow> mutable_resources;
    for (auto &resource : state.resources)
        mutable_resources.push_back({resource.address, resource.bytes});
    sources.mutable_resources = mutable_resources;
    sources.tasks = &resident.sprite_tasks;
    sources.models = &resident.sprite_models;
    sources.heap = &resident.sprite_heap;
    sources.allocator = &resident.heap;
    if (actor)
        sources.field_actor = field::SpriteFieldActor{*actor, state.actors.at(*actor).storage};
    call(sources);
}

// Resident 800250e0: the sprite system's per-buffer arena and deferred frees.
void Program::sprite_buffer_begin(std::uint32_t buffer) {
    auto &r = resident;
    const auto releases = r.sprite_releases.at(buffer);
    r.sprite_buffer = buffer;
    r.sprite_arena_cursor = r.sprite_arenas.at(buffer);
    r.sprite_arena_start = r.sprite_arenas.at(buffer);
    r.sprite_arena_end = r.sprite_arenas.at(buffer) + r.sprite_arena_bytes;
    if (releases != 0)
        throw MissingDependency({"sprite_buffer_begin", 0x80025138, {}, {}},
                                "symbol:sprite-deferred-release", false,
                                "Deferred sprite block releases are not recovered");
    r.sprite_releases.at(buffer) = 0;
}

// Resident 800251c8: queue an image upload (a clear when `source` is 0) on
// this buffer's list, in its sprite arena. A full arena drops the request.
void Program::sprite_upload(std::uint32_t source, std::int32_t x, std::int32_t y,
                            std::uint32_t width, std::uint32_t height) {
    auto &r = resident;
    const auto node = r.sprite_arena_cursor;
    if (node + 0x10 >= r.sprite_arena_end)
        return;
    set_memory(node + 6, height, 2);
    set_memory(node, u32(x), 2);
    set_memory(node + 2, u32(y), 2);
    set_memory(node + 4, width, 2);
    set_memory(node + 8, source);
    r.sprite_arena_cursor = node + 0x10;
    set_memory(node + 0xc, r.sprite_uploads.at(r.sprite_buffer));
    r.sprite_uploads.at(r.sprite_buffer) = node;
}

// The frame record's scaled extents (+3 and +1 by the sprite scale +2c)
// become the sprite's +36/+38.
void Program::sprite_frame_scale(std::uint32_t sprite, std::uint32_t record) {
    const auto scale = s16(memory(sprite + 0x2c, 2));
    const auto scaled = [&](std::uint32_t at) {
        return u32(static_cast<std::int32_t>(memory(record + at, 1)) * scale / 4096);
    };
    set_memory(sprite + 0x36, scaled(3), 2);
    set_memory(sprite + 0x38, scaled(1), 2);
}

// The control bytes before each part of 8001dae8 and 8001d53c: flag bytes
// (bit 7) set part +8/+9 and flag 0x20, or with bit 6 select and fill an
// auxiliary record (renderer +34, 8 bytes each). Returns the stream at the
// part's own bytes; `group` keeps the last selected record.
std::uint32_t Program::sprite_part_controls(std::uint32_t sprite, std::uint32_t part,
                                            std::uint32_t stream, std::uint32_t &group) {
    set_memory(part + 9, 0, 1);
    set_memory(part + 8, 0, 1);
    set_memory(part + 0x14, memory(part + 0x14) & ~0x20U);
    for (;;) {
        const auto control = memory(stream, 1);
        if ((control & 0x80U) == 0)
            return stream;
        ++stream;
        if ((control & 0x40U) == 0) {
            if ((control & 4U) != 0)
                set_memory(part + 0x14, memory(part + 0x14) | 0x20U);
            if ((control & 1U) != 0)
                set_memory(part + 8, memory(stream++, 1), 1);
            if ((control & 2U) != 0)
                set_memory(part + 9, memory(stream++, 1), 1);
            continue;
        }
        const auto renderer = memory(sprite + 0x20);
        if (memory(renderer + 0x34) == 0)
            throw MissingDependency({"sprite_part_controls", 0x80031bdc, {}, {}},
                                    "symbol:sprite-auxiliary-allocation", false,
                                    "Allocating a sprite's auxiliary records is not recovered");
        group = control & 7U;
        const auto record = memory(renderer + 0x34) + group * 8;
        if ((control & 0x20U) != 0) {
            set_memory(record, memory(stream, 1), 1);
            set_memory(record + 1, memory(stream + 1, 1), 1);
            stream += 2;
        }
        if ((control & 0x10U) != 0)
            set_memory(record + 6, memory(stream++, 1) << 4U, 2);
        else
            set_memory(record + 6, 0, 2);
    }
}

// A part's offsets: signed bytes, or with the frame's wide flag signed
// 16-bit little-endian pairs. Returns the stream after the part.
std::uint32_t Program::sprite_part_offsets(std::uint32_t part, std::uint32_t stream, bool wide) {
    const auto half = [&](std::uint32_t at) {
        return memory(at, 1) | u32(s8(memory(at + 1, 1)) << 8);
    };
    if (wide) {
        set_memory(part, half(stream + 1), 2);
        set_memory(part + 2, half(stream + 3), 2);
        stream += 2;
    } else {
        set_memory(part, u32(s8(memory(stream + 1, 1))), 2);
        set_memory(part + 2, u32(s8(memory(stream + 2, 1))), 2);
    }
    return stream + 3;
}

// Resident 8001dae8: build frame `frame` of a sprite's frame table into its
// parts (renderer +30, 0x18 bytes each) and queue the image cells it uses.
// The binding (sprite +24) holds the table, the image origin (+4), the
// palette origin (+8) and palette (+c).
void Program::build_sprite_frame(std::uint32_t sprite, std::uint32_t frame) {
    set_memory(sprite + 0x40, memory(sprite + 0x40) & 0xfff5ffffU);
    const auto binding = memory(sprite + 0x24);
    const auto table = memory(binding);
    auto part = memory(memory(sprite + 0x20) + 0x30);
    if (s32(frame) >= static_cast<std::int32_t>(memory(table, 2) & 0x1ffU) + 1)
        return;
    const auto palette_x = s16(memory(binding + 8, 2));
    const auto palette_y = s16(memory(binding + 0xa, 2));
    if (const auto flags = memory(sprite + 0x3c); (flags & 0x40000000U) != 0) {
        set_memory(sprite + 0x3c, flags & 0xbfffffffU);
        const auto palette = memory(binding + 0xc);
        if (const auto colors = memory(palette, 2); colors != 0)
            sprite_upload(palette + 4 + colors * (memory(sprite + 0x3e, 2) & 0xf0U) * 2, palette_x,
                          palette_y, u32(s16(colors << 4U)), 1);
    }
    if ((memory(table, 2) & 0x8000U) != 0) {
        build_sprite_cell_frame(sprite, frame);
        return;
    }
    const auto origin = memory(binding + 4);
    const auto x = origin & 0xffffU;
    const auto y = origin >> 16U;
    const auto record = table + memory(table + frame * 2, 2);
    if (((memory(sprite + 0x40) >> 13U) & 0xfU) == 0xe)
        throw MissingDependency({"build_sprite_frame", 0x8001f530, {}, {}},
                                "symbol:sprite-frame-origin-mode-e", false,
                                "Frame origin selection of sprite mode 0xe is not recovered");
    sprite_frame_scale(sprite, record);
    const auto head = memory(record, 1);
    const auto count = head & 0x3fU;
    const auto blend = memory(sprite + 0x3c, 1) >> 5U;
    const auto color = memory(sprite + 0x28);
    auto cells = record + 6;
    auto stream = cells + count * 4;
    std::uint32_t group = 4;
    std::uint32_t built = 0;
    for (; built != count; ++built, part += 0x18) {
        stream = sprite_part_controls(sprite, part, stream, group);
        const auto placement = memory(cells + 2, 2);
        const auto cell_x = placement & 0x1fU;
        const auto cell_y = (placement >> 5U) & 0x3fU;
        const auto cell = table + memory(cells, 2) * 4;
        cells += 4;
        const auto eight_bit = (memory(cell + 2, 2) & 1U) != 0;
        auto flags = memory(part + 0x14);
        std::uint32_t u = 0;
        std::uint32_t words = 0;
        if (eight_bit) {
            u = (x & 0x3fU) * 2 + cell_x * 2;
            words = memory(cell, 1) >> 1U;
            flags |= 8U;
        } else {
            u = (x & 0x3fU) * 4 + cell_x * 4;
            words = memory(cell, 1) >> 2U;
            flags &= ~8U;
        }
        set_memory(part + 4, u, 1);
        flags = (flags & ~7U) | group;
        set_memory(part + 5, cell_y + (y & 0xffU), 1);
        set_memory(part + 6, memory(cell, 1), 1);
        set_memory(part + 7, memory(cell + 1, 1), 1);
        const auto control = memory(stream, 1);
        flags = (flags & ~0x10U) | ((control >> 2U) & 0x10U);
        set_memory(part + 0x14, flags);
        set_memory(part + 0x10, color);
        // Semi-transparency: the part's rate, else the sprite's.
        auto rate = (control >> 4U) & 3U;
        if (rate == 0)
            rate = blend;
        if (rate != 0) {
            --rate;
            set_memory(part + 0x13, memory(part + 0x13, 1) | 2U, 1);
        }
        set_memory(part + 0xa,
                   gpu::texture_page(eight_bit ? 1U : 0U, rate, static_cast<std::int32_t>(x),
                                     static_cast<std::int32_t>(y)),
                   2);
        set_memory(part + 0xc,
                   clut(palette_x + static_cast<std::int32_t>((control & 0xfU) * 16), palette_y),
                   2);
        sprite_upload(cell + 4, s16(x + cell_x), s16(y + cell_y), words, memory(cell + 1, 1));
        stream = sprite_part_offsets(part, stream, (head & 0x80U) != 0);
    }
    set_memory(sprite + 0x40, (memory(sprite + 0x40) & 0xffffff03U) | (built & 0x3fU) << 2U);
    sprite_upload(0, s16(x), s16(y), memory(record + 4, 1), memory(record + 5, 1));
}

// Resident 8001d53c: frames of tables flagged 0x8000 draw cells already in
// VRAM; each cell record gives its own texture position and size.
void Program::build_sprite_cell_frame(std::uint32_t sprite, std::uint32_t frame) {
    const auto binding = memory(sprite + 0x24);
    const auto table = memory(binding);
    const auto record = table + memory(table + frame * 2, 2);
    sprite_frame_scale(sprite, record);
    const auto head = memory(record, 1);
    const auto count = head & 0x3fU;
    auto part = memory(memory(sprite + 0x20) + 0x30);
    auto v_base = memory(binding + 6, 1);
    const auto blend = memory(sprite + 0x3c, 1) >> 5U;
    const auto color = memory(sprite + 0x28);
    auto cells = record + 4;
    auto stream = cells + count * 2;
    std::uint32_t group = 4;
    std::uint32_t built = 0;
    for (; built != count; ++built, part += 0x18) {
        stream = sprite_part_controls(sprite, part, stream, group);
        auto cell = table + memory(cells, 2);
        cells += 2;
        auto kind = memory(cell, 1);
        auto flags = memory(part + 0x14);
        std::uint32_t u_base = 0;
        if ((kind & 1U) != 0) {
            flags |= 8U;
            u_base = (memory(binding + 4, 2) & 0x3fU) >> 1U;
        } else {
            flags &= ~8U;
            u_base = (memory(binding + 4, 2) & 0x3fU) >> 2U;
        }
        set_memory(part + 0x14, flags);
        const auto control = memory(stream, 1);
        set_memory(part + 0x10, color);
        auto rate = (control >> 4U) & 3U;
        if (rate == 0)
            rate = blend;
        if (rate != 0) {
            --rate;
            set_memory(part + 0x13, memory(part + 0x13, 1) | 2U, 1);
        }
        const auto page_rate = u32(s16(rate));
        if ((kind & 0x10U) != 0) {
            // A two-byte kind names a resident texture origin (8004fab8) and
            // a palette row from 0x1cc.
            ++cell;
            kind |= memory(cell, 1) << 8U;
            const auto origin = 0x8004fab8U + ((kind << 1U) & 0x1cU);
            set_memory(part + 0xa,
                       gpu::texture_page(kind & 1U, page_rate, s16(memory(origin, 2)),
                                         s16(memory(origin + 2, 2))),
                       2);
            set_memory(part + 0xc,
                       clut(static_cast<std::int32_t>((kind >> 1U) & 0xf0U),
                            static_cast<std::int32_t>(((kind >> 9U) & 0xfU) + 0x1cc)),
                       2);
        } else {
            // Sprites flagged +a8 bit 0 may take texture origins from their
            // resource's page table (+7c, then +18).
            const auto paged = (memory(sprite + 0xa8) & 1U) == 1;
            const auto pages = paged ? memory(memory(sprite + 0x7c) + 0x18) : 0U;
            if (pages != 0) {
                const auto page = pages + ((kind << 1U) & 0x1cU);
                const auto page_x = memory(page, 2);
                const auto page_y = memory(page + 2, 2);
                v_base = page_y & 0xffU;
                u_base = (page_x & 0x3fU) >> 2U;
                set_memory(part + 0xa,
                           gpu::texture_page(kind & 1U, page_rate,
                                             static_cast<std::int32_t>(page_x),
                                             static_cast<std::int32_t>(page_y)),
                           2);
            } else {
                const auto page_y = memory(binding + 6, 2);
                set_memory(part + 0xa,
                           gpu::texture_page(kind & 1U, page_rate,
                                             s16(memory(binding + 4, 2)) +
                                                 static_cast<std::int32_t>((kind << 5U) & 0x1c0U),
                                             static_cast<std::int32_t>(page_y)),
                           2);
            }
            set_memory(
                part + 0xc,
                clut(s16(memory(binding + 8, 2)) + static_cast<std::int32_t>((control & 0xfU) * 16),
                     static_cast<std::int32_t>(memory(binding + 0xa, 2))),
                2);
        }
        set_memory(part + 0x14, (memory(part + 0x14) & ~7U) | group);
        set_memory(part + 4, u_base + memory(cell + 1, 1), 1);
        set_memory(part + 5, v_base + memory(cell + 2, 1), 1);
        set_memory(part + 6, memory(cell + 3, 1), 1);
        set_memory(part + 7, memory(cell + 4, 1), 1);
        set_memory(part + 0x14, (memory(part + 0x14) & ~0x10U) | ((control >> 2U) & 0x10U));
        stream = sprite_part_offsets(part, stream, (head & 0x80U) != 0);
    }
    set_memory(sprite + 0x40, (memory(sprite + 0x40) & 0xffffff03U) | (built & 0x3fU) << 2U);
}

// Resident 8001d468: build each sprite frame scheduled since the last pass,
// then empty the list (80059190).
void Program::sprite_frames() {
    auto &environment = resident.sprite;
    for (auto sprite = environment.frame_head; sprite != 0;) {
        if (const auto frame = s16(memory(sprite + 0x34, 2)); frame == 0)
            set_memory(sprite + 0x40, memory(sprite + 0x40) & 0xffffff03U);
        else
            build_sprite_frame(sprite, u32(frame));
        sprite = memory(memory(sprite + 0x20) + 0x38);
    }
    environment.frame_head = 0;
}

// Resident 80022038: refresh the sprite matrix (80022090) when flagged
// (+3c bit 28).
void Program::refresh_sprite_matrix(std::uint32_t sprite) {
    if ((memory(sprite + 0x3c) & 0x10000000U) == 0)
        return;
    with_sprite_sources([&](const field::SpriteSources &sources) {
        field::update_sprite_matrix({sprite, record_block(sprite)}, sources);
    });
    set_memory(sprite + 0x3c, memory(sprite + 0x3c) & ~0x10000000U);
}

// Resident 80025718, the draw callback of model-bearing task sprites: place
// the model at the sprite position and draw it into the sprite table.
void Program::draw_task_model(std::uint32_t node) {
    auto &gte = resident.gte;
    const auto sprite = memory(node + 4);
    refresh_sprite_matrix(sprite);
    const auto renderer = memory(sprite + 0x20);
    const auto model = memory(renderer + 0x34);
    if (model == 0)
        return;
    // TransMatrix (80049d9c): the sprite position becomes the translation.
    for (std::uint32_t i = 0; i < 3; ++i)
        set_memory(renderer + 0xc + 0x14 + i * 4, u32(s16(memory(sprite + 2 + i * 4, 2))));
    if ((memory(sprite + 0x3f, 1) & 1U) != 0)
        throw MissingDependency({"draw_task_model", 0x800257a0, {}, {}},
                                "symbol:task-model-unset-matrix", false,
                                "Task models flagged +3f loads an unset stack matrix");
    gte.transform.r = resident.sprite_view.r;
    const auto placed = field::compose_matrix(resident.sprite_view, memory_matrix(renderer + 0xc));
    gte.transform = placed;
    draw_model(model, memory(renderer + 0x2c + resident.sprite_buffer * 4), resident.sprite_table,
               static_cast<std::int32_t>(memory(sprite + 0x42, 2) & 4U));
}

// Resident 8001c9f8: the pending task list's callbacks.
void Program::sprite_pending_tasks() {
    auto &tasks = resident.sprite_tasks;
    tasks.next = tasks.pending_head;
    while (tasks.next != 0) {
        const auto node = tasks.next;
        tasks.current = node;
        tasks.next = memory(node + 0x18);
        const auto callback = memory(node + 8);
        if (callback == 0)
            continue;
        if (callback != 0x80025718)
            throw MissingDependency({"sprite_pending_tasks", callback, {}, {}},
                                    "symbol:sprite-pending-task-callback", false,
                                    "This pending sprite task callback is not recovered");
        draw_task_model(node);
    }
}

// Resident 8001f6b0: a one-sided sprite's parts take its color (+28) and
// blend rate (+3c bits 5-7, less one).
void Program::sprite_recolor_parts(std::uint32_t sprite) {
    const auto flags = memory(sprite + 0x3c);
    if ((flags & 3U) != 1)
        return;
    auto rate = (flags >> 5U) & 7U;
    if (rate != 0)
        --rate;
    const auto color = memory(sprite + 0x28);
    auto part = memory(memory(sprite + 0x20) + 0x30);
    for (std::uint32_t i = 0; i != memory(sprite + 0x40, 1) >> 2U; ++i, part += 0x18) {
        set_memory(part + 0x10, color);
        set_memory(part + 0xa, (memory(part + 0xa, 2) & 0xff9fU) | rate << 5U, 2);
    }
}

// Resident 80021b98.
void Program::sprite_color(std::uint32_t sprite, std::uint32_t red, std::uint32_t green,
                           std::uint32_t blue) {
    set_memory(sprite + 0x28, red, 1);
    set_memory(sprite + 0x29, green, 1);
    set_memory(sprite + 0x2a, blue, 1);
    set_memory(sprite + 0x2b, memory(sprite + 0x2b, 1) & 0xfeU, 1);
    sprite_recolor_parts(sprite);
}

// RotTransPers (8004a64c) with the loaded rotation and translation: the
// ordering-table depth SZ3 / 4.
std::int32_t Program::rot_trans_pers(const field::GteVector &vector) {
    auto &gte = resident.gte;
    gte.set_vector(0, vector);
    gte.rtps();
    return static_cast<std::int32_t>(gte.sz(3)) >> 2;
}

// Resident 8001e148: the sprite's place in view. Its position goes through
// the sprite camera (8004fbb8, ApplyMatrix 80049cec) and its frame origin
// (renderer +3c/+3d, scaled) offsets it; the renderer matrix (+c) becomes
// the loaded rotation and translation.
void Program::place_sprite(std::uint32_t sprite) {
    auto &gte = resident.gte;
    if (resident.sprite.platform_mode != 0 || resident.sprite_platform_b != 0)
        refresh_sprite_matrix(sprite);
    const auto shift = (memory(sprite + 0x40) >> 8U) & 0x1fU;
    const auto renderer = memory(sprite + 0x20);
    auto y = s8(memory(renderer + 0x3d, 1)) << shift;
    auto x = s8(memory(renderer + 0x3c, 1)) << shift;
    if (((memory(sprite + 0xac) >> 2U) & 1U) != 0)
        x = -x;
    const auto scale = s16(memory(sprite + 0x2c, 2));
    y = s32(u32(y) * u32(scale)) / 4096;
    x = s32(u32(x) * u32(scale)) / 4096;
    gte.transform.r = resident.sprite_view.r;
    gte.set_vector(0, {static_cast<std::int16_t>(s16(memory(sprite + 2, 2))),
                       static_cast<std::int16_t>(s16(memory(sprite + 6, 2))),
                       static_cast<std::int16_t>(s16(memory(sprite + 0xa, 2)))});
    gte.mvmva(0, 0, 3);
    const auto &t = resident.sprite_view.t;
    set_memory(renderer + 0x20, u32(t[0]) + u32(gte.mac(1)) + u32(x));
    set_memory(renderer + 0x24, u32(t[1]) + u32(gte.mac(2)) + u32(y));
    set_memory(renderer + 0x28, u32(t[2]) + u32(gte.mac(3)));
    gte.transform = memory_matrix(renderer + 0xc); // SetRotMatrix, SetTransMatrix
}

// Resident 8001e3d8: each part becomes a textured quad (POLY_FT4) in the
// sprite arena, linked at `slot` (or per group with +3c bit 27). Parts of a
// group share a matrix: the renderer's, or its product with the group's
// auxiliary rotation and offset. Groups masked by +3d (8004faf8) are skipped.
void Program::emit_sprite_parts(std::uint32_t sprite, std::uint32_t slot) {
    auto &r = resident;
    auto &gte = r.gte;
    const auto flags = [&] { return memory(sprite + 0x3c); };
    const auto shift = (memory(sprite + 0x40) >> 8U) & 0x1fU;
    const auto renderer = memory(sprite + 0x20);
    const auto origin_y = s8(memory(renderer + 0x3d, 1)) << shift;
    auto origin_x = s8(memory(renderer + 0x3c, 1)) << shift;
    if (((memory(sprite + 0xac) >> 2U) & 1U) != 0)
        origin_x = -origin_x;
    const auto count = (memory(sprite + 0x40) >> 2U) & 0x3fU;
    if (r.sprite_arena_cursor + count * 0x28 >= r.sprite_arena_end || count == 0)
        return;
    auto part = memory(renderer + 0x30);
    std::uint32_t group = 0xffffffffU;
    bool visible = false;
    for (std::uint32_t i = 0; i != (memory(sprite + 0x40, 1) >> 2U); ++i, part += 0x18) {
        if (const auto next = memory(part + 0x14) & 7U; next != group) {
            group = next;
            visible = (memory(0x8004faf8 + group * 2, 2) & memory(sprite + 0x3d, 1)) == 0;
            const auto records = memory(renderer + 0x34);
            const auto record = records + group * 8;
            if (records == 0 || (memory(record, 2) == 0 && memory(record + 6, 2) == 0)) {
                gte.transform = memory_matrix(renderer + 0xc);
            } else {
                auto x = s8(memory(record, 1)) << shift;
                const auto y = s8(memory(record + 1, 1)) << shift;
                const auto mirrored = ((flags() >> 3U) & 1U) != 0;
                if (mirrored)
                    x = -x;
                const auto scale = s16(memory(sprite + 0x2c, 2));
                auto roll = static_cast<std::int16_t>(memory(record + 6, 2));
                if (mirrored)
                    roll = static_cast<std::int16_t>(-roll);
                auto m =
                    field::rotation_matrix({static_cast<std::int16_t>(memory(record + 2, 2)),
                                            static_cast<std::int16_t>(memory(record + 4, 2)), roll},
                                           resident.math.trigonometry);
                m.t = {s32(memory(renderer + 0x20) + u32(s32(u32(x) * u32(scale)) / 4096)),
                       s32(memory(renderer + 0x24) + u32(s32(u32(y) * u32(scale)) / 4096)),
                       s32(memory(renderer + 0x28))};
                // SetMulMatrix (8004987c): the loaded rotation is the renderer's
                // times this one, column by column through MVMVA.
                const auto base = memory_matrix(renderer + 0xc);
                gte.transform.r = base.r;
                std::array<std::int16_t, 9> product{};
                for (std::size_t c = 0; c < 3; ++c) {
                    gte.set_vector(0, {m.r[c], m.r[3 + c], m.r[6 + c]});
                    gte.mvmva(0, 0, 3);
                    for (std::size_t row = 0; row < 3; ++row)
                        product[row * 3 + c] = gte.ir(row + 1);
                }
                gte.transform.r = product;
                gte.transform.t = m.t;
            }
        }
        if (!visible)
            continue;
        const auto packet = r.sprite_arena_cursor;
        r.sprite_arena_cursor = packet + 0x28;
        set_memory(packet + 3, 9, 1);
        set_memory(packet + 4, memory(part + 0x10));
        set_memory(packet + 0x16, memory(part + 0xa, 2), 2);
        set_memory(packet + 0xe, memory(part + 0xc, 2), 2);
        const auto width = s16(memory(part + 6, 1) + u32(s8(memory(part + 8, 1)))) << shift;
        const auto height = s16(memory(part + 7, 1) + u32(s8(memory(part + 9, 1)))) << shift;
        auto x = s16(memory(part, 2)) << shift;
        auto y = s16(memory(part + 2, 2)) << shift;
        auto w = width;
        auto h = height;
        if (((flags() >> 3U) & 1U) != 0) {
            w = -width;
            x = -x;
        }
        if (((flags() >> 4U) & 1U) != 0) {
            h = -height;
            y = -y;
        }
        auto &quad = r.sprite_quad;
        const auto half = [](std::int32_t value) { return static_cast<std::int16_t>(value); };
        const auto part_flags = memory(part + 0x14);
        const auto left = ((part_flags >> 4U) & 1U) == 0 ? x : x + w;
        const auto right = ((part_flags >> 4U) & 1U) == 0 ? x + w : x;
        const auto top = ((part_flags >> 5U) & 1U) == 0 ? y : y + h;
        const auto bottom = ((part_flags >> 5U) & 1U) == 0 ? y + h : y;
        quad[0][0] = half(left - origin_x);
        quad[1][0] = half(right - origin_x);
        quad[2][0] = half(right - origin_x);
        quad[3][0] = half(left - origin_x);
        quad[0][1] = half(top - origin_y);
        quad[1][1] = half(top - origin_y);
        quad[2][1] = half(bottom - origin_y);
        quad[3][1] = half(bottom - origin_y);
        // RotTransPers4 (8004a73c): corners 2 and 3 fill the packet's third
        // and fourth vertices crosswise. The depth cue and flag go to this
        // function's stack locals, which nothing reads.
        static constexpr std::array<std::uint32_t, 4> vertex{8, 0x10, 0x20, 0x18};
        static_cast<void>(resident::rot_trans_pers4(
            gte,
            [&](std::uint32_t i) { return field::GteVector{quad[i][0], quad[i][1], quad[i][2]}; },
            [&](std::uint32_t i, std::uint32_t value) {
                if (i < vertex.size())
                    set_memory(packet + vertex[i], value);
            }));
        // Texture corners; a mirrored quad samples one texel to the left.
        auto u = memory(part + 4, 1);
        const auto v = memory(part + 5, 1);
        auto u_span = memory(part + 6, 1) - 1;
        const auto v_span = memory(part + 7, 1) - 1;
        if (s16(memory(packet + 0x20, 2)) < s16(memory(packet + 8, 2))) {
            if (u == 0)
                u_span = memory(part + 6, 1) - 2;
            else
                --u;
        }
        set_memory(packet + 0xc, u, 1);
        set_memory(packet + 0xd, v, 1);
        set_memory(packet + 0x14, u + u_span, 1);
        set_memory(packet + 0x15, v, 1);
        set_memory(packet + 0x1c, u, 1);
        set_memory(packet + 0x1d, v + v_span, 1);
        set_memory(packet + 0x24, u + u_span, 1);
        set_memory(packet + 0x25, v + v_span, 1);
        add_primitive(((flags() >> 27U) & 1U) != 0 ? slot - group * 4 : slot, packet);
    }
}

// Resident 8001e298.
void Program::sprite_billboard(std::uint32_t sprite, std::uint32_t slot) {
    place_sprite(sprite);
    emit_sprite_parts(sprite, slot);
    if (((memory(sprite + 0x3c) >> 2U) & 1U) != 0)
        throw MissingDependency({"sprite_billboard", 0x8001e9bc, {}, {}},
                                "symbol:sprite-billboard-8001e9bc", false,
                                "The second billboard pass of +3c bit 2 is not recovered");
}

// Field 80075b44: each drawn actor (descriptor flag 0x40) keeps its previous
// placement, is projected to set its off-screen flag (+4 bit 9), and its
// sprite is scaled, fogged and drawn at its depth in `table`.
void Program::frame_billboards(std::uint32_t table) {
    auto &state = *field;
    auto &gte = resident.gte;
    const auto elevation = s16(state.camera.elevation);
    const auto raised = static_cast<std::int16_t>(-(elevation / 3 * 2));
    // 8009a514: the camera's octant.
    const auto octant =
        (7U - u32((static_cast<std::int32_t>(state.control_inputs.camera_angle) - 0x100) >> 9)) &
        7U;
    const auto missing = [](std::uint32_t address, const char *id, const char *reason) {
        throw MissingDependency({"frame_billboards", address, {}, {}}, id, false, reason);
    };
    for (const auto &owned : state.actors) {
        const auto descriptor = owned.descriptor_address;
        const auto kind = memory(descriptor + 0x58, 2);
        if ((kind & 0x40U) == 0)
            continue;
        const auto actor = memory(descriptor + 0x4c);
        const auto sprite = memory(descriptor + 4);
        for (std::uint32_t i = 0; i < 8; ++i)
            set_memory(descriptor + 0x2c + i * 4, memory(descriptor + 0xc + i * 4));
        if ((memory(actor + 4) & 0x2000U) != 0) {
            if (resident.w_4f380 == 0)
                missing(0x80076300, "symbol:field-party-model-placement",
                        "Placing party models (801e8670) is not recovered");
            continue;
        }
        // The actor's matrix in view: 800afa64 times its rotation, and its
        // low-halfword position through 800afa64.
        const auto &world = state.camera.scaled_world;
        gte.transform.r = world.r;
        field::GteMatrix placed{};
        for (std::uint32_t c = 0; c < 3; ++c) {
            gte.set_ir({static_cast<std::int16_t>(memory(descriptor + 0xc + c * 2, 2)),
                        static_cast<std::int16_t>(memory(descriptor + 0x12 + c * 2, 2)),
                        static_cast<std::int16_t>(memory(descriptor + 0x18 + c * 2, 2))});
            gte.mvmva(0, 3, 3);
            for (std::uint32_t row = 0; row < 3; ++row)
                placed.r[row * 3 + c] = gte.ir(row + 1);
        }
        gte.transform.t = world.t;
        gte.set_vector(0, {static_cast<std::int16_t>(memory(descriptor + 0x20, 2)),
                           static_cast<std::int16_t>(memory(descriptor + 0x24, 2)),
                           static_cast<std::int16_t>(memory(descriptor + 0x28, 2))});
        gte.mvmva(0, 0, 0);
        placed.t = {gte.mac(1), gte.mac(2), gte.mac(3)};
        gte.transform = placed;
        gte.set_vector(0, {0, raised, 0});
        gte.rtps();
        const auto screen = gte.sxy(2);
        const auto flag = gte.flag();
        auto depth = static_cast<std::int32_t>(gte.sz(3)) >> 2;
        const auto on_screen =
            u32((s32(screen) >> 16) + 9) < 0x143 && u32(s16(screen) + 0x27) < 399;
        set_memory(actor + 4, on_screen ? memory(actor + 4) & ~0x200U : memory(actor + 4) | 0x200U);
        if (resident.w_4f37c != 0 || (kind & 0x20U) != 0 || s32(flag) < 0)
            continue;
        // Sprite scale: three quarters of the actor's, five quarters more
        // for kind 7 while 800b2268 is set.
        field::GteLong scale{};
        for (std::uint32_t i = 0; i < 3; ++i)
            scale[i] = s16(memory(actor + 0xf4 + i * 2, 2)) * 3 >> 2;
        if (s16(memory(actor + 0xe4, 2)) == 7 && state.party_reassignment != 0)
            for (auto &value : scale)
                value = value * 5 >> 2;
        // The renderer matrix takes the orbit rotation (8007409c copies nine
        // halfwords; the translation words come from uninitialised stack and
        // are replaced by 8001e148 before any use), then ScaleMatrix.
        const auto renderer = memory(sprite + 0x20);
        auto oriented = state.camera.orbit;
        field::scale_matrix(oriented, scale);
        for (std::uint32_t i = 0; i < 9; ++i)
            set_memory(renderer + 0xc + i * 2, static_cast<std::uint16_t>(oriented.r[i]), 2);
        set_memory(renderer + 0x1e, static_cast<std::uint16_t>(oriented.pad), 2);
        // Facing sprites (+14 bit 21) seen from behind sort half a unit nearer
        // or farther.
        if (const auto facing = memory(actor + 0x14); (facing & 0x200000U) != 0) {
            const auto side = (octant - (((facing >> 11U) - 2) & 7U)) & 7U;
            if (side != 0 && side != 4)
                depth = rot_trans_pers({0, static_cast<std::int16_t>(side < 4 ? -0x80 : 0x80), 0});
        }
        if (state.b_b2357 == 0 && state.sprite_gate != 0) {
            // Depth cue (DPCS) of the fog color.
            // The code byte of RGBC does not reach the stored color bytes.
            const auto &fog = resident.sprite_models.fog_color;
            gte.set_data(6, u32(fog[0]) | u32(fog[1]) << 8U | u32(fog[2]) << 16U);
            gte.execute(0x10U | 1U << 19U);
            const auto color = gte.rgb(2);
            sprite_color(sprite, color & 0xffU, (color >> 8U) & 0xffU, (color >> 16U) & 0xffU);
        }
        const auto depth_shift = resident.sprite_models.depth_shift & 0x1fU;
        depth >>= depth_shift;
        if (depth >= 2)
            depth -= 2;
        const auto hidden = (memory(actor + 4) & 0x2000000U) != 0;
        if (hidden)
            missing(0x80076140, "symbol:billboard-stack-translation",
                    "A hidden actor keeps uninitialised stack words as its renderer translation");
        const auto layered = ((memory(actor + 0xe8, 2) + 0x22U) & 0xffffU) < 2;
        if (layered) {
            sprite_color(sprite, memory(actor + 0xfc, 1), memory(actor + 0xfd, 1),
                         memory(actor + 0xfe, 1));
            set_memory(sprite + 0x3d, 0xef, 1);
            sprite_billboard(sprite, table + u32(depth) * 4 - 0x40);
            const auto upper = rot_trans_pers({0, 300, 0}) >> depth_shift;
            sprite_color(sprite, memory(actor + 0xff, 1), memory(actor + 0x100, 1),
                         memory(actor + 0x101, 1));
            set_memory(sprite + 0x3d, 0xf7, 1);
            sprite_billboard(sprite, table + u32(upper) * 4);
            continue;
        }
        set_memory(sprite + 0x3d, 0, 1);
        if ((memory(actor + 0x134) & 0x60U) != 0)
            missing(0x80076234, "symbol:billboard-split-sprites",
                    "Split billboards (8001e2f8/8001e368) are not recovered");
        if (state.sprite_gate == 0) // 80075b08
            sprite_color(sprite, memory(actor + 0xfc, 1), memory(actor + 0xfd, 1),
                         memory(actor + 0xfe, 1));
        sprite_billboard(sprite, table + u32(depth) * 4);
    }
}

// VectorNormal (80048d7c): the vector scaled to length 4096 through the
// GTE (SQR, the resident reciprocal square-root table 80056b94, GPF).
field::GteLong Program::vector_normal(const field::GteLong &vector) {
    auto &gte = resident.gte;
    const field::GteVector ir{static_cast<std::int16_t>(vector[0]),
                              static_cast<std::int16_t>(vector[1]),
                              static_cast<std::int16_t>(vector[2])};
    gte.set_ir(ir);
    gte.execute(0x28U | 1U << 10U); // SQR sf=0 lm=1
    const auto sum = u32(gte.mac(1)) + u32(gte.mac(2)) + u32(gte.mac(3));
    gte.set_data(30, sum);
    const auto zeros = gte.data(31) & ~1U;
    const auto shift = static_cast<std::int32_t>(31U - zeros) >> 1;
    const auto mantissa = zeros >= 24 ? s32(sum << (zeros - 24)) : s32(sum) >> (24 - zeros);
    const auto index = u32(mantissa - 0x40) * 2;
    gte.set_data(8, u32(resident.math.reciprocal.at(index / 2)));
    gte.set_ir(ir);
    gte.execute(0x3dU); // GPF sf=0
    return {gte.mac(1) >> shift, gte.mac(2) >> shift, gte.mac(3) >> shift};
}

// Field 800764b4: a ground shadow quad (descriptor +8: four corners, then a
// packet per buffer) for each drawn actor, oriented to the ground normal
// (actor +50) and placed at the actor, sorted by its average depth.
void Program::frame_shadows(std::uint32_t table, std::uint32_t buffer) {
    auto &state = *field;
    auto &gte = resident.gte;
    if (resident.w_4f37c != 0)
        return;
    const auto &world = state.camera.scaled_world;
    for (const auto &owned : state.actors) {
        const auto descriptor = owned.descriptor_address;
        if ((memory(descriptor + 0x58) & 0x60U) != 0x40)
            continue;
        const auto actor = memory(descriptor + 0x4c);
        if ((memory(actor + 4) & 0x102200U) != 0 || (memory(actor + 4) & 0x800U) != 0 ||
            (memory(actor) & 0x10000U) != 0 || (memory(actor + 0x14) & 0x200002U) != 0)
            continue;
        // Two outer products (OP: diagonal from the loaded rotation words
        // 0, 2 and 4) with the normal give the ground's other axes.
        const auto normal_word = [&](std::uint32_t i) { return memory(actor + 0x50 + i * 4); };
        const auto normal = field::GteVector{static_cast<std::int16_t>(normal_word(0)),
                                             static_cast<std::int16_t>(normal_word(1)),
                                             static_cast<std::int16_t>(normal_word(2))};
        const auto cross = [&](const field::GteLong &axis) {
            gte.set_control(0, u32(axis[0]));
            gte.set_control(2, u32(axis[1]));
            gte.set_control(4, u32(axis[2]));
            gte.set_ir(normal);
            gte.execute(0x0cU | 1U << 19U); // OP sf=1
            return vector_normal({gte.mac(1), gte.mac(2), gte.mac(3)});
        };
        const auto side = cross({0, 0, 0x1000});
        const auto front = cross(side);
        field::GteMatrix ground{};
        for (std::size_t i = 0; i < 3; ++i) {
            ground.r[i] = static_cast<std::int16_t>(side[i]);
            ground.r[3 + i] = normal[i];
            ground.r[6 + i] = static_cast<std::int16_t>(front[i]);
        }
        const auto sprite = memory(descriptor + 4);
        // Into view through 800afa64 (column products by MVMVA), placed at the
        // descriptor's x/z and the sprite's ground height (+84).
        field::GteMatrix placed{};
        gte.transform.r = world.r;
        for (std::size_t c = 0; c < 3; ++c) {
            gte.set_ir({ground.r[c], ground.r[3 + c], ground.r[6 + c]});
            gte.mvmva(0, 3, 3);
            for (std::size_t row = 0; row < 3; ++row)
                placed.r[row * 3 + c] = gte.ir(row + 1);
        }
        gte.transform.t = world.t;
        gte.set_vector(0, {static_cast<std::int16_t>(memory(descriptor + 0x20, 2)),
                           static_cast<std::int16_t>(memory(sprite + 0x84, 2)),
                           static_cast<std::int16_t>(memory(descriptor + 0x28, 2))});
        gte.mvmva(0, 0, 0);
        placed.t = {gte.mac(1), gte.mac(2), gte.mac(3)};
        const auto small = state.party_reassignment != 0 && (memory(actor) & 0x400U) != 0;
        field::GteLong scale{};
        for (std::uint32_t i = 0; i < 3; ++i)
            scale[i] = s16(memory(actor + 0xf4 + i * 2, 2)) * 0xc00 >> (small ? 14 : 12);
        field::scale_matrix(placed, scale);
        gte.transform = placed;
        const auto record = memory(descriptor + 8);
        const auto packet = record + 0x20 + buffer * 0x28;
        const auto depth = rot_average4(record, packet);
        add_primitive(table + (depth >> (resident.sprite_models.depth_shift & 0x1fU)) * 4, packet);
    }
}

// Field 800752c8.
void Program::frame_characters(FrameServices &services, const ProgramObserver &observe) {
    static_cast<void>(services);
    auto &state = *field;
    if (state.orientation_hold == 1)
        return;
    observed(observe, *this, "characters_begin", 0x800752c8);
    sprite_buffer_begin(state.draw_buffer);
    observed(observe, *this, "characters_arena", 0x800250e0);
    resident.sprite_table = state.draw_block + 0xcc; // 80024fe4
    observed(observe, *this, "characters_table", 0x80024fe4);
    resident.sprite_view = state.camera.scaled_world; // 80024ff4
    observed(observe, *this, "characters_view", 0x80024ff4);
    sprite_frames();
    observed(observe, *this, "characters_frames", 0x8001d468);
    sprite_pending_tasks();
    observed(observe, *this, "characters_pending", 0x8001c9f8);
    with_sprite_sources([&](const field::SpriteSources &sources) {
        field::advance_sprite_tasks(resident.sprite_tasks, resident.sprite, sources);
    });
    observed(observe, *this, "characters_tasks", 0x8001c964);
    frame_billboards(state.draw_block + 0xcc);
    observed(observe, *this, "characters_billboards", 0x80075b44);
    // Advance the sprite timers (80023210) of drawn actors not held.
    for (std::uint32_t index = 0; index < state.actors.size(); ++index) {
        const auto descriptor = state.actors[index].descriptor_address;
        const auto actor = memory(descriptor + 0x4c);
        const auto flags = memory(actor + 4);
        bool advance = false;
        if ((memory(descriptor + 0x58) & 0x60U) == 0x40)
            advance =
                (flags & 0x600U) != 0x200 && (flags & 0x1000U) == 0 && (memory(actor) & 1U) == 0;
        else
            advance = (flags & 0x1000000U) != 0;
        if (!advance)
            continue;
        const auto sprite = memory(descriptor + 4);
        with_sprite_sources(
            [&](const field::SpriteSources &sources) {
                static_cast<void>(field::advance_sprite_timer({sprite, record_block(sprite)},
                                                              resident.sprite, sources));
            },
            index);
    }
    observed(observe, *this, "characters_timers", 0x80023210);
    frame_shadows(state.draw_block + 0xcc, state.draw_buffer);
    if (state.event_control.diagnostic_suppression == 0)
        throw MissingDependency({"frame_characters", 0x80081b00, {}, {}}, "symbol:field-80081b00",
                                false, "The unsuppressed call of 80081b00 is not recovered");
}

} // namespace xem::reconstruction
