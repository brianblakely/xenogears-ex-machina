// Runs a recovered field entry from an original RAM image. The image is the
// only game-state input; expected exit images never reach this process.
#include "field_memory.hpp"

#include "xem/reconstruction/resident_heap.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

namespace game = xem::reconstruction;
namespace field = game::field;
namespace analysis = xem::analysis;
namespace {
struct InputError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct HostBudget : std::runtime_error {
    using std::runtime_error::runtime_error;
};
// Thrown by the host at a requested library boundary; not a game result.
struct BoundaryReached {};

std::vector<std::uint8_t> read_file(const char *path, std::size_t maximum) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw InputError(std::string("Cannot open ") + path);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)), {});
    if (bytes.size() > maximum)
        throw InputError(std::string("Oversized input ") + path);
    return bytes;
}
std::vector<std::uint8_t> unhex(std::string_view text) {
    if (text.size() % 2 != 0)
        throw InputError("Odd hexadecimal byte string");
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() / 2);
    const auto digit = [](char c) -> std::uint8_t {
        if (c >= '0' && c <= '9')
            return static_cast<std::uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f')
            return static_cast<std::uint8_t>(c - 'a' + 10);
        throw InputError("Malformed hexadecimal byte string");
    };
    for (std::size_t i = 0; i < text.size(); i += 2)
        bytes.push_back(static_cast<std::uint8_t>(digit(text[i]) << 4U | digit(text[i + 1])));
    return bytes;
}
std::string hex(std::span<const std::uint8_t> bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (auto value : bytes) {
        result += digits[value >> 4U];
        result += digits[value & 15U];
    }
    return result;
}
std::string quote(std::string_view text) {
    std::ostringstream out;
    out << '"';
    for (const auto raw : text) {
        const auto character = static_cast<unsigned char>(raw);
        if (character == '\\' || character == '"')
            out << '\\' << static_cast<char>(character);
        else if (character < 0x20)
            out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                << static_cast<unsigned>(character);
        else
            out << static_cast<char>(character);
    }
    out << '"';
    return out.str();
}
// Memory a pure resident service writes outside Program state: the
// decoder's output range.
using ServiceOutput = std::optional<std::pair<std::uint32_t, std::vector<std::uint8_t>>>;
// The owned ranges of the exported state (and a service's output), as JSON
// objects.
std::string owned_ranges(const game::Program &program, const ServiceOutput &service_output = {}) {
    analysis::OriginalMemory image;
    image.ram.assign(analysis::ram_bytes, 0);
    const auto ranges = program.field    ? analysis::export_field(program, image)
                        : program.battle ? analysis::export_battle(program, image)
                        : program.menu   ? analysis::export_menu(program, image)
                                         : analysis::export_resident(program, image);
    std::ostringstream owned;
    if (service_output) {
        owned << "{\"name\":\"decoder_output\",\"address\":" << service_output->first
              << ",\"hex\":" << quote(hex(service_output->second)) << '}';
        if (!ranges.empty())
            owned << ',';
    }
    for (std::size_t i = 0; i < ranges.size(); ++i) {
        const auto &range = ranges[i];
        owned << (i ? "," : "") << "{\"name\":" << quote(range.name)
              << ",\"address\":" << range.address
              << ",\"hex\":" << quote(hex(image.range(range.address, range.size))) << '}';
    }
    return owned.str();
}
// Hardware register stores from the `first`th on, as JSON objects.
std::string hardware_writes(const game::Program &program, std::size_t first) {
    std::ostringstream out;
    const auto &writes = program.resident.hardware_writes;
    for (auto i = first; i < writes.size(); ++i)
        out << (i > first ? "," : "") << "{\"address\":" << writes[i].address
            << ",\"value\":" << writes[i].value << ",\"width\":" << writes[i].width << '}';
    return out.str();
}
// GTE control registers 0-30 (FLAG is transient).
std::string gte_controls(const game::Program &program) {
    std::ostringstream out;
    for (std::uint32_t i = 0; i < 31; ++i)
        out << (i ? "," : "") << program.resident.gte.control(i);
    return out.str();
}
void optional(std::ostream &out, const auto &value) {
    if (value)
        out << +*value;
    else
        out << "null";
}
} // namespace

namespace {
int run_case(int argc, char **argv) {
    std::string status = "invalid_input", reason, dependency;
    std::string_view entry = argc > 1 ? argv[1] : "unknown";
    game::SourcePoint point{"prepare_start", 0, {}, {}};
    std::optional<std::uint8_t> opcode;
    std::map<std::uint16_t, std::size_t> executed; // Event opcodes entered, fe00|x extended.
    std::optional<std::uint32_t> sprite_pc;
    std::uint32_t work = 0;
    bool executing = false;
    std::optional<game::Program> program;
    std::ostringstream frames; // Completed boundaries of "field_frames".
    std::optional<std::uint32_t> return_value;
    std::string result = "null";
    ServiceOutput service_output;
    try {
        if (argc != 15)
            throw InputError("Usage: xem-memory-runner ENTRY BUDGET STOP RAM SCRATCHPAD "
                             "FIELD_SOURCE OVERLAY RESOURCES GTE REGISTERS IO PLATFORM DISC "
                             "SERVICES");
        // Resident entries need no loaded field; FIELD_SOURCE, OVERLAY and
        // RESOURCES are unused. OVERLAY is the decoded field overlay image.
        // PLATFORM lists recorded platform inputs; DISC is the raw track the
        // disc drive service delivers sectors from (may be empty).
        const bool resident_entry =
            entry == "heap_allocate" || entry == "heap_release" || entry == "music_stop" ||
            entry == "disc_read_file" || entry == "disc_read_files" ||
            entry == "disc_read_stream" || entry == "decode_block" || entry == "sound_set_mode" ||
            entry == "sound_set_master" || entry == "sound_set_cd" ||
            entry == "sound_update_voices" || entry == "set_next_mode" || entry == "field_exit" ||
            entry == "battle_mode_exit" || entry == "interrupt_dispatch" || entry == "sound_tick" ||
            entry == "sequence_open" || entry == "sequence_start" ||
            entry.starts_with("mode_dispatch");
        // Field entries beyond the update: one extended event handler, the
        // movie loop's decision.
        const bool field_entry = entry == "field_event_extended" || entry == "movie_decision" ||
                                 entry == "music_poll" || entry == "music_chunk";
        const bool battle_entry =
            entry == "battle_commit" || entry == "battle_apply" || entry == "battle_alive" ||
            entry == "battle_rewards" || entry == "battle_reward_totals" ||
            entry == "battle_drops" || entry == "battle_atb" || entry == "battle_reload" ||
            entry == "battle_ai" || entry == "battle_results_step" ||
            entry == "battle_setup_phase" || entry == "battle_prologue" ||
            entry == "battle_after_load" || entry == "battle_after_scene" ||
            entry == "battle_adjust_party" || entry == "battle_place_party" ||
            entry == "battle_scene_files" || entry == "battle_setup_files" ||
            entry == "battle_load_prologue" || entry == "battle_effect_lists" ||
            entry == "battle_release_setup" || entry == "battle_renderer_setup" ||
            entry == "battle_stage_setup" || entry == "battle_opening" ||
            entry == "battle_opening_images" || entry == "battle_opening_windows" ||
            entry == "battle_loader" || entry.starts_with("battle_turn_");
        const bool menu_save_entry = entry == "menu_save_serialize" || entry == "menu_save_file" ||
                                     entry == "menu_save_seal" || entry == "menu_save_store" ||
                                     entry == "menu_names_decode" || entry == "menu_load_check" ||
                                     entry == "menu_load_restore" || entry == "menu_load_apply" ||
                                     entry == "menu_load_slot_valid" ||
                                     entry == "menu_load_find_slot" ||
                                     entry == "menu_text_encode" || entry == "menu_text_decode";
        const bool menu_entry = entry == "menu_item_effect" || entry == "menu_item_use" ||
                                entry == "menu_equip_swap" || entry == "menu_equip_bonus" ||
                                entry == "menu_equip_stats" || menu_save_entry;
        const bool transition_entry = entry == "field_save" || entry == "field_preload" ||
                                      entry == "field_map_change_step" ||
                                      entry == "field_map_change_start";
        // The field reload 800a5c40 and the steps around each field frame.
        const bool reload_entry =
            entry == "field_pre_frame" || entry == "field_post_frame" ||
            entry == "field_reload_draw" || entry == "field_reload_shade" ||
            entry == "field_reload_fade_in" || entry == "field_reload_fade_frame" ||
            entry == "field_reload_finish" || entry == "field_reload_teardown" ||
            entry == "field_reload" || entry == "field_load";
        // The field main loop leaving for battle (80078334..8007954c).
        const bool battle_exit_entry = entry == "field_battle_start" ||
                                       entry == "field_battle_leave" || entry == "field_teardown" ||
                                       entry == "field_battle_release" ||
                                       entry == "field_battle_exit";
        if (entry != "field_event_pass" && entry != "field_update" && entry != "field_move" &&
            entry != "battle_mode_start" && entry != "field_checkpoints" &&
            !entry.starts_with("field_frame") && !resident_entry && !battle_entry && !menu_entry &&
            !field_entry && !transition_entry && !reload_entry && !battle_exit_entry)
            throw InputError("Unsupported memory-image entry");
        const auto hex_words = [](const char *text, std::size_t count, const char *message) {
            std::vector<std::uint32_t> values;
            std::istringstream words(text);
            for (std::string item; std::getline(words, item, ',');) {
                std::size_t used = 0;
                if (item.empty() || item.size() > 8)
                    throw InputError(message);
                values.push_back(static_cast<std::uint32_t>(std::stoul(item, &used, 16)));
                if (used != item.size())
                    throw InputError(message);
            }
            if (values.size() != count)
                throw InputError(message);
            return values;
        };
        // CPU registers at entry (34 words: GPRs, then HI and LO) supply arguments.
        const auto registers =
            hex_words(argv[10], 34, "REGISTERS must be 34 comma-separated hex words");
        const auto budget = std::stoul(argv[2]);
        const std::string_view stop = argv[3];
        analysis::OriginalMemory memory;
        memory.ram = read_file(argv[4], analysis::ram_bytes);
        const auto scratch = read_file(argv[5], 1024);
        if (scratch.size() != memory.scratchpad.size())
            throw InputError("Scratchpad image must contain exactly 1 KiB");
        std::ranges::copy(scratch, memory.scratchpad.begin());
        if (resident_entry) {
            program = analysis::import_resident(memory);
            // 80039850 reads the sequence event data a disc read placed at A0
            // (its byte length is the data's third word).
            if (entry == "sequence_open")
                analysis::import_disc_data(*program, memory, registers[4],
                                           memory.word(registers[4] + 8));
            // The dispatcher's second heap restart (at the mode table row's
            // BSS end + 4) takes in the RAM below the heap's head, which no
            // other Program value owns.
            if (entry == "mode_dispatch:reinit" || entry == "mode_dispatch:sync") {
                const auto row = 0x8001808cU + memory.word(0x80018088) * 16U;
                const auto start = (memory.word(row + 8) + 4U) & ~3U;
                const auto first = program->resident.heap.head - 8;
                if (start < first) {
                    const auto bytes = memory.range(start, first - start);
                    program->resident.heap_outside.emplace(
                        start, std::vector<std::uint8_t>(bytes.begin(), bytes.end()));
                }
            }
        } else if (entry == "battle_mode_start") {
            program = analysis::import_battle_overlay(memory);
        } else if (battle_entry) {
            program = analysis::import_battle(memory);
        } else if (menu_entry) {
            program = analysis::import_menu(memory);
            // The save payload (A0; S4 at the seal) and the load buffer (S4
            // at the check; A0 = buffer + 100 at restore and apply) are heap
            // blocks the menu allocates for the transfer.
            if (entry == "menu_save_serialize" || entry == "menu_save_file" ||
                entry == "menu_save_store" || entry == "menu_load_restore" ||
                entry == "menu_load_apply")
                analysis::import_menu_block(*program, memory, registers[4]);
            else if (entry == "menu_save_seal" || entry == "menu_load_check")
                analysis::import_menu_block(*program, memory, registers[20]);
        } else if (entry == "field_load" || entry == "field_battle_release" ||
                   entry == "field_battle_exit") {
            // Between the reload's teardown and the load: no field is loaded.
            program =
                analysis::import_unloaded_field(memory, read_file(argv[7], analysis::ram_bytes));
        } else {
            // Qualified resource extents: one "address size" decimal pair per line.
            std::vector<analysis::ResourceExtent> resources;
            std::ifstream manifest(argv[8]);
            if (!manifest)
                throw InputError("Cannot open resource manifest");
            for (std::uint32_t address = 0; manifest >> address;) {
                std::size_t size = 0;
                if (!(manifest >> size) || resources.size() == 64)
                    throw InputError("Malformed resource manifest");
                resources.push_back({address, size});
            }
            if (!manifest.eof())
                throw InputError("Malformed resource manifest");
            program = analysis::import_field(memory, read_file(argv[6], analysis::ram_bytes),
                                             read_file(argv[7], analysis::ram_bytes), resources);
            // A poll with the sequence read issued (8004f358 1) may open the
            // sequence over the event data the read placed at 80062648 (its
            // byte length is the data's third word).
            if (entry == "music_poll" && program->resident.music.sequence_pending == 1)
                analysis::import_disc_data(*program, memory, 0x80062648, memory.word(0x80062650));
        }
        // The 64 GTE registers at entry: data 0-31, then control 0-31. SXYP,
        // IRGB/ORGB and LZCR mirror other registers and are not written.
        const auto gte = hex_words(argv[9], 64, "GTE must be 64 comma-separated hex words");
        for (std::uint32_t i = 0; i < 32; ++i)
            if (i != 15 && i != 28 && i != 29 && i != 31)
                program->resident.gte.set_data(i, gte[i]);
        for (std::uint32_t i = 0; i < 32; ++i)
            program->resident.gte.set_control(i, gte[32 + i]);
        // The hardware I/O page observed at entry (1f801000..1f801fff).
        const auto io = read_file(argv[11], program->resident.io.size());
        if (io.size() != program->resident.io.size())
            throw InputError("IO page must contain exactly 4 KiB");
        std::ranges::copy(io, program->resident.io.begin());
        analysis::load_platform(*program, argv[12], argv[13]);
        analysis::attach_interrupt_memory(*program, memory);
        // A field teardown releases whole heap blocks: own all their bytes.
        if (entry == "field_reload_teardown" || entry == "field_reload" || entry == "field_load" ||
            entry == "field_teardown" || entry == "field_battle_release")
            analysis::import_heap_contents(*program, memory);
        // Platform results for a field frame, one "name value..." per line
        // (hexadecimal), in the order the original consumed them. A "frame"
        // line starts the next main-loop iteration of "field_frames": the
        // code between frames, then the frame.
        std::vector<game::FrameServices> sections(1);
        {
            std::ifstream lines(argv[14]);
            if (!lines)
                throw InputError("Cannot open services");
            for (std::string line; std::getline(lines, line);) {
                std::istringstream fields(line);
                std::string name;
                std::uint32_t first = 0, second = 0;
                if (line == "frame") {
                    sections.emplace_back();
                    continue;
                }
                if (!(fields >> name >> std::hex >> first))
                    throw InputError("Malformed service line");
                auto &services = sections.back();
                if (name == "hblank")
                    services.hblank_counts.push_back(first);
                else if (name == "vblank_wait" && fields >> std::hex >> second)
                    services.vblank_waits.push_back({first, second});
                else if (name == "vblank")
                    services.vblank_counts.push_back(first);
                else if (name == "alarm_polls")
                    services.alarm_polls.push_back(first);
                else if (name == "gpu_status")
                    services.gpu_status.push_back(first);
                else if (name == "dma_busy")
                    services.dma_busy.push_back(first);
                else if (name == "interrupt_mask")
                    services.interrupt_masks.push_back(first);
                else if (name == "gpu_info")
                    services.gpu_info.push_back(first);
                else if (name == "vram_read") {
                    // "vram_read WORDS HEX": the read-back's bytes.
                    std::string text;
                    if (!(fields >> text) || text.size() != std::size_t{first} * 8U)
                        throw InputError("Malformed VRAM read-back");
                    services.vram_reads.push_back(unhex(text));
                } else
                    throw InputError("Unknown service result " + name);
            }
        }
        if (sections.size() != 1 && entry != "field_frames")
            throw InputError("Only field_frames takes several frame sections");
        auto &services = sections.front();
        executing = true;
        const game::ProgramObserver observer = [&](const game::Program &, game::SourcePoint at,
                                                   bool completed) {
            point = at;
            if (!completed && at.event_opcode)
                ++executed[*at.event_opcode];
            if (completed && at.operation == stop)
                throw BoundaryReached{};
            if (!completed && work++ == budget)
                throw HostBudget("Analysis operation budget exhausted before the next operation");
        };
        if (entry == "field_event_pass") {
            const auto pass = program->event_pass(observer);
            std::ostringstream out;
            out << "{\"visited\":" << pass.visited
                << ",\"dispatched_actors\":" << pass.dispatched_actors
                << ",\"diagnostic_requests\":" << pass.diagnostic_requests
                << ",\"gate_stopped_pass\":" << (pass.gate_stopped_pass ? "true" : "false") << '}';
            result = out.str();
        } else if (entry == "field_update") {
            program->field_update(observer);
        } else if (entry == "field_frames") {
            // Consecutive main-loop iterations from one import: the first
            // frame, then per section the code between frames and the next
            // frame. Every boundary's exported state is reported; nothing
            // observed enters between them. The loop's s4 and s5 come from
            // the registers at the imported frame's entry.
            auto &state = *program->field;
            state.combination_latched = registers[20] != 0;
            state.music_saved = registers[21] != 0;
            const auto report = [&](std::string_view boundary, std::size_t written) {
                frames << (frames.tellp() > 0 ? "," : "") << "{\"boundary\":" << quote(boundary)
                       << ",\"gte\":[" << gte_controls(*program) << "],\"owned\":["
                       << owned_ranges(*program) << "],\"hardware_writes\":["
                       << hardware_writes(*program, written) << "]}";
            };
            for (std::size_t k = 0; k < sections.size(); ++k) {
                auto written = program->resident.hardware_writes.size();
                if (k != 0) {
                    program->field_between_frames(sections[k], observer);
                    report("entry", written);
                    written = program->resident.hardware_writes.size();
                }
                program->field_frame(sections[k], observer);
                report("exit", written);
            }
        } else if (entry.starts_with("field_frame")) {
            // "field_frame" or "field_frame:STEP" resumes at a frame step.
            static const std::map<std::string_view, game::FrameStep> steps{
                {"start", game::FrameStep::start},
                {"emitters", game::FrameStep::emitters},
                {"fade", game::FrameStep::fade},
                {"compass", game::FrameStep::compass},
                {"models", game::FrameStep::models},
                {"characters", game::FrameStep::characters},
                {"particles", game::FrameStep::particles},
                {"distortion", game::FrameStep::distortion},
                {"call_800a84c0", game::FrameStep::call_800a84c0},
                {"call_80075484", game::FrameStep::call_80075484},
                {"call_8007520c", game::FrameStep::call_8007520c},
                {"call_800abec8", game::FrameStep::call_800abec8},
                {"drawn_time", game::FrameStep::drawn_time},
                {"draw_sync", game::FrameStep::draw_sync},
                {"dialogue_timers", game::FrameStep::dialogue_timers},
                {"dialogue", game::FrameStep::dialogue},
                {"vertical_sync", game::FrameStep::vertical_sync},
                {"timed_release", game::FrameStep::timed_release},
                {"clear", game::FrameStep::clear},
                {"environments", game::FrameStep::environments},
                {"uploads", game::FrameStep::uploads},
                {"call_800920d8", game::FrameStep::call_800920d8},
                {"load", game::FrameStep::load},
                {"tables", game::FrameStep::tables},
                {"draw", game::FrameStep::draw}};
            auto from = game::FrameStep::start;
            if (entry != "field_frame") {
                const auto found =
                    entry.starts_with("field_frame:")
                        ? steps.find(entry.substr(std::string_view("field_frame:").size()))
                        : steps.end();
                if (found == steps.end())
                    throw InputError("Unknown field frame step");
                from = found->second;
            }
            program->field_frame(services, observer, from);
        } else if (entry == "field_checkpoints") {
            program->checkpoint_pass(observer);
        } else if (entry == "field_save") {
            program->save_field_departure(); // 800a30fc
        } else if (entry == "field_preload") {
            // 8001b484: A0 map data id, A1 slot.
            return_value =
                static_cast<std::uint32_t>(program->preload_field(registers[4], registers[5]));
        } else if (entry == "field_pre_frame") {
            program->field_pre_frame(services); // 80077dac
        } else if (entry == "field_post_frame") {
            program->field_post_frame(); // 80078b5c
        } else if (entry == "field_reload_draw") {
            program->reload_transition_draw(); // 800a6408
        } else if (entry == "field_reload_shade") {
            // 800a5600(S1 >> 16): the argument is formed in the call's delay slot.
            program->reload_transition_shade(
                static_cast<std::uint32_t>(static_cast<std::int32_t>(registers[17]) >> 16));
        } else if (entry == "field_reload_fade_in") {
            program->field_reload_fade_in(services, observer); // 800a6120..800a63a0
        } else if (entry == "field_reload_fade_frame") {
            // One pass from the loop head 800a6148; S1 holds the shade.
            return_value = static_cast<std::uint32_t>(program->field_reload_fade_frame(
                services, static_cast<std::int32_t>(registers[17]), observer));
        } else if (entry == "field_load") {
            program->load_field(services, registers[29] - 0xa0, observer); // 80070cc8
        } else if (entry == "field_reload") {
            // From the reload's entry 800a5c40: its frame is SP - 48h.
            program->field_reload(services, registers[29] - 0x48, observer);
        } else if (entry == "field_reload_teardown") {
            program->field_reload_teardown(services, observer); // From 800a5c40
        } else if (entry == "field_reload_finish") {
            // 800a63a0..800a6400; SP is the reload's frame.
            program->field_reload_finish(services, registers[29]);
        } else if (entry == "field_map_change_step") {
            program->field_map_change_step(services, observer); // 80078494..80078558 of 80077e88
        } else if (entry == "field_map_change_start") {
            // 80078494 up to the reload call: the reload is due.
            if (!program->start_map_change())
                throw std::runtime_error("The recovered step does not reach the original reload");
        } else if (entry == "heap_allocate") {
            // 80031bdc: A0 size, A1 mode; the header records the JAL at RA - 8.
            const auto block = game::resident::heap_allocate(program->resident.heap, registers[4],
                                                             registers[5], registers[31] - 8);
            return_value = block ? block->address : 0;
        } else if (entry == "heap_release") {
            // 800320e8: A0 block. A block on the imported list that the heap does
            // not hold is the caller's; its bytes are its extent in the image. A
            // block holding a sound-driver object (battle 80071288 frees the
            // effect bank) passes the object's bytes; the driver keeps only its
            // now stale address.
            game::resident::HeapBlock block{registers[4], {}};
            const auto &headers = program->resident.heap.headers;
            auto &objects = program->resident.sound.objects;
            const auto object = objects.find(block.address);
            const bool sound_object = object != objects.end();
            if (sound_object) {
                block.bytes = std::move(objects.extract(object).mapped());
            } else if (const auto found = headers.find(block.address - 8);
                       block.address != 0 && found != headers.end() &&
                       (found->second[1] & game::resident::heap_tag_mask) != 0) {
                const auto extent =
                    memory.range(block.address, found->second[0] - block.address - 8);
                block.bytes.assign(extent.begin(), extent.end());
            }
            return_value = static_cast<std::uint32_t>(
                game::resident::heap_release(program->resident.heap, block, registers[31] - 8));
            // A kept block (-1) is not released; the object stays the driver's.
            if (sound_object && !block.bytes.empty())
                objects.emplace(block.address, std::move(block.bytes));
        } else if (entry == "battle_commit") {
            // 80085ccc: A0 attacker slot, A1 target mask, A2 animation.
            program->commit_battle_action(registers[4], registers[5] & 0xffff,
                                          registers[6] & 0xffff);
        } else if (entry == "battle_apply") {
            program->apply_battle_results(registers[4]); // 80085618: A0 queue slot
        } else if (entry == "battle_alive") {
            program->update_battle_alive(); // 8007252c
        } else if (entry == "battle_ai") {
            // 800799c8: A0 enemy slot, A1 flag.
            program->run_battle_enemy_script(registers[4] & 0xff, registers[5] & 0xff);
        } else if (entry.starts_with("battle_turn_")) {
            // Turn-procedure steps between presentation calls; A0 is the
            // actor at 80079778 and 800793f0.
            const auto actor = registers[4] & 0xff;
            const auto step = entry.substr(12);
            // The camera call 800bc404 is presentation: the executor step ends
            // there (hook 80078c64, result 1) and resumes after it (80078c6c);
            // a completed executor returns 0.
            const game::battle::FrameTargets frame = [&](std::uint32_t) {
                return_value = 1;
                throw BoundaryReached{};
            };
            program->run_battle([&](game::battle::Battle &context) {
                if (step == "select") // 1: a slot acts (80071bd8), 0: none (80072254)
                    return_value = game::battle::select_turn(context) ? 1 : 0;
                else if (step == "begin")
                    game::battle::begin_turn(context);
                else if (step == "actions_begin")
                    game::battle::begin_actions(context, actor);
                else if (step == "actions") {
                    game::battle::execute_actions(context, actor, frame);
                    return_value = 0;
                } else if (step == "actions_resume") { // 80078b34: S1 actor, S0 index; S6 flag
                    game::battle::resume_actions(context, registers[17], registers[16],
                                                 (registers[22] & 0xff) != 0, frame);
                    return_value = 0;
                } else if (step == "prepare")
                    return_value = game::battle::prepare_turn(context);
                else if (step == "order")
                    game::battle::order_targets(context);
                else if (step == "settle")
                    game::battle::settle_turn(context);
                else if (step == "finish")
                    game::battle::finish_turn(context);
                else if (step == "decode") // 80089ccc
                    game::battle::decode_input(context, program->resident);
                else if (step == "menu") // 800807c8: S2 member
                    game::battle::menu_step(context, program->resident, registers[18] & 0xff);
                else if (step == "menu_presented") {
                    // The capture brackets the camera and target camera as
                    // presentation; the attack model (1), the frame inside
                    // 800861d0 (2) and the target text (3) end the step at
                    // their call.
                    const game::battle::MenuPresent present =
                        [&](game::battle::MenuPresentation call, std::uint32_t) {
                            using Call = game::battle::MenuPresentation;
                            if (call == Call::attack_model || call == Call::frame ||
                                call == Call::target_text || call == Call::combo_camera) {
                                return_value = call == Call::attack_model  ? 1
                                               : call == Call::frame       ? 2
                                               : call == Call::target_text ? 3
                                                                           : 4;
                                throw BoundaryReached{};
                            }
                            if (call == Call::model_reset)
                                throw game::battle::BattleError(
                                    "The attack model reset 800b8da4 is not bracketed");
                        };
                    game::battle::menu_step(context, program->resident, registers[18] & 0xff,
                                            present);
                } else if (step == "view_resume") { // 80094134: S2 member (8008189c)
                    const game::battle::MenuPresent present =
                        [&](game::battle::MenuPresentation call, std::uint32_t) {
                            using Call = game::battle::MenuPresentation;
                            if (call == Call::frame || call == Call::combo_camera) {
                                return_value = call == Call::frame ? 2 : 4;
                                throw BoundaryReached{};
                            }
                            if (call != Call::camera && call != Call::target_camera)
                                throw game::battle::BattleError(
                                    "A presentation call after the target text is not bracketed");
                        };
                    game::battle::resume_attack_view(context, program->resident,
                                                     registers[18] & 0xff, present);
                } else if (step == "combo_resume") { // 800819e4: S3 member
                    const game::battle::MenuPresent present =
                        [&](game::battle::MenuPresentation call, std::uint32_t) {
                            using Call = game::battle::MenuPresentation;
                            if (call == Call::frame) {
                                return_value = 2;
                                throw BoundaryReached{};
                            }
                            if (call != Call::camera)
                                throw game::battle::BattleError(
                                    "A presentation call after the combo camera is not bracketed");
                        };
                    game::battle::resume_combo(context, program->resident, registers[19], present);
                } else if (step == "attack_resume") // 80087ac0: S0 member
                    game::battle::resume_attack_entry(context, program->resident,
                                                      registers[16] & 0xff);
                else if (step == "confirm_resume") // 80086b34: FP member
                    game::battle::resume_attack_confirm(context, program->resident,
                                                        registers[30] & 0xff);
                else
                    throw InputError("Unsupported turn step");
            });
        } else if (entry == "battle_prologue") {
            program->battle_prologue(); // 80070f40 up to 800b8098
            program->deliver_pending_arrivals();
        } else if (entry == "battle_after_load") {
            program->battle_after_load(); // 800b8098's return up to 800b81bc
        } else if (entry == "battle_after_scene") {
            program->battle_after_scene(registers[2]); // 801e7210's return: V0
        } else if (entry == "battle_setup_files") {
            program->battle_setup_files(); // 8001bbac
            program->deliver_pending_arrivals();
        } else if (entry == "battle_load_prologue") {
            program->battle_load_prologue(registers[4]); // 800b8098: A0 mode
        } else if (entry == "battle_effect_lists") {
            program->battle_effect_lists(); // 800b7870 return up to 801e7210
            program->deliver_pending_arrivals();
        } else if (entry == "battle_release_setup") {
            program->battle_release_setup(); // 80071278 up to 8009892c
            program->deliver_pending_arrivals();
        } else if (entry == "battle_renderer_setup") {
            program->battle_renderer_setup(registers[4]); // 800b81bc: A0 the task's argument
            program->deliver_pending_arrivals();
        } else if (entry == "battle_opening") {
            program->battle_opening(); // 8007118c up to 80077990
            program->deliver_pending_arrivals();
        } else if (entry == "battle_opening_images") {
            program->battle_opening_images(services); // 80077990
            program->deliver_pending_arrivals();
        } else if (entry == "battle_opening_windows") {
            program->battle_opening_windows(); // 8007819c
            program->deliver_pending_arrivals();
        } else if (entry == "battle_loader") {
            // A state of the loading task 801e6fec: A0 the task node.
            program->battle_loader_step(registers[4], services);
            program->deliver_pending_arrivals();
        } else if (entry == "battle_scene_files") {
            program->battle_scene_files(); // 8001bb0c
            program->deliver_pending_arrivals();
        } else if (entry == "battle_stage_setup") {
            // 801e7210: A0 8005949c, A2 the stage, A3 and the caller's stack
            // words SP + 10 and + 14 the origin, colors and tint.
            if (registers[4] != 0x8005949cU)
                throw InputError("The stage setup's scene pointer is not 8005949c");
            const auto sp = registers[29];
            return_value =
                program->battle_stage_setup(services, sp, registers[6], registers[7],
                                            memory.word(sp + 0x10), memory.word(sp + 0x14));
            program->deliver_pending_arrivals();
        } else if (entry == "battle_adjust_party") {
            program->battle_adjust_party(); // 8009892c
        } else if (entry == "battle_place_party") {
            program->battle_place_party(); // 8009892c's return up to 800723e0
        } else if (entry == "battle_setup_phase") {
            // 801e5840: A0 phase; its callees place LoadImage rectangles below SP.
            program->setup_battle_phase(registers[4], services, registers[29]);
            program->deliver_pending_arrivals();
        } else if (entry == "battle_atb") {
            program->tick_battle_timers(); // 8007171c
        } else if (entry == "battle_reload") {
            program->reload_battle_timer(); // 800718bc
        } else if (entry == "battle_results_step") {
            // At the frame routine's return (80071714): RA is the caller's
            // continuation, S0 the window 8008fa60 closes.
            program->run_battle([&](game::battle::Battle &context) {
                return_value = game::battle::result_screen_step(context, program->resident,
                                                                registers[31], registers[16]);
            });
        } else if (entry == "battle_rewards") {
            program->grant_battle_rewards(); // 801e2794
        } else if (entry == "battle_reward_totals") {
            program->total_battle_rewards(); // 801e2280 up to 801e23d4
        } else if (entry == "battle_drops") {
            // 801e1444: A0 ids, A1 counts, A2 categories.
            program->add_battle_drops(registers[4], registers[5], registers[6]);
        } else if (entry == "menu_item_effect") {
            // 801e31c0: A0 table directory, A1 character, A2 item.
            return_value =
                program->apply_menu_item_effect(registers[4], registers[5], registers[6]);
        } else if (entry == "menu_item_use") {
            // 801dbba4 inside 801db920: FP inventory entry, S3 target mask.
            program->use_menu_item(registers[30], registers[19]);
        } else if (entry == "menu_equip_swap") {
            // 801df0d4: A0 party slot, A1 part, A2 special, A3 gear.
            return_value = program->swap_menu_equipment(registers[4], registers[5], registers[6],
                                                        registers[7]);
        } else if (entry == "menu_equip_bonus") {
            program->menu_equipment_bonuses(registers[4], registers[5]); // 801e36d4
        } else if (entry == "menu_equip_stats") {
            program->menu_equipment_stats(registers[4], registers[5]); // 801e3a80
        } else if (menu_save_entry) {
            // 801cb184 keeps its name buffer at its SP + 28; the buffer's
            // bytes at the call are the caller's stack as the entry image
            // holds it (serialize: SP - 58 - 58 + 28, nothing between writes
            // there; apply: 801e4d10's frame, SP - 18 - 30, whose first 10
            // bytes it leaves and whose +10 holds its saved S0, the payload).
            const auto stack = [&](std::uint32_t address) {
                game::menu::NameScratch bytes{};
                std::ranges::copy(memory.range(address, bytes.size()), bytes.begin());
                return bytes;
            };
            const auto sp = registers[29];
            if (entry == "menu_save_serialize" || entry == "menu_save_file") {
                // 801cba4c: A0 payload, A2 file digit (A1 card unused).
                auto buffer = stack(sp - 0x58 - 0x58 + 0x28);
                program->serialize_menu_save(registers[4], registers[6] & 0xff, buffer);
                if (entry == "menu_save_file") {
                    // Then the seal of 801cc424 and the card file 801cbd90
                    // writes: header template, then the payload.
                    static_cast<void>(program->seal_menu_save(registers[4]));
                    const auto block = game::menu::save_file_block(
                        game::menu::Menu{*program->menu, program->resident.sound}, registers[4]);
                    result = "{\"block\":" + quote(hex(block)) + '}';
                }
            } else if (entry == "menu_save_seal") {
                return_value = program->seal_menu_save(registers[20]); // S4 payload
            } else if (entry == "menu_save_store") {
                program->store_menu_game_data(registers[4]); // 801e4a28: A0 payload
            } else if (entry == "menu_names_decode") {
                auto buffer = stack(sp - 0x58 + 0x28);
                program->decode_menu_names(buffer);
            } else if (entry == "menu_load_check") {
                // 801cb6f0 to the decision at 801cb71c: S4 file buffer.
                const auto check = program->check_menu_load(registers[20]);
                return_value = check.sum;
                std::ostringstream out;
                out << "{\"sum\":" << +check.sum << ",\"stored\":" << +check.stored
                    << ",\"decision\":"
                    << (check.decision == game::menu::LoadDecision::accepted
                            ? "\"accepted\""
                            : "\"checksum_mismatch\"")
                    << '}';
                result = out.str();
            } else if (entry == "menu_load_restore") {
                program->restore_menu_game_data(registers[4], registers[5]); // 801e4d10
            } else if (entry == "menu_load_apply") {
                auto buffer = stack(sp - 0x18 - 0x30);
                for (std::uint32_t i = 0; i < 4; ++i)
                    buffer[0x10 + i] = static_cast<std::uint8_t>(registers[4] >> (8 * i));
                program->apply_menu_load(registers[4], buffer); // 801cb28c: A0 payload
            } else if (entry == "menu_load_slot_valid") {
                return_value =
                    program->menu_load_slot_valid(registers[4]) ? 1U : 0U; // 801c9bcc: A0 mode
            } else if (entry == "menu_load_find_slot") {
                return_value = program->find_menu_load_slot(registers[4]); // 801c9d34: A0 mode
            } else {
                // Resident 80033c20 (A0 text, A1 codes) or 80033b34 (A0 codes,
                // A1 text, A2 count) over their callers' stack buffers. The
                // encoder's source is known up to its destination, which
                // follows it in 801cba4c's frame.
                const auto codec = game::menu::name_codec(*program->menu);
                std::vector<std::uint8_t> output;
                if (entry == "menu_text_encode") {
                    if (registers[5] <= registers[4] || registers[5] - registers[4] > 0x100)
                        throw InputError("Encoder source must precede its destination");
                    const auto encoded = game::menu::encode_text(
                        codec, memory.range(registers[4], registers[5] - registers[4]));
                    for (const auto code : encoded.codes) {
                        output.push_back(static_cast<std::uint8_t>(code));
                        output.push_back(static_cast<std::uint8_t>(code >> 8));
                    }
                    return_value = static_cast<std::uint32_t>(encoded.result);
                } else {
                    const auto count = registers[6];
                    if (count > 0x100)
                        throw InputError("Decoder count exceeds the supported buffer");
                    output = game::menu::decode_text(codec, memory.range(registers[4], count * 2),
                                                     count);
                }
                service_output.emplace(registers[5], std::move(output));
            }
        } else if (entry == "disc_read_file") {
            // 800295d8: A0 file, A1 destination, A2 offset, A3 mode. A stream
            // read (mode 100 or 200) selects the destination as its ring; its
            // header extent is 0x24 + 8 * the count word the ring begins with.
            // The ring is attached only when that header lies in RAM; read_file
            // itself makes the original's -3 checks before using it and fails
            // explicitly if it needs a ring it does not own.
            const auto offset = registers[5] & 0x1fffffffU;
            if ((registers[7] & 0x300U) != 0 && registers[5] != 0 &&
                offset + 4U <= analysis::ram_bytes) {
                const auto count = std::uint64_t{memory.word(registers[5])};
                if (offset + 0x24U + count * 8U <= analysis::ram_bytes) {
                    const auto header =
                        memory.range(registers[5], 0x24U + static_cast<std::size_t>(count) * 8U);
                    program->resident.disc_read.ring = {registers[5],
                                                        {header.begin(), header.end()}};
                }
            }
            return_value = static_cast<std::uint32_t>(program->read_file(
                static_cast<std::int32_t>(registers[4]), registers[5], registers[6], registers[7]));
        } else if (entry == "decode_block") {
            // 80032eb4: A0 packed source, A1 output, both in RAM. The decoder
            // reads its input from the entry image as it reaches it, so
            // output written over unread input is decoded as written. V0 is
            // set in the return's delay slot, after the exit hook.
            auto ram = memory.ram;
            const auto block =
                game::field::decode_packed_in_memory(ram, 0x80000000U, registers[4], registers[5]);
            const auto size = static_cast<std::uint32_t>(memory.word(registers[4]));
            const auto begin = (registers[5] & 0x1fffffffU);
            service_output.emplace(
                registers[5],
                std::vector<std::uint8_t>(ram.begin() + begin, ram.begin() + begin + size));
            std::ostringstream out;
            out << "{\"groups\":" << block.groups
                << ",\"source_bytes_read\":" << block.source_bytes_read << '}';
            result = out.str();
        } else if (entry == "disc_read_stream") {
            // 80029eb0: A0 file, A1 ring, A2 offset (A3 unused); halfword
            // parameters five to ten on the caller's stack (SP + 0x10..0x24).
            const auto ring = registers[5];
            if (ring != 0 && (ring & 0x1fffffffU) + 4U <= analysis::ram_bytes) {
                const auto count = std::uint64_t{memory.word(ring)};
                if ((ring & 0x1fffffffU) + 0x24U + count * 8U <= analysis::ram_bytes) {
                    const auto header =
                        memory.range(ring, 0x24U + static_cast<std::size_t>(count) * 8U);
                    program->resident.disc_read.ring = {ring, {header.begin(), header.end()}};
                }
            }
            std::array<std::uint16_t, 6> parameters{};
            for (std::uint32_t i = 0; i < parameters.size(); ++i)
                parameters[i] =
                    static_cast<std::uint16_t>(memory.word(registers[29] + 0x10 + i * 4, 2));
            return_value = static_cast<std::uint32_t>(program->read_stream(
                static_cast<std::int32_t>(registers[4]), ring, registers[6], parameters));
        } else if (entry == "disc_read_files") {
            // 80029afc: A0 list, A1 offset (or seek file). The list is the
            // caller's memory, sorted in place: its entries up to the zero
            // file and that halfword become Program-owned.
            const auto list = registers[4];
            if (list != 0) {
                std::uint32_t count = 0;
                while ((list & 0x1fffffffU) + count * 8U + 2U <= analysis::ram_bytes &&
                       memory.word(list + count * 8U, 2) != 0 && count < 0x1000)
                    ++count;
                const auto extent = memory.range(list, count * 8U + 2U);
                program->resident.disc_read.list = {list, {extent.begin(), extent.end()}};
            }
            return_value = static_cast<std::uint32_t>(
                program->read_files(static_cast<std::int32_t>(registers[5])));
        } else if (entry == "sound_set_mode") {
            program->set_sound_mode(static_cast<std::int32_t>(registers[4])); // 800386c4: A0 mode
        } else if (entry == "sound_set_master") {
            // 80038c68: A0 volume, A1 frames.
            game::resident::set_master_volume(program->resident.sound, registers[4], registers[5]);
        } else if (entry == "sound_set_cd") {
            // 80038d18: A0 volume, A1 frames.
            game::resident::set_cd_volume(program->resident.sound, registers[4], registers[5]);
        } else if (entry == "sound_update_voices") {
            // 8003ebf0: A0 sequence, A1 first voice record, A2 voice count.
            game::resident::update_voices(program->resident.sound, registers[4], registers[5],
                                          registers[6]);
        } else if (entry == "battle_mode_exit") {
            // 8001b758: the outcome 800c48ea and 800d3338 are read from the
            // battle overlay the returned battle leaves in RAM.
            program->finish_battle_mode(memory.ram.at(0xc48ea), memory.ram.at(0xd3338));
        } else if (entry == "battle_mode_start") {
            program->battle_mode_start(); // 8001b6c4 up to 80070f40
        } else if (entry.starts_with("mode_dispatch")) {
            // 80019acc(0) from a resumable point ("mode_dispatch:STEP") to the
            // row call; returns the mode's function.
            static const std::map<std::string_view, game::DispatchStep> steps{
                {"start", game::DispatchStep::start},
                {"heap", game::DispatchStep::heap},
                {"wait", game::DispatchStep::wait},
                {"sync", game::DispatchStep::sync},
                {"reinit", game::DispatchStep::reinit}};
            auto from = game::DispatchStep::start;
            if (entry != "mode_dispatch") {
                const auto found =
                    steps.find(entry.substr(std::string_view("mode_dispatch:").size()));
                if (!entry.starts_with("mode_dispatch:") || found == steps.end())
                    throw InputError("Unknown mode dispatch step");
                from = found->second;
            }
            return_value = program->mode_dispatch(services, from, observer);
            program->deliver_pending_arrivals();
        } else if (entry == "set_next_mode") {
            program->set_next_mode(registers[4]); // 8001996c: A0 mode
        } else if (entry == "field_event_extended") {
            // The FE table's handler for the current actor: 800afd1c indexes
            // it and 800b0078 must be its storage.
            const auto index = memory.word(0x800afd1c);
            if (index >= program->field->actors.size() ||
                program->field->actors[index].address != memory.word(0x800b0078))
                throw InputError("The current event actor globals disagree");
            program->event_extended(index, observer);
        } else if (entry == "movie_decision") {
            // Field 800a801c or 800a7fdc, after the pad drain: 0 when the loop
            // runs another frame, 1 when it ends (at 800a80b4).
            struct Waits final : field::MovieServices {
                std::uint32_t count{};
                void wait_vertical_blank() override { ++count; }
            } waits;
            const auto step = program->movie_decision(waits);
            return_value = step == field::MovieStep::next_frame ? 0U : 1U;
            std::ostringstream out;
            out << "{\"step\":"
                << (step == field::MovieStep::skip  ? "\"skip\""
                    : step == field::MovieStep::end ? "\"end\""
                                                    : "\"next_frame\"")
                << ",\"vertical_blank_waits\":" << waits.count << '}';
            result = out.str();
        } else if (entry == "field_battle_start") {
            // 80078334: S5 records that 800afc78 was saved.
            program->field->music_saved = registers[21] != 0;
            program->field_battle_start();
        } else if (entry == "field_battle_leave") {
            program->field_battle_leave(services); // 80078abc up to 800700b0
        } else if (entry == "field_teardown") {
            program->field_teardown(services); // 800700b0
        } else if (entry == "field_battle_release") {
            program->field_battle_release(program->field->w_adb30); // 80078b04..80078b2c
        } else if (entry == "field_battle_exit") {
            // 8007954c(0), up to its call of the mode dispatcher 80019acc.
            if (!program->exit_field(0))
                throw std::runtime_error("8004f370 keeps the field; the dispatcher is not called");
        } else if (entry == "field_exit") {
            // 8007954c: A0 kind; 1 where it calls the mode dispatcher 80019acc.
            // The exit follows the field's teardown (its actors' storage is
            // already free heap memory), so only the exit mode 800b0064 is
            // imported as field state; the exit reads it and writes no field
            // state, and the export is the resident state alone.
            program->field = std::make_unique<game::FieldState>();
            program->field->exit_mode = memory.word(0x800b0064);
            try {
                return_value = program->exit_field(registers[4]) ? 1U : 0U;
            } catch (...) {
                program->field.reset();
                throw;
            }
            program->field.reset();
        } else if (entry == "sound_tick") {
            return_value = program->sound_tick(registers[2]); // 8003c028: V0 at entry
        } else if (entry == "interrupt_dispatch") {
            program->interrupt_dispatch(); // 8004b9b4
        } else if (entry == "music_stop") {
            program->stop_music(); // 8001b66c
        } else if (entry == "music_poll") {
            // 80085c90: A0 music id. Arrivals inside the call that its waits
            // did not take came before it returned.
            return_value = program->poll_music(registers[4]);
            program->deliver_pending_arrivals();
        } else if (entry == "music_chunk") {
            program->consume_music_chunk(registers[4]); // 800859dc: A0 chunk
            program->deliver_pending_arrivals();
        } else if (entry == "sequence_open") {
            return_value = program->open_sequence(registers[4]); // 80039850: A0 event data
        } else if (entry == "sequence_start") {
            // 80039a80: A0 sequence, A1 volume, A2 fade ticks.
            program->start_sequence(registers[4], registers[5], registers[6]);
        } else {
            program->field_move(observer);
        }
        status = "completed_boundary";
        reason = "Selected recovered entry returned normally";
    } catch (const BoundaryReached &) {
        status = "completed_boundary";
        reason = "Stopped at the requested completed library boundary";
        // Arrivals recorded inside a dispatcher call came before its boundary.
        if (entry.starts_with("mode_dispatch"))
            try {
                program->deliver_pending_arrivals();
            } catch (const std::exception &error) {
                status = "reconstruction_error";
                reason = error.what();
            }
    } catch (const HostBudget &error) {
        status = "host_budget_exhausted";
        reason = error.what();
    } catch (const game::MissingDependency &error) {
        status = error.recovered ? "dependency_not_connected" : "dependency_needs_recovery";
        reason = error.what();
        point = error.point;
        dependency = error.dependency;
    } catch (const field::UnrecoveredSpriteCommand &error) {
        status = "dependency_needs_recovery";
        reason = error.what();
        opcode = error.opcode;
        sprite_pc = error.command_pc;
        point.operation = "ordinary_sprite_command";
        point.machine_address = error.machine_address;
        dependency = "instruction:sprite:0x" + hex(std::span(&error.opcode, 1));
    } catch (const field::UnsupportedInstruction &error) {
        status = "dependency_needs_recovery";
        reason = error.what();
        point.event_pc = error.pc;
        opcode = error.opcode;
        dependency = "instruction:primary:0x" + hex(std::span(&error.opcode, 1));
    } catch (const field::UnsupportedExtendedInstruction &error) {
        status = "dependency_needs_recovery";
        reason = error.what();
        point.event_pc = error.pc;
        opcode = error.opcode;
        dependency = "instruction:extended:0x" + hex(std::span(&error.opcode, 1));
    } catch (const field::UnrecoveredEncounter &error) {
        status = "dependency_needs_recovery";
        reason = error.what();
        dependency = "symbol:field-control-encounter";
    } catch (const field::UnrecoveredSpriteBehavior &error) {
        status = "dependency_needs_recovery";
        reason = error.what();
        dependency = "symbol:field-return-sprite-ownership";
    } catch (const game::PlatformInputError &error) {
        // The recovered code asked for a platform input the original did not
        // read at that point: its path differs from the original's.
        status = "behavioral_divergence";
        reason = error.what();
    } catch (const InputError &error) {
        status = "invalid_input";
        reason = error.what();
    } catch (const game::ServiceUnavailable &error) {
        status = "invalid_input";
        reason = std::string("Service result not supplied: ") + error.what();
    } catch (const std::exception &error) {
        status = executing ? "reconstruction_error" : "invalid_input";
        reason = error.what();
    }
    // Committed state is exported even after a stop: it is partial, never a
    // result. Export happens before reporting so its failure changes the status.
    std::string owned;
    if (program && executing) {
        try {
            owned = owned_ranges(*program, service_output);
        } catch (const std::exception &error) {
            status = "reconstruction_error";
            reason = std::string("Export failed: ") + error.what();
            owned.clear();
        }
    }
    std::cout << "{\"entry\":" << quote(entry) << ",\"status\":" << quote(status)
              << ",\"reason\":" << quote(reason) << ",\"dependency\":" << quote(dependency)
              << ",\"operations\":" << work << ",\"entry_result\":" << result
              << ",\"location\":{\"operation\":" << quote(point.operation)
              << ",\"machine_address\":" << point.machine_address << ",\"actor\":";
    optional(std::cout, point.actor);
    std::cout << ",\"event_pc\":";
    optional(std::cout, point.event_pc);
    std::cout << ",\"sprite_bytecode_pc\":";
    optional(std::cout, point.sprite_pc ? point.sprite_pc : sprite_pc);
    std::cout << "},\"opcode\":";
    optional(std::cout, opcode);
    std::cout << ",\"executed_opcodes\":{";
    for (auto first = true; const auto &[code, count] : executed) {
        const std::array<std::uint8_t, 2> bytes{static_cast<std::uint8_t>(code >> 8U),
                                                static_cast<std::uint8_t>(code)};
        std::cout << (first ? "" : ",")
                  << quote(code > 0xff ? hex(bytes) : hex(std::span(&bytes[1], 1))) << ':' << count;
        first = false;
    }
    std::cout << "},\"return_value\":";
    optional(std::cout, return_value);
    std::cout << ",\"gte\":[";
    if (program && !owned.empty())
        std::cout << gte_controls(*program);
    std::cout << "],\"platform_unconsumed\":" << (program ? program->resident.platform.size() : 0)
              << ",\"delivered_sectors\":[";
    if (program)
        for (std::size_t i = 0; i < program->resident.drive.delivered.size(); ++i)
            std::cout << (i ? "," : "") << program->resident.drive.delivered[i];
    std::cout << "],\"owned\":[" << owned << "],\"hardware_writes\":[";
    if (program && executing)
        std::cout << hardware_writes(*program, 0);
    // Stack windows the code ran on inside heap blocks: callee frames.
    std::cout << "],\"stack_windows\":[";
    if (program)
        for (std::size_t i = 0; i < program->resident.switched_stacks.size(); ++i) {
            const auto &[address, size] = program->resident.switched_stacks[i];
            std::cout << (i ? "," : "") << '[' << address << ',' << size << ']';
        }
    std::cout << "],\"frames\":[" << frames.str() << "]}\n";
    return status == "completed_boundary" ? 0 : 1;
}
} // namespace

int main(int argc, char **argv) {
    // --batch: one call per standard input line, its arguments tab-separated
    // exactly as for a single run; each call's report is one output line.
    // Calls share nothing: each imports its own entry image.
    if (argc == 2 && std::string_view(argv[1]) == "--batch") {
        for (std::string line; std::getline(std::cin, line);) {
            std::vector<std::string> fields{argv[0]};
            for (std::size_t start = 0;;) {
                const auto tab = line.find('\t', start);
                fields.push_back(line.substr(start, tab - start));
                if (tab == std::string::npos)
                    break;
                start = tab + 1;
            }
            std::vector<char *> arguments;
            for (auto &field : fields)
                arguments.push_back(field.data());
            arguments.push_back(nullptr);
            static_cast<void>(run_case(static_cast<int>(fields.size()), arguments.data()));
            std::cout.flush();
        }
        return 0;
    }
    return run_case(argc, argv);
}
