#include "xem/reconstruction/field_return.hpp"

#include <algorithm>
#include <bit>
#include <limits>

namespace xem::reconstruction::field::original {
namespace {
constexpr std::size_t globals_size = 0x958;
constexpr std::size_t actor_record_size = 0x174;
constexpr std::size_t variables_size = 0x800;

std::uint32_t word(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t width = 4) {
    if (offset > bytes.size() || width > bytes.size() - offset)
        throw ReturnFormatError("Field-return word exceeds supplied storage");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(bytes[offset + i]) << (8U * i);
    return value;
}

void put(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value,
         std::size_t width = 4) {
    if (offset > bytes.size() || width > bytes.size() - offset)
        throw ReturnFormatError("Field-return store exceeds supplied storage");
    for (std::size_t i = 0; i < width; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}

struct Reader {
    std::span<const std::uint8_t> bytes;
    std::size_t offset{};
    template <std::size_t Size> Block<Size> take() {
        if (offset > bytes.size() || Size > bytes.size() - offset)
            throw ReturnFormatError("Truncated field-return record");
        Block<Size> result;
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), Size, result.begin());
        offset += Size;
        return result;
    }
};

struct Writer {
    std::span<std::uint8_t> bytes;
    std::size_t offset{};
    void append(std::span<const std::uint8_t> input) {
        if (offset > bytes.size() || input.size() > bytes.size() - offset)
            throw ReturnFormatError("Field-return output exceeds supplied storage");
        std::copy(input.begin(), input.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
        offset += input.size();
    }
    template <std::size_t Size> Block<Size> previous() const {
        return Reader{bytes, offset}.take<Size>();
    }
    void halfwords(std::span<const std::uint16_t> input) {
        for (auto value : input) {
            put(bytes, offset, value, 2);
            offset += 2;
        }
    }
};

template <typename Actor> void validate_extensions(const Actor &actor) {
    const bool has_110 = (word(actor.actor, 0x134) & 0x80U) != 0;
    const bool has_114 = (word(actor.actor, 0x12c) & 0x1000U) != 0;
    if (has_110 != actor.extension_110.has_value() || has_114 != actor.extension_114.has_value())
        throw ReturnFormatError(
            "Field-return extension ownership flags and supplied data disagree");
}

void append_globals(Writer &out, const ReturnGlobals &globals) {
    out.append(globals.object_state);
    out.append(globals.transform_state);
    out.append(globals.collision_attributes);
    out.append(globals.field_state);
    out.append(globals.camera_state);
}

ReturnGlobals read_globals(Reader &in) {
    return {in.take<0x38>(), in.take<0x74>(), in.take<0x400>(), in.take<0x2e4>(), in.take<0x1c8>()};
}
} // namespace

Block<48> capture_sprite_return(const SpriteCaptureInput &input, const Block<48> &previous) {
    auto result = previous;
    std::copy_n(input.sprite.begin(), 12, result.begin());
    put(result, 0x10, word(input.sprite, 0x80, 2), 2);
    const auto flags = word(input.sprite, 0xa8);
    put(result, 0x12, (flags >> 11U) & 63U, 2);
    // The original explicitly sign-extends each source byte before a halfword store.
    for (std::size_t i = 0; i < 2; ++i) {
        const auto signed_byte = std::bit_cast<std::int8_t>(input.sprite[0xaf + i]);
        put(result, 0x14 + i * 2, static_cast<std::uint32_t>(signed_byte), 2);
    }
    put(result, 0x18, (flags >> 22U) & 63U, 2);
    std::copy(input.sequencer.begin(), input.sequencer.end(), result.begin() + 0x1c);
    std::copy(input.settings.begin() + 6, input.settings.end(), result.begin() + 0x24);
    put(result, 0x2a, word(input.sprite, 0x2c, 2), 2);
    put(result, 0x2c, word(input.sprite, 0x82, 2), 2);
    return result;
}

FieldCaptureResult capture_field_return(const FieldCaptureInput &input,
                                        std::span<const std::uint8_t> previous) {
    constexpr auto fixed_size = 4 + globals_size + variables_size;
    if (previous.size() < fixed_size ||
        input.actors.size() > (previous.size() - fixed_size) / actor_record_size)
        throw ReturnFormatError("Field-return storage cannot contain the requested actor count");
    std::size_t required = fixed_size + input.actors.size() * actor_record_size;
    for (const auto &actor : input.actors) {
        validate_extensions(actor);
        const std::size_t extra = (actor.extension_110 ? 12 : 0) + (actor.extension_114 ? 16 : 0);
        if (extra > previous.size() - required)
            throw ReturnFormatError("Field-return storage cannot contain optional actor data");
        required += extra;
    }
    FieldCaptureResult result{Bytes(previous.begin(), previous.end()), required, {}};
    // The original truncates descriptor count to one byte and leaves bytes 1..3 intact.
    result.storage[0] = static_cast<std::uint8_t>(input.descriptor_count);
    Writer out{result.storage, 4};
    append_globals(out, input.globals);
    for (const auto &actor : input.actors) {
        out.append(actor.descriptor_auxiliary);
        put(out.bytes, out.offset, actor.descriptor_flags & 0xffffU);
        out.offset += 4;
        out.append(capture_sprite_return(actor.sprite, out.previous<48>()));
        out.append(actor.actor);
        if (actor.extension_110)
            out.append(*actor.extension_110);
        if (actor.extension_114)
            out.append(*actor.extension_114);
    }
    out.halfwords(input.variables);
    for (std::size_t i = 0; i < 3; ++i)
        result.saved_party_modes[i] = input.party_modes[i];
    return result;
}

EventActor ActorReturnRecord::event_state() const {
    EventActor result;
    result.flags = word(actor, 0);
    result.layer_flags = word(actor, 4);
    for (std::size_t i = 0; i < result.slots.size(); ++i) {
        const auto offset = 0x8c + 8 * i;
        result.slots[i] = {static_cast<std::uint16_t>(word(actor, offset, 2)), actor[offset + 2],
                           actor[offset + 3], word(actor, offset + 4)};
    }
    result.pc = static_cast<std::uint16_t>(word(actor, 0xcc, 2));
    result.selected_slot = actor[0xce];
    return result;
}

ParsedFieldReturn parse_field_return(std::span<const std::uint8_t> storage,
                                     std::size_t event_actor_count) {
    constexpr auto fixed_size = 4 + globals_size + variables_size;
    if (storage.size() < fixed_size ||
        event_actor_count > (storage.size() - fixed_size) / actor_record_size)
        throw ReturnFormatError("Truncated field return or impossible event actor count");
    ParsedFieldReturn result;
    result.descriptor_count = storage[0];
    Reader in{storage, 4};
    result.globals = read_globals(in);
    result.actors.reserve(event_actor_count);
    for (std::size_t i = 0; i < event_actor_count; ++i) {
        ActorReturnRecord actor;
        actor.descriptor_auxiliary = in.take<8>();
        actor.stored_descriptor_flags = word(in.take<4>(), 0);
        actor.sprite_checkpoint = in.take<48>();
        actor.actor = in.take<0x138>();
        if ((word(actor.actor, 0x134) & 0x80U) != 0)
            actor.extension_110 = in.take<12>();
        if ((word(actor.actor, 0x12c) & 0x1000U) != 0)
            actor.extension_114 = in.take<16>();
        result.actors.push_back(actor);
    }
    for (auto &value : result.variables)
        value = static_cast<std::uint16_t>(word(in.take<2>(), 0, 2));
    result.bytes_used = in.offset;
    return result;
}

FieldRestoreResult restore_field_return_data(const ParsedFieldReturn &snapshot,
                                             std::span<const ActorRestoreTarget> initialized,
                                             const RestoreAllocation &allocate) {
    if (initialized.size() != snapshot.actors.size())
        throw ReturnFormatError("Field return needs every initialized event actor");
    for (const auto &actor : snapshot.actors) {
        validate_extensions(actor);
        if ((actor.extension_110 || actor.extension_114) && !allocate)
            throw ReturnFormatError("Field-return extension allocator is required");
    }
    FieldRestoreResult result{snapshot.descriptor_count,
                              snapshot.globals,
                              {initialized.begin(), initialized.end()},
                              snapshot.variables,
                              {},
                              snapshot.bytes_used};
    result.pending_sprite_checkpoints.reserve(snapshot.actors.size());
    for (std::size_t i = 0; i < snapshot.actors.size(); ++i) {
        const auto &saved = snapshot.actors[i];
        auto &target = result.actors[i];
        const auto retained_resource = word(target.actor, 0x118);
        target.descriptor_auxiliary = saved.descriptor_auxiliary;
        target.descriptor_flags =
            (target.descriptor_flags & 0xffff0000U) | (saved.stored_descriptor_flags & 0xffffU);
        target.actor = saved.actor;
        put(target.actor, 0x118, retained_resource);
        target.extension_110 = saved.extension_110;
        target.extension_114 = saved.extension_114;
        if (saved.extension_110)
            put(target.actor, 0x110, allocate(i, *saved.extension_110));
        if (saved.extension_114)
            put(target.actor, 0x114, allocate(i, *saved.extension_114));
        result.pending_sprite_checkpoints.push_back(saved.sprite_checkpoint);
    }
    return result;
}

} // namespace xem::reconstruction::field::original
