// Field overlay 8007554c: one field frame. The move phase, then drawing into
// the current draw buffer block, buffer presentation and the frame-rate wait.
// Original addresses name correlations only; owned records are Program state.
#include "xem/reconstruction/field_collision.hpp"
#include "xem/reconstruction/field_view.hpp"
#include "xem/reconstruction/original_layout.hpp"
#include "xem/reconstruction/program.hpp"

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

std::uint32_t Program::memory(std::uint32_t address, std::size_t width) const {
    if (field && field->packets.contains(address, width))
        return field->packets.word(address, width);
    std::array<std::uint8_t, 4> bytes{};
    read_original(*this, address, std::span(bytes).first(width));
    return word(bytes, 0, width);
}

void Program::set_memory(std::uint32_t address, std::uint32_t value, std::size_t width) {
    if (field && field->packets.contains(address, width)) {
        field->packets.put(address, value, width);
        return;
    }
    std::array<std::uint8_t, 4> bytes{};
    put(bytes, 0, value, width);
    write_original(*this, address, std::span(bytes).first(width));
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
    // RotAverage4 (8004a7bc): RTPT on three corners, RTPS on the fourth.
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
    load_image(services, 0x800b004c, 0x800afd24);
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

// Field 8007554c.
void Program::field_frame(FrameServices &services, const ProgramObserver &observe) {
    auto &state = loaded(*this);
    if (state.event_control.diagnostic_suppression == 0)
        throw MissingDependency({"field_frame", 0x8007554c, {}, {}},
                                "symbol:field-diagnostic-output", false,
                                "Unsuppressed frame diagnostic output is not recovered");
    // 8007555c VSync(1); the VSync(-1) that follows only paces the final wait.
    state.frame_start_hcount = take_service(services.hblank_counts, "VSync(1) at frame start");
    field_move(observe);
    // 80086908: the listener for positional sound emitters.
    const auto source = state.emitter_source;
    if (source >= 0 && source <= 2)
        frame_emitters(static_cast<std::uint32_t>(source));
    observed(observe, *this, {"field_frame_emitters", 0x80086908, {}, {}}, true);
    frame_fade();
    observed(observe, *this, {"field_frame_fade", 0x80071cb4, {}, {}}, true);
    frame_compass(services);
    observed(observe, *this, {"field_frame_compass", 0x80074108, {}, {}}, true);
    throw MissingDependency({"field_frame_models", 0x800748e8, {}, {}}, "symbol:field-frame-models",
                            false, "The model pass of 800748e8 is not connected");
}

} // namespace xem::reconstruction
