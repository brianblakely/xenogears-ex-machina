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
void optional(std::ostream &out, const auto &value) {
    if (value)
        out << +*value;
    else
        out << "null";
}
} // namespace

int main(int argc, char **argv) {
    std::string status = "invalid_input", reason, dependency;
    std::string_view entry = argc > 1 ? argv[1] : "unknown";
    game::SourcePoint point{"prepare_start", 0, {}, {}};
    std::optional<std::uint8_t> opcode;
    std::map<std::uint16_t, std::size_t> executed; // Event opcodes entered, fe00|x extended.
    std::optional<std::uint32_t> sprite_pc;
    std::uint32_t work = 0;
    bool executing = false;
    std::optional<game::Program> program;
    std::optional<std::uint32_t> return_value;
    std::string result = "null";
    try {
        if (argc != 13)
            throw InputError("Usage: xem-memory-runner ENTRY BUDGET STOP RAM SCRATCHPAD "
                             "FIELD_SOURCE OVERLAY RESOURCES GTE REGISTERS IO SERVICES");
        // Resident entries need no loaded field; FIELD_SOURCE, OVERLAY and
        // RESOURCES are unused. OVERLAY is the decoded field overlay image.
        const bool resident_entry = entry == "heap_allocate" || entry == "heap_release" ||
                                    entry == "music_stop" || entry == "disc_read_file";
        const bool battle_entry = entry == "battle_commit" || entry == "battle_apply" ||
                                  entry == "battle_alive" || entry == "battle_rewards" ||
                                  entry == "battle_reward_totals" || entry == "battle_drops" ||
                                  entry == "battle_atb" || entry == "battle_reload" ||
                                  entry == "battle_ai";
        if (entry != "field_event_pass" && entry != "field_update" && entry != "field_move" &&
            entry != "field_checkpoints" && !entry.starts_with("field_frame") && !resident_entry &&
            !battle_entry)
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
        } else if (battle_entry) {
            program = analysis::import_battle(memory);
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
        // Platform results for a field frame, one "name value..." per line
        // (hexadecimal), in the order the original consumed them.
        game::FrameServices services;
        {
            std::ifstream lines(argv[12]);
            if (!lines)
                throw InputError("Cannot open services");
            for (std::string line; std::getline(lines, line);) {
                std::istringstream fields(line);
                std::string name;
                std::uint32_t first = 0, second = 0;
                if (!(fields >> name >> std::hex >> first))
                    throw InputError("Malformed service line");
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
                else
                    throw InputError("Unknown service result " + name);
            }
        }
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
        } else if (entry == "battle_atb") {
            program->tick_battle_timers(); // 8007171c
        } else if (entry == "battle_reload") {
            program->reload_battle_timer(); // 800718bc
        } else if (entry == "battle_rewards") {
            program->grant_battle_rewards(); // 801e2794
        } else if (entry == "battle_reward_totals") {
            program->total_battle_rewards(); // 801e2280 up to 801e23d4
        } else if (entry == "battle_drops") {
            // 801e1444: A0 ids, A1 counts, A2 categories.
            program->add_battle_drops(registers[4], registers[5], registers[6]);
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
        } else if (entry == "music_stop") {
            program->stop_music(); // 8001b66c
        } else {
            program->field_move(observer);
        }
        status = "completed_boundary";
        reason = "Selected recovered entry returned normally";
    } catch (const BoundaryReached &) {
        status = "completed_boundary";
        reason = "Stopped at the requested completed library boundary";
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
    std::ostringstream owned;
    if (program && executing) {
        analysis::OriginalMemory image;
        image.ram.assign(analysis::ram_bytes, 0);
        try {
            const auto ranges = program->field    ? analysis::export_field(*program, image)
                                : program->battle ? analysis::export_battle(*program, image)
                                                  : analysis::export_resident(*program, image);
            for (std::size_t i = 0; i < ranges.size(); ++i) {
                const auto &range = ranges[i];
                owned << (i ? "," : "") << "{\"name\":" << quote(range.name)
                      << ",\"address\":" << range.address
                      << ",\"hex\":" << quote(hex(image.range(range.address, range.size))) << '}';
            }
        } catch (const std::exception &error) {
            status = "reconstruction_error";
            reason = std::string("Export failed: ") + error.what();
            owned.str("");
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
    // GTE control registers 0-30 at exit (FLAG is transient).
    if (program && !owned.str().empty())
        for (std::uint32_t i = 0; i < 31; ++i)
            std::cout << (i ? "," : "") << program->resident.gte.control(i);
    std::cout << "],\"owned\":[" << owned.str() << "]}\n";
    return status == "completed_boundary" ? 0 : 1;
}
