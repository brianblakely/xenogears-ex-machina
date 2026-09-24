#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

// Platform services that interrupt-context code of the resident executable
// dc0b2dd7... reads. These are external inputs, not original RAM: a native
// runtime supplies them from its own hardware services, the analysis host
// from recorded observations.
namespace xem::reconstruction {

class PlatformInputError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// One ordered platform event. A `read` is the value an original hardware
// register load returned, identified by the address of that load instruction
// (its site). An `interrupt` is the arrival of an interrupt exception while
// recovered code waits; its handler then consumes the reads that follow.
// Arrival time is external: waits deliver the next interrupt when they would
// otherwise poll again.
struct PlatformInput {
    enum class Kind : std::uint8_t { read, interrupt };
    Kind kind{Kind::read};
    std::uint32_t site{};
    std::uint32_t value{};
    bool operator==(const PlatformInput &) const = default;
};

// Consume the next input; it must be a read from `site` whose value fits
// `width` bytes (a sign-extending load supplies its extended register value).
std::uint32_t platform_read(std::deque<PlatformInput> &inputs, std::uint32_t site,
                            std::uint32_t width, bool sign_extended = false);

inline constexpr std::uint32_t raw_sector_bytes = 2352;
using RawSector = std::array<std::uint8_t, raw_sector_bytes>;

// Native disc service contract. The drive delivers raw 2352-byte sectors of
// the disc image, one per data-ready interrupt. A read command (ReadN 06,
// ReadS 1b) starts at the last Setloc position when one was sent since the
// previous read command, else continues; each delivery advances by one
// sector. The data FIFO of a delivered sector starts at byte 12 (header,
// subheader, data) when the last Setmode selected whole sectors (mode bit
// 0x20), else at byte 24 (data only). Pause, Stop and Init end delivery.
struct DiscDrive {
    // Host service: the raw sector at a logical block address (Setloc
    // position less 150). Throws when unavailable.
    std::function<RawSector(std::uint32_t)> read_sector;
    std::optional<std::uint32_t> next;   // Sector the next data-ready delivers
    std::optional<std::uint32_t> target; // Setloc position not yet used by a read
    std::uint8_t mode{};                 // Last Setmode parameter the drive received
    bool reading{};
    std::optional<RawSector> buffer;      // Delivered sector, readable after a data request
    std::uint32_t cursor{};               // FIFO bytes of `buffer` already transferred
    std::vector<std::uint32_t> delivered; // Delivered sectors, in order (observation)

    // A command the controller received (its parameter bytes first).
    void command(std::uint8_t code, std::span<const std::uint8_t> parameters);
    // A data-ready interrupt: the next sector enters the buffer.
    void data_ready();
    // The next `bytes` FIFO bytes of the buffered sector.
    std::vector<std::uint8_t> transfer(std::uint32_t bytes);
};

} // namespace xem::reconstruction
