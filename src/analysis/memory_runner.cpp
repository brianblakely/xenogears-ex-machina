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
        if (argc != 12)
            throw InputError("Usage: xem-memory-runner ENTRY BUDGET STOP RAM SCRATCHPAD "
                             "FIELD_SOURCE OVERLAY RESOURCES GTE REGISTERS IO");
        // Resident entries need no loaded field; FIELD_SOURCE, OVERLAY and
        // RESOURCES are unused. OVERLAY is the decoded field overlay image.
        const bool resident_entry = entry == "heap_allocate" || entry == "heap_release" ||
                                    entry == "music_stop" || entry == "disc_read_file" ||
                                    entry == "sound_set_mode" || entry == "sound_set_master" ||
                                    entry == "sound_set_cd" || entry == "sound_update_voices" ||
                                    entry == "set_next_mode" || entry == "field_exit";
        // Field entries beyond the update: one extended event handler, the
        // movie loop's decision.
        const bool field_entry = entry == "field_event_extended" || entry == "movie_decision";
        const bool battle_entry = entry == "battle_commit" || entry == "battle_apply" ||
                                  entry == "battle_alive" || entry == "battle_rewards" ||
                                  entry == "battle_reward_totals" || entry == "battle_drops" ||
                                  entry == "battle_atb" || entry == "battle_reload" ||
                                  entry == "battle_ai";
        if (entry != "field_event_pass" && entry != "field_update" && entry != "field_move" &&
            entry != "field_checkpoints" && !resident_entry && !battle_entry && !field_entry)
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
        // The 32 GTE control registers at entry.
        const auto gte = hex_words(argv[9], 32, "GTE must be 32 comma-separated hex words");
        std::array<std::uint32_t, 8> loaded{};
        std::ranges::copy_n(gte.begin(), 8, loaded.begin());
        program->resident.gte = game::field::gte_from_words(loaded);
        // The hardware I/O page observed at entry (1f801000..1f801fff).
        const auto io = read_file(argv[11], program->resident.io.size());
        if (io.size() != program->resident.io.size())
            throw InputError("IO page must contain exactly 4 KiB");
        std::ranges::copy(io, program->resident.io.begin());
        program->resident.gte_screen = {static_cast<std::int32_t>(gte[24]),
                                        static_cast<std::int32_t>(gte[25]),
                                        static_cast<std::uint16_t>(gte[26])};
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
    if (program && !owned.str().empty()) {
        const auto gte = game::field::gte_words(program->resident.gte);
        const auto &screen = program->resident.gte_screen;
        for (std::size_t i = 0; i < gte.size(); ++i)
            std::cout << (i ? "," : "") << gte[i];
        std::cout << ',' << static_cast<std::uint32_t>(screen.offset_x) << ','
                  << static_cast<std::uint32_t>(screen.offset_y) << ','
                  << static_cast<std::uint32_t>(
                         static_cast<std::int32_t>(static_cast<std::int16_t>(screen.h)));
    }
    std::cout << "],\"owned\":[" << owned.str() << "],\"hardware_writes\":[";
    if (program && executing)
        for (std::size_t i = 0; i < program->resident.hardware_writes.size(); ++i) {
            const auto &write = program->resident.hardware_writes[i];
            std::cout << (i ? "," : "") << "{\"address\":" << write.address
                      << ",\"value\":" << write.value << ",\"width\":" << write.width << '}';
        }
    std::cout << "]}\n";
    return status == "completed_boundary" ? 0 : 1;
}
