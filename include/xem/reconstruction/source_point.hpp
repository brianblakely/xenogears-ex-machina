#pragma once

#include "xem/reconstruction/field_sprite_factory.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace xem::reconstruction {

// Source correlation for debugging. A script PC is never a machine-code address.
struct SourcePoint {
    std::string_view operation;
    std::uint32_t machine_address{};
    std::optional<std::size_t> actor;
    std::optional<std::uint32_t> event_pc;
    std::optional<field::FieldSpriteArguments> sprite_arguments{};
    std::optional<std::uint32_t> sprite_pc{};
    // Event opcode being executed; extended opcodes are fe00 | extended byte.
    std::optional<std::uint16_t> event_opcode{};
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

} // namespace xem::reconstruction
