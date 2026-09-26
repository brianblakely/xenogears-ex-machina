#pragma once

#include "xem/reconstruction/movie.hpp"

namespace xem::analysis {

// The analysis host's MdecCodec: the general-purpose variable-length decode
// of a PlayStation MDEC bitstream (version 2 and 3 frames) into the MDEC's
// run-level halfwords, with the call bound and resumable context of Sony's
// libpress DecDCTvlc2 that the movie library links (801d4cc8). Its lookup
// tables are libpress data inside the loaded library image (801d802c,
// 801e802c and the two DC tables below the first); the codec reads them
// from RAM like the bitstream. It is a codec, not recovered game behavior.
class LibpressMdecCodec final : public reconstruction::movie::MdecCodec {
  public:
    bool decode_vlc(const Memory &memory, std::uint32_t bitstream, std::uint32_t output,
                    std::uint32_t limit, std::span<std::uint8_t> context) override;
};

} // namespace xem::analysis
