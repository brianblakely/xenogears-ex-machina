#pragma once

#include <cstdint>
#include <functional>
#include <span>

// The movie library of Disc 1 profile na-slus-00664-39c547a9afc6: source
// slot 19 (file a9 of directory 4, sha256 4606650a...), 0x15a1c bytes that
// the field movie player (field 800a7c58) copies to 801d3000. It holds the
// game's streaming player (801d30c4..801d4448), Sony's libpress MDEC
// interface (801d444c..801d5394) and CD stream ring (801d57dc..801d68b4),
// their tables and their statics. Its image is a heap block of the loader;
// the statics below are addressed inside it as the original addresses them.
//
// General-purpose codec work is not reconstructed as game behavior: the
// MDEC decodes macroblocks in hardware (commands are HardwareWrite records,
// its DMA completion an interrupt) and the variable-length decode of each
// frame's bitstream is the MdecCodec service. CD-XA audio sectors never
// reach RAM: the drive decodes them into the SPU's CD input once the player
// selects its file and channel (Setfilter) and real-time audio mode.
namespace xem::reconstruction::movie {

inline constexpr std::uint32_t library_address = 0x801d3000;
inline constexpr std::uint32_t library_bytes = 0x15a1c;
inline constexpr std::uint32_t library_file = 0xa9;

// Player configuration (initialized data of the image).
inline constexpr std::uint32_t split_display = 0x801d68b4; // 800a708c: presentation 2
inline constexpr std::uint32_t frame_width = 0x801d68b8;   // u16: last frame header width
inline constexpr std::uint32_t frame_height = 0x801d68ba;  // u16: last frame header height
inline constexpr std::uint32_t image_width = 0x801d68bc;   // u16: in 16-bit VRAM units
inline constexpr std::uint32_t image_height = 0x801d68be;  // u16: the most rows loaded
inline constexpr std::uint32_t vlc_pending = 0x801d68c0;   // a frame's decode is resumable
inline constexpr std::uint32_t vlc_limit = 0x801d68c4;     // halfwords per decode call
inline constexpr std::uint32_t color_mode = 0x801d68c8;    // bit 0: 24-bit output
inline constexpr std::uint32_t end_frame = 0x801d68cc;     // the movie's last frame

// Player statics (zero-initialized data of the image).
inline constexpr std::uint32_t decoded_bitstream = 0x801e8910; // ring sector of the frame
inline constexpr std::uint32_t frame_bitstream = 0x801e8914;   // its data (VLC input)
inline constexpr std::uint32_t vlc_buffer_index = 0x801e8918;
inline constexpr std::uint32_t vlc_buffers = 0x801e891c;   // two run-level buffers
inline constexpr std::uint32_t slice_buffer_index = 0x801e8924;
inline constexpr std::uint32_t slice_buffers = 0x801e8928; // two decoded-slice buffers
inline constexpr std::uint32_t decode_display = 0x801e8930; // display buffer the next frame fills
// Two display buffers of four halfwords each: x, y, then x and y past the
// frame's last column and row.
inline constexpr std::uint32_t display_buffers = 0x801e8934;
inline constexpr std::uint32_t load_display = 0x801e8944; // display buffer slices load into
// Two LoadImage rectangles (x, y, width, height), one per display buffer.
inline constexpr std::uint32_t slice_rects = 0x801e8948;
inline constexpr std::uint32_t mdec_idle = 0x801e8958;   // u8: the MDEC finished a frame
inline constexpr std::uint32_t frame_waiting = 0x801e895c; // u8: no frame was in the ring
inline constexpr std::uint32_t restarted = 0x801e8960;     // u8
inline constexpr std::uint32_t player_state = 0x801e8964;  // s8: -1 stopped, 0 closed, 1, 2
inline constexpr std::uint32_t host_stream = 0x801e8968;   // u8: 8004fe48 (host file table)
inline constexpr std::uint32_t start_sector = 0x801e896c;
inline constexpr std::uint32_t cd_mode = 0x801e8970;
inline constexpr std::uint32_t movie_file = 0x801e8974;  // u16
inline constexpr std::uint32_t xa_channel = 0x801e8978;  // u16
inline constexpr std::uint32_t row_limit = 0x801e897c;   // s16: rows a slice loads at most
inline constexpr std::uint32_t loaded_frame = 0x801e8980; // frame the MDEC decodes
inline constexpr std::uint32_t first_frame = 0x801e8984;
inline constexpr std::uint32_t shown_frame = 0x801e8988; // last frame fully loaded
inline constexpr std::uint32_t ring_buffer = 0x801e898c;
inline constexpr std::uint32_t frame_callback = 0x801e8990; // (frame, x, y) when loaded
inline constexpr std::uint32_t ring_frame = 0x801e8994;    // frame number of the last sector
inline constexpr std::uint32_t load_enabled = 0x801e8998;  // slices go to VRAM
inline constexpr std::uint32_t saved_directory = 0x801e899c; // and its offset at 801e89a0
inline constexpr std::uint32_t fade_in_pending = 0x801e89a4;
inline constexpr std::uint32_t fade_out_pending = 0x801e89a8;
// The stream's last complete frame: its last sector's location (four bytes
// of its record) and its frame number.
inline constexpr std::uint32_t stream_location = 0x801e89ac;
inline constexpr std::uint32_t stream_frame = 0x801e89b0;
inline constexpr std::uint32_t slice_width = 0x801e89c0; // u16: 16 or 24
// A sector the stream's data callback left for the MDEC completion.
inline constexpr std::uint32_t stream_deferred = 0x801e89d0;
inline constexpr std::uint32_t skipped_frames = 0x801e89d4;
inline constexpr std::uint32_t load_restart = 0x801e89e0;
inline constexpr std::uint32_t previous_frame = 0x801e89ec;
inline constexpr std::uint32_t stall_count = 0x801e89f4; // polls without a ring frame

// libpress: the decode-call limit (801d4c94: 2 * halfwords, or ffffff) and
// the 36 bytes of the variable-length decoder's resumable context (801d5008).
inline constexpr std::uint32_t vlc_call_limit = 0x801d4c94;
inline constexpr std::uint32_t vlc_context = 0x801d5008;
inline constexpr std::uint32_t vlc_context_bytes = 36;
// libpress register addresses (initialized pointers of the image): MDEC
// command/data 801d7820 -> 1f801820, control/status 801d7824 -> 1f801824,
// DPCR 801d7828; DMA0 address/block/control 801d77f0..801d77f8; DMA1
// address/block/control 801d77fc..801d7804.
inline constexpr std::uint32_t mdec_registers = 0x801d77f0;

// The variable-length decode of one frame's MDEC bitstream (libpress
// DecDCTvlc2, 801d4cc8): a general-purpose codec service. The library calls
// it with the frame's bitstream to start a frame, or with no bitstream to
// resume one; each call writes at most `limit` bytes of run-level halfwords
// (the output for the MDEC's command stream) and keeps its position in
// `context`. It returns true while output remains. RAM is reached through
// `memory` (original address, byte count); the codec's own tables live in the
// library image.
class MdecCodec {
  public:
    virtual ~MdecCodec() = default;
    using Memory = std::function<std::span<std::uint8_t>(std::uint32_t, std::uint32_t)>;
    virtual bool decode_vlc(const Memory &memory, std::uint32_t bitstream, std::uint32_t output,
                            std::uint32_t limit, std::span<std::uint8_t> context) = 0;
};

} // namespace xem::reconstruction::movie
