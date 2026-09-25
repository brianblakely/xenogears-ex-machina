#include "xem/reconstruction/field_events.hpp"
#include "xem/reconstruction/field_media.hpp"

#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace field = xem::reconstruction::field;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Error, typename Operation>
void rejects(Operation operation, const char *message) {
    try {
        operation();
    } catch (const Error &) {
        return;
    }
    throw std::runtime_error(message);
}

struct Scenario {
    std::vector<std::uint8_t> code{0xfe, 0xa2, 0x35, 0, 0, 9, 0, 0x40, 0};
    field::EventActor actor;
    field::EventVariables variables;
    field::EventContext context;
    field::MusicLoadState music;
    Scenario() {
        music.gate = field::music_pending;
        context.current_actor = &actor;
        context.variables = &variables;
        context.program = {code, {}};
        context.control.budget_mode = 1;
        context.control.gate_values = {1, 1, 1};
        actor.flags = 0xa1b2c3d4;
        actor.layer_flags = 0xf0e1d2c3;
        for (std::uint16_t i = 0; i < 8; ++i)
            actor.slots[i] = {static_cast<std::uint16_t>(100U + i),
                              static_cast<std::uint8_t>(i + 1U), static_cast<std::uint8_t>(i),
                              0xa5c00000U | i};
    }
    void extended(field::EventContext &current, std::uint8_t opcode) {
        field::execute_music_extended_event(current, opcode, music);
    }
    void primary(field::EventContext &current, std::uint8_t opcode) {
        if (opcode == 0xfe)
            field::run_extended_event(current, [&](auto &ctx, auto op) { extended(ctx, op); });
        else
            field::execute_core_event(current, opcode);
    }
    field::BatchResult batch() {
        context.program = {code, {}};
        return field::run_event_batch(context, 8, [&](auto &ctx, auto op) { primary(ctx, op); });
    }
};

void pending_ready_and_shared_batch() {
    Scenario s;
    const auto before = s.actor;
    const auto pending = s.batch();
    check(pending.dispatched == 1 && pending.reason == field::BatchExit::handler_break &&
              s.actor.pc == 0 && s.context.control.break_requested == 1 &&
              s.context.control.budget_mode == 1 && s.variables.read(0) == 0,
          "Pending music wait retries FE and breaks the real batch without running continuation");
    check(s.actor.flags == before.flags && s.actor.layer_flags == before.layer_flags &&
              s.actor.selected_slot == before.selected_slot,
          "Music wait must preserve actor flags and selected slot");
    for (std::size_t i = 0; i < 8; ++i)
        check(s.actor.slots[i].resume_pc == before.slots[i].resume_pc &&
                  s.actor.slots[i].countdown == before.slots[i].countdown &&
                  s.actor.slots[i].event_tag == before.slots[i].event_tag &&
                  s.actor.slots[i].control_bits == before.slots[i].control_bits,
              "Prefix/music wait must preserve every event slot field");
    s.music.gate = 0;
    const auto ready = s.batch();
    check(ready.dispatched == 1 && ready.reason == field::BatchExit::handler_break &&
              s.actor.pc == 2 && s.variables.read(0) == 0,
          "Ready music wait advances beyond A2 but still breaks this batch");
    const auto resumed = s.batch();
    check(resumed.dispatched == 2 && resumed.reason == field::BatchExit::handler_break &&
              s.variables.read(0) == 9 && s.actor.pc == 8,
          "Next batch executes the actual assignment/end continuation");
    for (const auto status : {0U, 1U, 2U, 0x80000000U, 0xfffffffeU}) {
        Scenario nonpending;
        nonpending.music.gate = status;
        (void)nonpending.batch();
        check(nonpending.actor.pc == 2, "Only exactly ffffffff may retry the prefix");
    }
}

void initialization_safeguard_and_wrap() {
    Scenario s;
    s.context.control.budget_mode = 0;
    const auto pending = s.batch();
    check(pending.reason == field::BatchExit::safeguard && pending.dispatched == 1025 &&
              pending.diagnostic_requested && s.actor.pc == 0 &&
              s.context.control.batch_limit == 65535 && s.context.control.budget_mode == 0 &&
              s.context.control.break_requested == 1,
          "Pending wait must preserve initialization mode and reach its real 1025-dispatch "
          "safeguard");
    s.context.control.diagnostic_suppression = 7;
    check(!s.batch().diagnostic_requested, "Diagnostic suppression survives pending wait loops");
    s.context.control.budget_mode = 1;
    s.context.control.post_initialization = 1;
    s.context.control.gate_values[1] = 0;
    check(s.batch().reason == field::BatchExit::control_gate,
          "Shared gate evaluation precedes music handler's break request");

    Scenario wrapped;
    wrapped.code.assign(65536, 0);
    wrapped.code[65535] = 0xfe;
    wrapped.code[0] = 0xa2;
    wrapped.actor.pc = 65535;
    (void)wrapped.batch();
    check(wrapped.actor.pc == 65535, "Prefix increment and pending decrement both wrap as u16");
    wrapped.music.gate = 0;
    (void)wrapped.batch();
    check(wrapped.actor.pc == 1, "Ready wait after a wrapping FE prefix advances to PC one");
    wrapped.actor.pc = 65535;
    field::wait_music_load_extended(wrapped.context, 0);
    check(wrapped.actor.pc == 0, "The extended handler's ready increment also wraps as u16");
}

void real_dispatch_and_failures() {
    Scenario s;
    s.code[1] = 0xa3;
    bool unknown = false;
    try {
        (void)s.batch();
    } catch (const field::UnsupportedExtendedInstruction &error) {
        unknown = error.pc == 1 && error.opcode == 0xa3;
    }
    check(unknown && s.actor.pc == 1,
          "Unknown extended opcode must retain its namespace, advanced PC and identity");
    s.actor.pc = 0;
    s.code.resize(1);
    s.context.program = {s.code, {}};
    rejects<field::EventError>([&] { (void)s.batch(); }, "Truncated extended operand must fail");
    check(s.actor.pc == 1, "A bounded extended lookup error follows the original PC increment");
    s.actor.pc = 0;
    rejects<field::EventError>([&] { field::run_extended_event(s.context, {}); },
                               "Missing extended dispatcher must not silently succeed");
    s.actor.pc = 0;
    s.code[0] = 0xfd;
    rejects<field::EventError>(
        [&] {
            field::run_extended_event(s.context,
                                      [&](auto &ctx, auto opcode) { s.extended(ctx, opcode); });
        },
        "A different primary opcode must not enter extended dispatch");
    check(s.actor.pc == 0, "Wrong prefix fails before mutating PC");
    s.context.current_actor = nullptr;
    rejects<field::EventError>([&] { field::wait_music_load_extended(s.context, 0); },
                               "Music wait requires an actor");

    // This authored replacement dispatcher proves shared actor ownership follows
    // the real called handler; it is not original execution evidence.
    Scenario replace;
    field::EventActor other;
    other.pc = 19;
    field::run_extended_event(replace.context, [&](auto &ctx, auto opcode) {
        check(opcode == 0xa2 && ctx.current_actor->pc == 1, "Dispatcher sees the extended PC");
        ctx.current_actor = &other;
    });
    check(replace.actor.pc == 1 && replace.context.current_actor == &other && other.pc == 19,
          "The wrapper must not restore an actor pointer replaced by its handler");
}

// Authored call doubles. These check control flow and resource ownership, never
// stand in for original evidence of the unrecovered streaming/audio routines.
struct MusicCalls final : field::MusicCalls {
    std::vector<std::string> operations;
    // The callback at the address the stream stored (800859dc).
    std::function<void(field::MusicResource)> consume;
    std::vector<std::uint32_t> stream_results;
    std::size_t stream_index{};
    std::uint32_t busy{};
    std::function<void()> on_disc_query;
    field::MusicResource created{123};
    void record(const std::string &name, std::initializer_list<std::uint32_t> args = {}) {
        std::string line = name;
        for (auto value : args)
            line += " " + std::to_string(value);
        operations.push_back(line);
    }
    void consume_stream_chunk(std::uint32_t consumer, field::MusicResource chunk) override {
        if (consumer != 0x800859dc || !consume)
            throw field::EventError("No chunk callback at the stored address");
        consume(chunk);
    }
    field::MusicResource next_stream_chunk() override {
        record("stream");
        if (stream_index >= stream_results.size())
            throw field::EventError("Missing authored streaming result");
        return stream_results[stream_index++];
    }
    field::MusicResource allocate_stream_buffer(std::uint32_t blocks, std::uint32_t mode) override {
        record("stream_allocate", {blocks, mode});
        return 11;
    }
    void release_stream_chunk(field::MusicResource chunk) override {
        record("release_chunk", {chunk});
    }
    field::MusicResource start_wave_transfer(field::MusicResource staging, std::uint32_t bytes,
                                             std::uint32_t mode) override {
        record("wave_start", {staging, bytes, mode});
        return 66;
    }
    void continue_wave_transfer(field::MusicResource staging, std::uint32_t bytes) override {
        record("wave_continue", {staging, bytes});
    }
    void wait_audio_service(std::uint32_t flags) override { record("wait", {flags}); }
    void release_buffer(field::MusicResource buffer) override { record("free", {buffer}); }
    void select_directory(std::uint32_t index, std::uint32_t offset) override {
        record("directory", {index, offset});
    }
    std::uint32_t file_size(std::uint32_t file) override {
        record("size", {file});
        return 2048;
    }
    field::MusicResource allocate_buffer(std::uint32_t bytes, std::uint32_t mode) override {
        record("allocate", {bytes, mode});
        return 33;
    }
    void read_file(std::uint32_t file, field::MusicResource destination, std::uint32_t offset,
                   std::uint32_t mode) override {
        record("read", {file, destination, offset, mode});
    }
    std::uint32_t disc_busy() override {
        record("busy");
        if (on_disc_query)
            on_disc_query();
        return busy;
    }
    field::MusicResource load_shared_wave(field::MusicResource input, std::uint32_t mode) override {
        record("shared", {input, mode});
        return 44;
    }
    field::MusicResource create_sequence(field::MusicResource input) override {
        record("create", {input});
        return created;
    }
    void start_sequence(field::MusicResource sequence, std::uint32_t volume,
                        std::uint32_t parameter) override {
        record("start", {sequence, volume, parameter});
    }
    void configure_sequence(field::MusicResource sequence, std::uint32_t first,
                            std::uint32_t second) override {
        record("configure", {sequence, first, second});
    }
    void resume_sequence(field::MusicResource sequence, std::uint32_t volume,
                         std::uint32_t parameter) override {
        record("resume", {sequence, volume, parameter});
    }
};

void wave_and_shared_resource_lifetime() {
    field::BattleRequestState request;
    const field::MusicSelection selection{7, 3, 1};
    field::MusicLoadState state;
    state.wave_pending = 1;
    state.deferred_sequence_read = 1;
    state.loaded_sequence = field::music_pending;
    state.wave_staging = 22;
    MusicCalls calls;
    calls.stream_results = {1, 2, 3, 4, 5};
    state.stream.descriptor = 11;
    request.menu_gate = 1;
    state.stream.consumer = 0x800859dc;
    calls.consume = [&](field::MusicResource chunk) { calls.record("consume", {chunk}); };
    check(field::poll_music_load(state, request, selection, calls) == field::music_pending &&
              calls.stream_index == 5 && state.wave_pending == 1 &&
              state.deferred_sequence_read == 1,
          "Exactly five unfinished stream steps yield before sequence loading");
    calls.operations.clear();
    calls.stream_results.push_back(0);
    check(field::poll_music_load(state, request, selection, calls) == field::music_pending,
          "Wave completion and deferred read still yield this poll");
    check(calls.operations == std::vector<std::string>{"stream", "busy", "free 11", "wait 16",
                                                       "free 22", "directory 28 0",
                                                       "read 34 2147886664 0 128", "directory 4 0"},
          "Wave completion waits before freeing, then issues the selected sequence read");
    check(state.wave_pending == 0 && state.wave_loaded_now == 1 && state.loaded_wave_bank == 3 &&
              state.sequence_pending == 1 && state.deferred_sequence_read == 0 &&
              request.menu_gate == 0,
          "Wave and sequence flags commit in the recovered order");

    state = {};
    state.shared_release_flag = 0xffff;
    state.shared_release_started = 7;
    calls = {};
    const field::MusicSelection shared{7, 255, 0};
    check(field::poll_music_load(state, request, shared, calls) == field::music_pending &&
              state.shared_wave_state == 128 && state.shared_staging == 33,
          "Shared wave startup publishes its allocation and pending state");
    check(calls.operations == std::vector<std::string>{"directory 28 0", "size 3",
                                                       "allocate 2048 1", "read 3 33 0 128",
                                                       "directory 4 0"},
          "Shared startup preserves directory, allocation and read arguments");
    calls.operations.clear();
    calls.busy = 2;
    check(field::poll_music_load(state, request, shared, calls) == field::music_pending &&
              calls.operations == std::vector<std::string>{"busy"} &&
              state.shared_release_flag == 0xffff && state.shared_release_started == 7,
          "Any nonzero disc busy result preserves pending shared ownership");
    calls.operations.clear();
    calls.busy = 0;
    check(field::poll_music_load(state, request, shared, calls) == 0 &&
              state.shared_wave_state == 1 && state.shared_wave == 44 &&
              state.active_shared_wave == 44 && state.shared_release_flag == 0 &&
              state.shared_release_started == 0,
          "Shared completion records both owners and clears release controls");
    check(calls.operations ==
              std::vector<std::string>{"busy", "shared 33 0", "wait 16", "free 33", "busy"},
          "Shared finish precedes the separate sequence disc query");
}

void stream_start_and_chunk_ownership() {
    field::MusicStreamState state;
    field::BattleRequestState request;
    MusicCalls calls;
    unsigned consumed = 0;
    calls.consume = [&](field::MusicResource chunk) {
        check(chunk == 17, "Consumer receives the original A0 chunk");
        ++consumed;
    };
    field::start_music_stream(state, request, 31, 2, 0x800859dc, calls);
    check(request.menu_gate == 1 && state.descriptor == 11 && state.consumer == 0x800859dc &&
              calls.operations ==
                  std::vector<std::string>{"stream_allocate 8 2", "read 31 11 0 256"},
          "Stream start allocates its descriptor and reads before installing the consumer");
    field::EventActor actor;
    const std::array<std::uint8_t, 3> request_code{0x71, 0, 0x80};
    field::EventContext context;
    context.current_actor = &actor;
    context.program.bytecode = request_code;
    context.control.gate_values = {1, 1, 1};
    request.field_active = 1;
    check(field::execute_battle_request(context, request, 0, 0).retry ==
              field::BattleRequestRetry::menu_gate_set,
          "Stream startup blocks the real battle request through its shared 800adb2c owner");
    calls.operations.clear();
    calls.stream_results = {17, 0, 0};
    check(field::poll_music_stream(state, request, calls) == 0 && consumed == 1 &&
              state.next_chunk == 17 && calls.operations == std::vector<std::string>{"stream"},
          "A ready chunk invokes the actual registered consumer without disc polling");
    calls.busy = 2;
    check(field::poll_music_stream(state, request, calls) == 0 && request.menu_gate == 1,
          "An empty stream remains active while disc loading is busy");
    calls.busy = 0;
    check(field::poll_music_stream(state, request, calls) == field::music_pending &&
              request.menu_gate == 0 && state.descriptor == 11 && state.consumer == 0x800859dc,
          "Stream completion frees descriptor but retains the original stored token and callback");
    check(field::execute_battle_request(context, request, 0, 0).accepted() && actor.pc == 3,
          "Stream completion releases that same battle gate without copying state");
    state.consumer = 0x80012345;
    calls.stream_results.push_back(19);
    rejects<field::EventError>([&] { (void)field::poll_music_stream(state, request, calls); },
                               "A chunk callback at another address must not silently succeed");
    check(state.next_chunk == 19, "A failed callback preserves the already-issued stream query");
    state.consumer = 0x800859dc;
    request.menu_gate = 1;
    calls.stream_results.push_back(0);
    calls.on_disc_query = [&] { state.next_chunk = 77; };
    calls.operations.clear();
    check(field::poll_music_stream(state, request, calls) == 0 && request.menu_gate == 1 &&
              state.next_chunk == 77 &&
              calls.operations == std::vector<std::string>{"stream", "busy"},
          "The post-disc shared chunk recheck prevents premature descriptor release");
}

void wave_chunk_staging_and_transfer_order() {
    field::MusicLoadState state;
    state.wave_staging = 22;
    field::BattleRequestState request;
    MusicCalls calls;
    std::array<std::array<std::uint8_t, 2048>, 6> chunks{};
    for (std::size_t i = 0; i < chunks.size(); ++i)
        chunks[i].fill(static_cast<std::uint8_t>(i + 1));
    std::array<std::uint8_t, 8192> staging{};
    calls.consume = [&](field::MusicResource chunk) {
        check(chunk >= 101 && chunk <= 106, "Expected authored chunk token");
        field::consume_music_wave_chunk(state, chunk, chunks[chunk - 101], staging, calls);
    };
    field::start_music_stream(state.stream, request, 31, 1, 0x800859dc, calls);
    calls.operations.clear();
    calls.stream_results = {101, 102, 103, 104, 105, 106, 0};
    check(field::finish_music_wave_chunks(state.stream, request, calls) == field::music_pending &&
              state.wave_chunk_index == 4 && state.wave_transfer == 66 && request.menu_gate == 1,
          "First five steps fill initial staging, start transfer and submit the fifth chunk");
    check(calls.operations ==
              std::vector<std::string>{"stream", "release_chunk 101", "stream", "release_chunk 102",
                                       "stream", "release_chunk 103", "stream", "release_chunk 104",
                                       "wave_start 22 8192 0", "stream", "wait 16",
                                       "wave_continue 22 2048", "release_chunk 105"},
          "Initial transfer follows the fourth release; later transfer waits before copy/release");
    check(field::finish_music_wave_chunks(state.stream, request, calls) == 0 &&
              request.menu_gate == 0,
          "Following steps submit the remaining chunk before releasing the stream descriptor");
    for (std::size_t i = 0; i < staging.size(); ++i)
        check(staging[i] == (i < 2048 ? 6 : i / 2048 + 1),
              "Continuation overwrites only the leading chunk of the four-chunk staging block");
    for (const auto index : {-1, 5}) {
        state.wave_chunk_index = index;
        calls.operations.clear();
        const auto before = staging;
        field::consume_music_wave_chunk(state, 107, chunks[0], staging, calls);
        check(staging == before && calls.operations.empty() && state.wave_chunk_index == index,
              "Source switch leaves negative and greater-than-four chunk indices untouched");
    }
    // Four original word loads occur before four stores; forward overlap is
    // therefore a sequence of 16-byte copies, not one whole-buffer memmove.
    for (std::size_t i = 0; i < staging.size(); ++i)
        staging[i] = static_cast<std::uint8_t>(i);
    state.wave_chunk_index = 1;
    const std::span<const std::uint8_t, 2048> overlapping(staging.data() + 2032, 2048);
    field::consume_music_wave_chunk(state, 108, overlapping, staging, calls);
    for (std::size_t i = 2048; i < 4096; ++i)
        check(staging[i] == 240 + i % 16,
              "Wave staging preserves original overlapping word groups");
}

void poll_gate_and_event_connection() {
    field::BattleRequestState request;
    Scenario scenario;
    auto &state = scenario.music;
    state.loaded_sequence = field::music_pending;
    state.deferred_sequence_read = 1;
    state.start_parameter = field::music_pending;
    const field::MusicSelection selection{7, 255, 1};
    MusicCalls calls;
    field::update_music_load_gate(state, request, selection, calls);
    check(state.gate == field::music_pending && state.sequence_pending == 1,
          "Deferred read commits pending to the authoritative music gate");
    (void)scenario.batch();
    check(scenario.actor.pc == 0, "Real FE/A2 sees the connected pending load gate");
    calls.operations.clear();
    calls.busy = 1;
    field::update_music_load_gate(state, request, selection, calls);
    check(calls.operations == std::vector<std::string>{"busy"} &&
              state.gate == field::music_pending && state.sequence_pending == 1,
          "Busy disc prevents sequence creation and completion");
    calls.operations.clear();
    calls.busy = 0;
    field::update_music_load_gate(state, request, selection, calls);
    check(calls.operations ==
                  std::vector<std::string>{"busy", "create 2147886664", "start 123 127 0"} &&
              state.current_sequence == 123 && state.sequence_pending == 0 &&
              state.sequence_active == 1 && state.loaded_sequence == 7 && state.completed == 1 &&
              state.start_parameter == field::music_pending && state.gate == 0,
          "Disc completion creates and starts the sequence before committing ready");
    (void)scenario.batch();
    check(scenario.actor.pc == 2 && scenario.variables.read(0) == 0,
          "Connected ready wait advances but yields before its continuation");
    (void)scenario.batch();
    check(scenario.variables.read(0) == 9, "The following batch executes real script continuation");
    calls.operations.clear();
    field::update_music_load_gate(state, request, selection, calls);
    check(calls.operations.empty(), "A completed gate does not poll again");
}

void exact_flags_reuse_and_failures() {
    field::BattleRequestState request;
    field::MusicLoadState state;
    state.gate = 17;
    state.sequence_pending = 1;
    state.reuse_sequence = 2;
    state.cached_sequence = 91;
    MusicCalls calls;
    const field::MusicSelection selection{7, 255, 1};
    check(field::poll_music_load(state, request, selection, calls) == 0 &&
              calls.operations == std::vector<std::string>{"busy", "resume 91 127 240"} &&
              state.current_sequence == 91 && state.cached_sequence == 0 &&
              state.reuse_sequence == 0 && state.gate == 17,
          "Nonzero reuse transfers the cached owner and poll itself preserves gate");
    state.sequence_pending = 1;
    state.start_parameter = 0;
    calls.operations.clear();
    calls.created = 0;
    (void)field::poll_music_load(state, request, selection, calls);
    check(calls.operations == std::vector<std::string>{"busy", "create 2147886664", "start 0 0 0",
                                                       "configure 0 0 0"},
          "Alternate start configures even a zero creation result as the original does");
    state.wave_pending = 2;
    state.sequence_pending = 2;
    state.deferred_sequence_read = 2;
    calls.operations.clear();
    (void)field::poll_music_load(state, request, selection, calls);
    check(calls.operations == std::vector<std::string>{"busy"} && state.wave_pending == 2 &&
              state.sequence_pending == 2 && state.deferred_sequence_read == 2,
          "Only exactly-one pending flags execute their branches");
    state.deferred_sequence_read = 1;
    state.loaded_sequence = 7;
    calls.operations.clear();
    check(field::poll_music_load(state, request, selection, calls) == field::music_pending &&
              state.deferred_sequence_read == 0 && calls.operations.empty(),
          "Already-loaded deferred selection still yields without querying disc");
    rejects<field::EventError>(
        [&] { (void)field::poll_music_load(state, request, {255, 0, 0}, calls); },
        "Stop sentinel cannot address an unqualified music table row");
    Scenario scenario;
    rejects<field::EventError>(
        [&] { field::execute_music_extended_event(scenario.context, 0xa2, state); },
        "Extended music dispatcher rejects a working-PC opcode mismatch");
    scenario.context.current_actor = nullptr;
    rejects<field::EventError>(
        [&] { field::execute_music_extended_event(scenario.context, 0xa2, state); },
        "Extended music dispatcher requires shared actor ownership");
}
} // namespace

int main() {
    try {
        pending_ready_and_shared_batch();
        initialization_safeguard_and_wrap();
        real_dispatch_and_failures();
        wave_and_shared_resource_lifetime();
        stream_start_and_chunk_ownership();
        wave_chunk_staging_and_transfer_order();
        poll_gate_and_event_connection();
        exact_flags_reuse_and_failures();
        std::cout << "Music poll, gate and extended dispatch: connected source boundaries passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
