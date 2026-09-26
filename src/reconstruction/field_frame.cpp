// Field overlay 8007554c: one field frame. The move phase, then drawing into
// the current draw buffer block, buffer presentation and the frame-rate wait.
// Original addresses name correlations only; owned records are Program state.
#include "xem/reconstruction/field_collision.hpp"
#include "xem/reconstruction/field_view.hpp"
#include "xem/reconstruction/original_layout.hpp"
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <bit>

namespace xem::reconstruction {
namespace {
std::uint32_t word(std::span<const std::uint8_t> data, std::size_t offset, std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Frame read exceeds owned storage");
    std::uint32_t result = 0;
    for (std::size_t i = 0; i < width; ++i)
        result |= static_cast<std::uint32_t>(data[offset + i]) << (8U * i);
    return result;
}
void put(std::span<std::uint8_t> data, std::size_t offset, std::uint32_t value,
         std::size_t width = 4) {
    if (offset > data.size() || width > data.size() - offset)
        throw field::FieldFormatError("Frame write exceeds owned storage");
    for (std::size_t i = 0; i < width; ++i)
        data[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::int32_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::int32_t s32(std::uint32_t value) { return std::bit_cast<std::int32_t>(value); }
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
// The high halfword of a 16.16 word, as LH of its upper half reads it.
std::int32_t high(std::int32_t value) { return s16(u32(value) >> 16U); }
void observed(const ProgramObserver &observe, const Program &program, SourcePoint point,
              bool completed) {
    if (observe)
        observe(program, point, completed);
}
std::int32_t subtract(std::int32_t a, std::int32_t b) { return s32(u32(a) - u32(b)); }
// Field 80070594: the identity rotation from 8003f738 with a zero translation.
field::GteMatrix identity(std::span<const std::uint8_t> trigonometry) {
    auto m = field::rotation_matrix({0, 0, 0}, trigonometry);
    m.t = {};
    return m;
}
// MulMatrix2 (80049bdc) and CompMatrix (8004931c) load their left matrix as
// the GTE rotation.
void multiply_rotation(Gte &gte, const field::GteMatrix &left, field::GteMatrix &right) {
    gte.transform.r = left.r;
    field::multiply_rotation(left, right);
}
field::GteMatrix compose(Gte &gte, const field::GteMatrix &left, const field::GteMatrix &right) {
    gte.transform.r = left.r;
    return field::compose_matrix(left, right);
}
FieldState &loaded(Program &program) {
    if (!program.field)
        throw field::FieldFormatError("A field frame requires loaded field state");
    return *program.field;
}
} // namespace

std::uint32_t take_service(std::deque<std::uint32_t> &results, const char *what) {
    if (results.empty())
        throw ServiceUnavailable(what);
    const auto value = results.front();
    results.pop_front();
    return value;
}

// Resources (qualified source extents, such as the field geometry) are owned
// bytes the drawing code reads through their original address.
std::span<std::uint8_t> Program::resource_bytes(std::uint32_t address, std::size_t width) const {
    if (field)
        for (auto &resource : field->resources)
            if (address >= resource.address &&
                address - resource.address <= resource.bytes.size() &&
                width <= resource.bytes.size() - (address - resource.address))
                return std::span(const_cast<std::vector<std::uint8_t> &>(resource.bytes))
                    .subspan(address - resource.address, width);
    return {};
}

// RAM through its KUSEG, KSEG0 and KSEG1 mirrors: drawing code keeps packet
// cursors as 24-bit link addresses and stores through them.
std::uint32_t ram_address(std::uint32_t address) {
    if ((address & 0x1fffffffU) >= 0x200000U)
        throw field::FieldFormatError("Address outside main RAM");
    return 0x80000000U | (address & 0x1fffffU);
}

std::span<std::uint8_t> Program::record_block(std::uint32_t address) const {
    const auto from = [&](std::uint32_t base, const std::vector<std::uint8_t> &bytes) {
        auto &owned = const_cast<std::vector<std::uint8_t> &>(bytes);
        return address >= base && address - base < owned.size()
                   ? std::span(owned).subspan(address - base)
                   : std::span<std::uint8_t>{};
    };
    const auto array = [&](std::uint32_t base, const auto &bytes) {
        auto &owned = const_cast<std::remove_cvref_t<decltype(bytes)> &>(bytes);
        return address >= base && address - base < owned.size()
                   ? std::span<std::uint8_t>(owned).subspan(address - base)
                   : std::span<std::uint8_t>{};
    };
    // The loaded message table (component 7) that dialogue text reads.
    if (field)
        if (const auto bytes = from(field->messages_address, field->messages); !bytes.empty())
            return bytes;
    if (field)
        for (const auto &blocks : field->dialogue_blocks)
            for (const auto &block : blocks)
                if (const auto bytes = from(block.address, block.bytes); !bytes.empty())
                    return bytes;
    if (field)
        for (const auto &actor : field->actors) {
            if (const auto bytes = from(actor.sprite.sprite.address, actor.sprite.sprite.bytes);
                !bytes.empty())
                return bytes;
            if (const auto bytes = array(actor.address, actor.storage); !bytes.empty())
                return bytes;
            if (const auto bytes = array(actor.descriptor_address, actor.descriptor);
                !bytes.empty())
                return bytes;
            if (const auto bytes = from(actor.sprite.parts.address, actor.sprite.parts.bytes);
                !bytes.empty())
                return bytes;
        }
    // Descriptors of the map pieces after the event actors'.
    if (field)
        for (const auto &piece : field->pieces)
            if (const auto bytes = array(piece.address, piece.descriptor); !bytes.empty())
                return bytes;
    for (const auto &node : resident.sprite_tasks.nodes)
        if (const auto bytes = from(node.address, node.bytes); !bytes.empty())
            return bytes;
    // Sprite models' packet buffers (8002cb54).
    for (const auto &buffer : resident.sprite_models.buffers)
        if (const auto bytes = from(buffer.address, buffer.bytes); !bytes.empty())
            return bytes;
    // The movie mode's overlay and its player's stack frame.
    if (movie_mode_memory)
        for (const auto *block : {&movie_mode_memory->overlay, &movie_mode_memory->frame})
            if (const auto bytes = from(block->address, block->bytes); !bytes.empty())
                return bytes;
    // Music blocks, the cached mode block and the read-ahead block.
    for (const auto &block : resident.music_blocks)
        if (const auto bytes = from(block.address, block.bytes); !bytes.empty())
            return bytes;
    for (const auto *block : {&resident.mode_block, &resident.preload_block})
        if (const auto bytes = from(block->address, block->bytes); !bytes.empty())
            return bytes;
    // Disc reads: the ring, its payload, the read list, DMA transfers and the
    // file and directory tables.
    const auto &read = resident.disc_read;
    for (const auto *block : {&read.ring, &read.ring_payload, &read.list})
        if (const auto bytes = from(block->address, block->bytes); !bytes.empty())
            return bytes;
    for (const auto &block : resident.disc_transfers)
        if (const auto bytes = from(block.address, block.bytes); !bytes.empty())
            return bytes;
    if (const auto bytes = from(read.file_table, read.files); !bytes.empty())
        return bytes;
    if (const auto bytes = from(read.directory_table, read.directories); !bytes.empty())
        return bytes;
    // Persistent game data (*8005a39c) and the resident field snapshot.
    if (const auto bytes = from(resident.game_state, resident.game_data); !bytes.empty())
        return bytes;
    if (const auto bytes = from(0x8005a4e4, resident.field_snapshot); !bytes.empty())
        return bytes;
    // The SPU memory allocation table.
    if (const auto bytes = array(resident::spu_block_table, resident.sound.spu_blocks);
        !bytes.empty())
        return bytes;
    // Sound driver objects (80065b0c pool) and the pool's held bytes.
    for (const auto *pool : {&resident.sound.objects, &resident.sound.held})
        if (const auto after = pool->upper_bound(address); after != pool->begin())
            if (const auto bytes = from(std::prev(after)->first, std::prev(after)->second);
                !bytes.empty())
                return bytes;
    // Allocated heap bytes no other value interprets.
    if (const auto after = resident.heap_contents.upper_bound(address);
        after != resident.heap_contents.begin())
        if (const auto bytes = from(std::prev(after)->first, std::prev(after)->second);
            !bytes.empty())
            return bytes;
    return {};
}

std::span<std::uint8_t> Program::record_bytes(std::uint32_t address, std::size_t width) const {
    const auto bytes = record_block(address);
    return bytes.size() >= width ? bytes.first(width) : std::span<std::uint8_t>{};
}

std::uint32_t Program::memory(std::uint32_t address, std::size_t width) const {
    address = ram_address(address);
    if (field && field->regions.contains(address, width))
        return field->regions.word(address, width);
    if (battle && battle->contains(address, static_cast<std::uint32_t>(width)))
        return width == 1   ? battle->u8(address)
               : width == 2 ? battle->u16(address)
                            : battle->u32(address);
    if (const auto bytes = resource_bytes(address, width); !bytes.empty())
        return word(bytes, 0, width);
    if (const auto bytes = record_bytes(address, width); !bytes.empty())
        return word(bytes, 0, width);
    std::array<std::uint8_t, 4> bytes{};
    read_original(*this, address, std::span(bytes).first(width));
    return word(bytes, 0, width);
}

void Program::set_memory(std::uint32_t address, std::uint32_t value, std::size_t width) {
    address = ram_address(address);
    if (field && field->regions.contains(address, width)) {
        field->regions.put(address, value, width);
        return;
    }
    if (battle && battle->contains(address, static_cast<std::uint32_t>(width))) {
        if (width == 1)
            battle->put8(address, value);
        else if (width == 2)
            battle->put16(address, value);
        else
            battle->put32(address, value);
        return;
    }
    if (const auto bytes = resource_bytes(address, width); !bytes.empty()) {
        put(bytes, 0, value, width);
        return;
    }
    if (const auto bytes = record_bytes(address, width); !bytes.empty()) {
        put(bytes, 0, value, width);
        return;
    }
    std::array<std::uint8_t, 4> bytes{};
    put(bytes, 0, value, width);
    write_original(*this, address, std::span(bytes).first(width));
}

// RotAverage4 (8004a7bc): the four SVECTOR corners at `record` projected
// into the packet's vertices (RTPT on three, RTPS on the fourth); returns
// the average depth OTZ.
std::uint32_t Program::rot_average4(std::uint32_t record, std::uint32_t packet) {
    auto &gte = resident.gte;
    const auto vector = [&](std::uint32_t at) {
        return field::GteVector{static_cast<std::int16_t>(s16(memory(at, 2))),
                                static_cast<std::int16_t>(s16(memory(at + 2, 2))),
                                static_cast<std::int16_t>(s16(memory(at + 4, 2)))};
    };
    for (std::uint32_t i = 0; i < 3; ++i)
        gte.set_vector(i, vector(record + i * 8));
    gte.rtpt();
    for (std::uint32_t i = 0; i < 3; ++i)
        set_memory(packet + 8 + i * 8, gte.sxy(i));
    gte.set_vector(0, vector(record + 0x18));
    gte.rtps();
    set_memory(packet + 0x20, gte.sxy(2));
    gte.avsz4();
    return gte.otz();
}

// AddPrims (80043b84): link the packet list p0..p1 at the head of `table`.
void Program::add_primitives(std::uint32_t table, std::uint32_t first, std::uint32_t last) {
    set_memory(last, (memory(last) & 0xff000000U) | (memory(table) & 0xffffffU));
    set_memory(table, (memory(table) & 0xff000000U) | (first & 0xffffffU));
}

// PsyQ addPrim: link the packet at the head of the ordering-table entry.
void Program::add_primitive(std::uint32_t table_entry, std::uint32_t packet) {
    set_memory(packet, (memory(packet) & 0xff000000U) | (memory(table_entry) & 0xffffffU));
    set_memory(table_entry, (memory(table_entry) & 0xff000000U) | (packet & 0xffffffU));
}

// Field 80086590: the three nearest actors carrying a sound emitter (+10d
// not ff) keep or start their effect; table entries no longer matched stop.
void Program::frame_emitters(std::uint32_t listener_source) {
    auto &state = *field;
    std::array<std::int32_t, 3> listener{};
    if (listener_source == 1) {
        for (std::size_t i = 0; i < 3; ++i)
            listener[i] = high(state.camera.eye[i]);
    } else if (listener_source == 2) {
        for (std::size_t i = 0; i < 3; ++i)
            listener[i] = high(state.camera.target[i]);
    } else {
        const auto &a = state.actors.at(static_cast<std::size_t>(state.controlled_actor)).storage;
        for (std::size_t i = 0; i < 3; ++i)
            listener[i] = s16(word(a, 0x22 + 4 * i, 2));
    }
    struct Slot {
        std::int32_t distance = 0xffff;
        std::int32_t id = -1;
        std::uint32_t kind = 0;
        std::uint32_t actor = 0;
        bool kept = false;
        std::array<std::int16_t, 3> offset{};
    };
    std::array<Slot, 3> slots{};
    std::array<bool, 3> matched{};
    for (std::size_t i = 0; i < state.actors.size(); ++i) {
        auto &a = state.actors[i].storage;
        if (a[0x10d] == 0xff) {
            a[0x10d] = 0xff;
            continue;
        }
        std::array<std::int32_t, 3> away{};
        for (std::size_t k = 0; k < 3; ++k)
            away[k] = listener[k] - s16(word(a, 0x22 + 4 * k, 2));
        const auto distance =
            field::script_distance(away[0], away[1], away[2], resident.math.square_root);
        // Replace the farthest of the three kept slots.
        const auto &d = slots;
        std::size_t far = 0;
        if (d[0].distance < d[1].distance)
            far = d[1].distance < d[2].distance ? 2 : 1;
        else
            far = d[0].distance < d[2].distance ? 2 : 0;
        if (distance >= slots[far].distance)
            continue;
        auto &slot = slots[far];
        slot.actor = static_cast<std::uint32_t>(i);
        slot.distance = distance;
        slot.id = static_cast<std::int32_t>(word(a, 0x10a, 2));
        slot.kind = a[0x10c];
        for (std::size_t k = 0; k < 3; ++k)
            slot.offset[k] = static_cast<std::int16_t>(away[k]);
    }
    // Field 80086470: the table entry already playing for this actor.
    for (auto &slot : slots) {
        if (slot.id == -1)
            continue;
        for (std::size_t k = 0; k < 3; ++k)
            if (state.emitters[k][0] == slot.actor) {
                matched[k] = true;
                slot.kept = true;
                break;
            }
    }
    for (std::size_t k = 0; k < 3; ++k) {
        if (matched[k] || state.emitters[k][1] == 0xffff)
            continue;
        resident::stop_effect_pair(resident.sound, static_cast<std::uint32_t>(k * 2));
        state.emitters[k][1] = 0xffff;
        state.emitters[k][0] = 0xffff;
    }
    for (const auto &slot : slots) {
        if (slot.id == -1)
            continue;
        throw MissingDependency(
            {"field_frame_emitters", slot.kept ? 0x800860f0U : 0x800862ccU, slot.actor, {}},
            slot.kept ? "symbol:field-emitter-update" : "symbol:field-emitter-start", false,
            "Positional sound emitter effects are not recovered");
    }
}

// Field 80071cb4: advance both fade channels in mode 2 (80071a8c), then
// 8007da44 links each active channel's draw mode and tile into the frame's
// small ordering table (block + 80d4; channel 1 one entry further).
void Program::frame_fade() {
    auto &state = *field;
    auto &fade = state.fade;
    if (fade.mode == 2) {
        for (auto &channel : fade.channels) {
            auto &steps = channel.halves[2];
            if (channel.halves[1] == 0)
                continue;
            if (static_cast<std::int16_t>(steps) < 1) {
                steps = 0;
                if (fade.started != 1 && channel.words[0] == 0 && channel.words[1] == 0 &&
                    channel.words[2] == 0)
                    channel.halves[1] = 0;
                continue;
            }
            for (std::size_t c = 0; c < 3; ++c) {
                auto level = s32(channel.words[c]) + s32(channel.words[3 + c]);
                if ((level >> 8) > 0xff)
                    level = 0xff00;
                if (level < 0)
                    level = 0;
                channel.words[c] = u32(level);
            }
            steps = static_cast<std::uint16_t>(steps - 1U);
        }
    }
    const auto buffer = state.draw_buffer;
    const auto table = state.draw_block + 0x80d4U;
    for (std::uint32_t c = 0; c < 2; ++c) {
        auto &channel = fade.channels[c];
        if (channel.halves[1] == 0)
            continue;
        // 800afe3c + 8c: the texture window rectangle, the whole screen.
        const std::array<std::int16_t, 4> window{0, 0, 0x140, 0xe0};
        state.fade_windows[c] = window;
        const auto tpage = gpu::texture_page(0, u32(s16(channel.halves[0])), 0, 0) & 0xffffU;
        // SetDrawMode (800454dc) into this buffer's DR_MODE packet.
        auto modes = std::span(channel.modes).subspan(buffer * 12, 12);
        modes[3] = 2;
        put(modes, 4, gpu::draw_mode(resident.gpu_type, false, false, tpage));
        put(modes, 8, gpu::texture_window(&window));
        auto tile = std::span(channel.packets).subspan(buffer * 16, 16);
        for (std::size_t k = 0; k < 3; ++k)
            tile[4 + k] = static_cast<std::uint8_t>(s32(channel.words[k]) >> 8);
        const auto base = field::FadeChannel::base + c * field::FadeChannel::stride;
        const auto entry = table + (c == 1 ? 4U : 0U);
        add_primitive(entry, base + 0x18 + buffer * 16);
        add_primitive(entry, base + buffer * 12);
        if ((s32(channel.words[0]) >> 8) == 0 && (s32(channel.words[1]) >> 8) == 0 &&
            (s32(channel.words[2]) >> 8) == 0) {
            channel.halves[1] = 0;
            channel.halves[2] = 0;
        }
    }
}

std::uint16_t Program::overlay_half(std::uint32_t address) const {
    return static_cast<std::uint16_t>(overlay_byte(address) | overlay_byte(address + 1) << 8U);
}

void Program::compass_quad(std::uint32_t table, std::uint32_t record, const field::GteMatrix &m,
                           bool label) {
    auto &gte = resident.gte;
    const auto packet = record + 0x20 + field->draw_buffer * 0x28;
    field::push_matrix(resident.matrix_stack, gte.transform);
    gte.transform = m; // SetRotMatrix and SetTransMatrix
    static_cast<void>(rot_average4(record, packet));
    if (label) {
        // 8007ac58: a 16-pixel-wide letter centred on the lower edge.
        const auto sum = s16(memory(packet + 0x20, 2)) + s16(memory(packet + 0x18, 2));
        const auto x = (sum + static_cast<std::int32_t>(u32(sum) >> 31U)) >> 1;
        const auto bottom = s16(memory(packet + 0x22, 2));
        set_memory(packet + 8, u32(x - 8), 2);
        set_memory(packet + 0x10, u32(x + 8), 2);
        set_memory(packet + 0x18, u32(x - 8), 2);
        set_memory(packet + 0x20, u32(x + 8), 2);
        set_memory(packet + 0x1a, u32(bottom), 2);
        set_memory(packet + 0x22, u32(bottom), 2);
        set_memory(packet + 0xa, u32(bottom - 10), 2);
        set_memory(packet + 0x12, u32(bottom - 10), 2);
    }
    add_primitive(table + 4, packet);
    gte.transform = field::pop_matrix(resident.matrix_stack);
}

// Field 80074108: the compass palette upload, its view matrices, the quads
// and letters, and the matrix at 800afa84 the model pass scales.
void Program::frame_compass(FrameServices &services) {
    auto &state = *field;
    auto &c = state.camera;
    auto &gte = resident.gte;
    const auto &trig = resident.math.trigonometry;
    // Eight rows of the sixteen compass colours; a row whose heading octant
    // is blocked (800af9f5) is black.
    for (std::uint32_t row = 0; row < 8; ++row) {
        const bool dark = (c.heading_blocks[1] & overlay_half(0x800adc24 + row * 2)) != 0;
        for (std::uint32_t i = 0; i < 16; ++i)
            state.compass_palette[row * 16 + i] = dark ? 0 : state.compass_colors[i];
    }
    state.compass_palette_rect[2] = 0x80;
    load_image_at(services, 0x800b004c, 0x800afd24);
    gte.screen.h = 0x80;                          // SetGeomScreen (8004a14c)
    gte.screen.offset_x = s32(u32(0x10a) << 16U); // SetGeomOffset (8004a12c)
    gte.screen.offset_y = s32(u32(0xa6) << 16U);
    // Look at the origin from the camera's height and planar distance.
    const auto distance = field::collision_planar_length(subtract(c.eye[0], c.target[0]) >> 16,
                                                         subtract(c.eye[2], c.target[2]) >> 16,
                                                         resident.math.square_root);
    const field::GteLong eye{0, subtract(c.eye[1], c.target[1]), s32(u32(-distance) << 16U)};
    field::GteMatrix look{};
    field::build_view(gte, resident.math.reciprocal, look, eye, {0, 0, 0}, c.up);
    auto base = identity(trig);
    base.t[2] = 0x80;
    gte.transform.r = base.r; // SetRotMatrix and SetTransMatrix
    gte.transform.t = base.t;
    // The needle turns toward the followed actor's heading in view.
    const auto &followed = state.actors.at(state.followed_actor).storage;
    state.compass_target = static_cast<std::int16_t>(
        word(followed, 0x106, 2) + static_cast<std::uint16_t>(c.view_angle) + 0x400U);
    state.compass_heading = static_cast<std::int16_t>(
        state.camera_cut != 0
            ? u32(state.compass_target) & 0xfffU
            : field::turn_toward(state.compass_heading, state.compass_target, 0x40));
    field::GteMatrix spin{};
    spin = field::rotation_matrix({0, state.compass_heading, 0}, trig, spin);
    multiply_rotation(gte, look, spin);
    spin.t[2] = 0x1000;
    auto placed = compose(gte, base, spin);
    const bool shown =
        state.script_flags_b21d0[1] == 0 && state.camera_cut == 0 && resident.w_4f378 == 0;
    const auto table = state.draw_block + 0x80d4U;
    if (shown)
        compass_quad(table, 0x800b0f7c, placed, false);
    auto upright = identity(trig);
    multiply_rotation(gte, look, upright);
    upright.t[2] = 0x1000;
    placed = compose(gte, base, upright);
    // MulMatrix0 (8004920c): the rotation (and its sign halfword) only.
    auto product = upright;
    multiply_rotation(gte, base, product);
    state.matrix_afa84.r = product.r;
    state.matrix_afa84.pad = product.pad;
    gte.transform.r = base.r;
    gte.transform.t = base.t;
    auto view = identity(trig);
    multiply_rotation(gte, c.previous_view, view);
    view.t[2] = 0x1000;
    placed = compose(gte, base, view);
    base.r = placed.r; // 80074038 copies the rotation and translation
    base.t = placed.t;
    const auto tilt = field::rotation_matrix({0x400, 0, 0}, trig);
    if (shown) {
        for (std::uint32_t k = 0; k < 4; ++k) {
            auto offset = identity(trig);
            offset.t[0] = s16(overlay_half(0x800adc34 + k * 4));
            offset.t[2] = s16(overlay_half(0x800adc36 + k * 4));
            auto letter = compose(gte, base, offset);
            letter.r = tilt.r; // 8007409c
            compass_quad(table, 0x800b0dbc + k * 0x70, letter, true);
        }
        for (std::uint32_t k = 0; k < 16; ++k)
            compass_quad(table, 0x800b06bc + k * 0x70, base, false);
        for (std::uint32_t k = 0; k < 4; ++k)
            compass_quad(table, 0x800b0fec + k * 0x70, base, false);
    }
    add_primitive(table, 0x800b1e00 + state.draw_buffer * 0xc0);
    gte.screen.offset_x = s32(u32(0xa0) << 16U);
    gte.screen.offset_y = s32(u32(0x70) << 16U);
    gte.screen.h = static_cast<std::uint16_t>(c.projection);
}

// Field 8007554c, resumable at `from` (the call the frame makes next).
void Program::field_frame(FrameServices &services, const ProgramObserver &observe, FrameStep from) {
    auto &state = loaded(*this);
    if (state.event_control.diagnostic_suppression == 0)
        throw MissingDependency({"field_frame", 0x8007554c, {}, {}},
                                "symbol:field-diagnostic-output", false,
                                "Unsuppressed frame diagnostic output is not recovered");
    // Interrupts that arrived while a step ran are delivered when it ends.
    const auto done = [&](std::string_view operation, std::uint32_t address) {
        deliver_arrivals(address);
        observed(observe, *this, {operation, address, {}, {}}, true);
    };
    const auto unrecovered = [&](std::string_view operation, std::uint32_t address, const char *id,
                                 const char *reason) {
        throw MissingDependency({operation, address, {}, {}}, id, false, reason);
    };
    const auto block = [&] { return state.draw_block; };
    switch (from) {
    case FrameStep::start:
        // 8007555c VSync(1); the VSync(-1) that follows only paces the final wait.
        state.frame_start_hcount = take_service(services.hblank_counts, "VSync(1) at frame start");
        deliver_arrivals(0x8007555c);
        field_move(observe);
        deliver_arrivals(0x800739c0);
        [[fallthrough]];
    case FrameStep::emitters:
        // 80086908: the listener for positional sound emitters.
        if (state.emitter_source >= 0 && state.emitter_source <= 2)
            frame_emitters(static_cast<std::uint32_t>(state.emitter_source));
        done("field_frame_emitters", 0x80086908);
        [[fallthrough]];
    case FrameStep::fade:
        frame_fade();
        done("field_frame_fade", 0x80071cb4);
        [[fallthrough]];
    case FrameStep::compass:
        frame_compass(services);
        done("field_frame_compass", 0x80074108);
        [[fallthrough]];
    case FrameStep::models:
        // The model and character passes run on a stack in the scratchpad
        // (1f8003fc); no Program state lives there.
        frame_models();
        done("field_frame_models", 0x800748e8);
        [[fallthrough]];
    case FrameStep::characters:
        frame_characters(services, observe);
        done("field_frame_characters", 0x800752c8);
        [[fallthrough]];
    case FrameStep::particles:
        // 800a9688: particle emitters of the 64 slots (800b14b0).
        if (state.particles_paused == 0)
            for (std::size_t slot = 0; slot < state.particle_slots.size(); ++slot)
                if (state.particle_slots[slot] == 1)
                    unrecovered("field_frame_particles", 0x800a9688, "symbol:field-particles",
                                "Active particle emitters are not recovered");
        done("field_frame_particles", 0x800a9688);
        [[fallthrough]];
    case FrameStep::distortion:
        // 800a4dac: the screen distortion effect.
        if (state.distortion != 0)
            unrecovered("field_frame_distortion", 0x800a4dac, "symbol:field-screen-distortion",
                        "The screen distortion effect is not recovered");
        done("field_frame_distortion", 0x800a4dac);
        [[fallthrough]];
    case FrameStep::call_800a84c0:
        if (state.w_af278 != 0)
            unrecovered("field_frame_800a84c0", 0x800a84c0, "symbol:field-800a84c0",
                        "The display of 800a84c0 is not recovered");
        done("field_frame_800a84c0", 0x800a84c0);
        [[fallthrough]];
    case FrameStep::call_80075484:
        if (state.h_b00b2 != 0 && state.w_adb50 == 0)
            unrecovered("field_frame_80075484", 0x800273c4, "symbol:field-80075484",
                        "The drawing of 80075484 (800273c4) is not recovered");
        done("field_frame_80075484", 0x80075484);
        [[fallthrough]];
    case FrameStep::call_8007520c:
        if (resident.w_4f380 == 0 && state.w_b2264 != 0)
            unrecovered("field_frame_8007520c", 0x8007520c, "symbol:field-8007520c",
                        "The drawing of 8007520c is not recovered");
        done("field_frame_8007520c", 0x8007520c);
        [[fallthrough]];
    case FrameStep::call_800abec8:
        if (state.w_adb54 != 0)
            unrecovered("field_frame_800abec8", 0x800abec8, "symbol:field-800abec8",
                        "The packets of 800abec8 are not recovered");
        done("field_frame_800abec8", 0x800abec8);
        [[fallthrough]];
    case FrameStep::drawn_time:
        // 80075694 VSync(1).
        state.frame_drawn_hcount = take_service(services.hblank_counts, "VSync(1) after drawing");
        done("field_frame_drawn_time", 0x80075694);
        [[fallthrough]];
    case FrameStep::draw_sync:
        draw_sync(services);
        done("field_frame_draw_sync", 0x800445d0);
        [[fallthrough]];
    case FrameStep::dialogue_timers:
        frame_dialogue_timers();
        done("field_frame_dialogue_timers", 0x800805f4);
        [[fallthrough]];
    case FrameStep::dialogue:
        frame_dialogue(services, block() + 0x80d4U);
        done("field_frame_dialogue", 0x8008004c);
        [[fallthrough]];
    case FrameStep::vertical_sync: {
        // 800756cc VSync(0): wait for the next vertical blank; libetc keeps
        // the counter and root counter 1 at its return.
        if (services.vblank_waits.empty())
            throw ServiceUnavailable("VSync(0) result");
        const auto wait = services.vblank_waits.front();
        services.vblank_waits.pop_front();
        resident.vsync_hcount = wait[0];
        resident.vsync_previous = wait[1];
        done("field_frame_vertical_sync", 0x8004b54c);
        [[fallthrough]];
    }
    case FrameStep::timed_release:
        // Resident 80032cb8: release blocks whose frame countdown ran out.
        // Tag 10's word (80059fcc) holds the blocks released after a frame
        // countdown.
        if (resident.heap.tag_words[10] != 0)
            unrecovered("field_frame_timed_release", 0x80032cb8, "symbol:heap-timed-release",
                        "Frame-timed heap releases are not recovered");
        done("field_frame_timed_release", 0x80032cb8);
        [[fallthrough]];
    case FrameStep::clear:
        // Clear the next frame's draw area to the field's background color,
        // black while the camera cuts (mode 3 copies VRAM instead).
        if (state.camera_cut != 0 && state.background_mode == 3)
            unrecovered("field_frame_clear", 0x8004495c, "symbol:field-background-copy",
                        "The MoveImage background of mode 3 is not recovered");
        {
            const auto &c = state.clear_color;
            const auto color =
                state.camera_cut != 0 ? 0U : u32(c[0]) | u32(c[1]) << 8U | u32(c[2]) << 16U;
            clear_image(services, block() + 0x5c, color);
        }
        done("field_frame_clear", 0x80044764);
        [[fallthrough]];
    case FrameStep::environments:
        put_disp_env(block() + 0xb8);
        put_draw_env(services, block());
        done("field_frame_environments", 0x80044c44);
        [[fallthrough]];
    case FrameStep::uploads: {
        // Resident 80025044: this buffer's pending image uploads and clears.
        // Nodes (rect, source, next) come from the buffer's sprite arena
        // (800251c8); a node without source clears its rectangle.
        auto &uploads = resident.sprite_uploads.at(resident.sprite_buffer);
        for (auto rect = uploads; rect != 0; rect = memory(rect + 0xc)) {
            if (const auto source = memory(rect + 8); source != 0)
                load_image_at(services, rect, source);
            else
                clear_image(services, rect, 0);
        }
        uploads = 0;
        done("field_frame_uploads", 0x80025044);
        [[fallthrough]];
    }
    case FrameStep::call_800920d8:
        if (state.h_afea8 > 0)
            unrecovered("field_frame_800920d8", 0x80027eac, "symbol:field-800920d8",
                        "The per-frame calls of 800920d8 (80027eac) are not recovered");
        done("field_frame_800920d8", 0x800920d8);
        [[fallthrough]];
    case FrameStep::load:
        if (state.pending_load != 0) {
            load_image_at(services, 0x800afc58, state.pending_load_source);
            state.pending_load = 0;
        }
        done("field_frame_load", 0x800757f8);
        [[fallthrough]];
    case FrameStep::tables:
        // AddPrims (80043b84 via 80075458): the model table joins the frame's
        // small table when no cut is in progress.
        if (state.camera_cut == 0) {
            const auto depth = static_cast<std::uint32_t>(state.ot_depth) * 4U;
            if (state.w_adb4c != 0)
                add_primitives(block() + 0xcc + depth, block() + 0x40d0 + depth, block() + 0x40d0);
            add_primitives(block() + 0x80f0, block() + 0xcc + depth, block() + 0xcc);
        }
        done("field_frame_tables", 0x80075850);
        [[fallthrough]];
    case FrameStep::draw:
        draw_otag(services, block() + 0x80f0);
        // The frame then waits until VSync(-1) reaches its start count plus
        // 800b217c + 2; the wait changes no RAM.
        done("field_frame", 0x8007554c);
        break;
    }
}

} // namespace xem::reconstruction
