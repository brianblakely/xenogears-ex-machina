#include "recorded_card.hpp"

#include <algorithm>
#include <limits>

namespace xem::analysis {
namespace game = reconstruction;

void RecordedCardBios::add(const std::string &kind, std::uint32_t value,
                           std::vector<std::uint8_t> bytes) {
    static constexpr std::array kinds{"wait",   "open_event", "open", "read",   "write",
                                      "format", "first",      "next", "rename", "kanji",
                                      "rom",    "info",       "init"};
    if (std::ranges::find(kinds, kind) == kinds.end())
        throw InputError("Unknown recorded card result kind " + kind);
    if (kind == "wait" && value > 3)
        throw InputError("A recorded card wait result is not an event index");
    if (kind == "init" && bytes.size() != std::tuple_size_v<CardPatch>)
        throw InputError("A recorded card patch is not 20 bytes");
    if (kind == "first" || kind == "next") {
        if (bytes.size() != (value == 0 ? 0 : std::tuple_size_v<DirectoryEntry>))
            throw InputError("A recorded directory result has the wrong byte count");
    } else if (kind == "read") {
        if (bytes.size() != (value < 0x80000000U ? value : 0))
            throw InputError("A recorded card read has the wrong byte count");
    } else if (kind != "init" && !bytes.empty())
        throw InputError("Unexpected bytes in a recorded scalar card result");
    if (kind == "rom" && value > 0xffffU)
        throw InputError("A recorded ROM halfword exceeds 16 bits");
    results_[kind].push_back({value, std::move(bytes)});
}

std::size_t RecordedCardBios::unconsumed() const {
    std::size_t count = 0;
    for (const auto &[kind, queue] : results_)
        count += queue.size();
    return count;
}

RecordedCardBios::Result RecordedCardBios::take(const std::string &kind) {
    auto &queue = results_[kind];
    if (queue.empty())
        throw game::ServiceUnavailable("card BIOS " + kind + " result");
    auto result = std::move(queue.front());
    queue.pop_front();
    return result;
}

RecordedCardBios::Request &RecordedCardBios::observe(std::string kind,
                                                     std::vector<std::uint32_t> arguments,
                                                     std::vector<std::string> names) {
    requests_.push_back(
        {std::move(kind), std::move(arguments), std::move(names), {}, {}, "unavailable", {}, {}});
    return requests_.back();
}

std::uint32_t RecordedCardBios::recorded(Request &request, const std::string &kind) {
    const auto result = take(kind);
    request.result = result.value;
    request.result_source = "recorded";
    return result.value;
}

std::uint32_t RecordedCardBios::constant(Request &request, std::uint32_t value) {
    request.result = value;
    request.result_source = "adapter_constant";
    return value;
}

void RecordedCardBios::bu_init() { observe("bu_init").result_source = "void"; }
std::uint32_t RecordedCardBios::open_event(std::uint32_t event_class, std::uint32_t spec,
                                           std::uint32_t mode, std::uint32_t handler) {
    return recorded(observe("open_event", {event_class, spec, mode, handler}), "open_event");
}
std::uint32_t RecordedCardBios::close_event(std::uint32_t event) {
    return constant(observe("close_event", {event}), 1);
}
std::uint32_t RecordedCardBios::test_event(std::uint32_t event) {
    auto &request = observe("test_event", {event});
    auto &queue = results_["wait"];
    if (queue.empty())
        throw game::ServiceUnavailable("card event wait result");
    static constexpr std::array<std::uint32_t, 4> order{3, 1, 0, 2};
    const auto fired = queue.front().value;
    const auto polled = order[poll_++ % 4];
    request.result_source = "derived_wait";
    request.result = polled == fired ? 1 : 0;
    if (polled != fired)
        return 0;
    poll_ = 0;
    queue.pop_front();
    // An aggregate observation corresponding to the original card-wait hook;
    // individual poll count/results are not observed by those old captures.
    auto &wait = observe("wait", {fired});
    wait.result = fired;
    wait.result_source = "derived_wait";
    return 1;
}
std::uint32_t RecordedCardBios::enable_event(std::uint32_t event) {
    return constant(observe("enable_event", {event}), 1);
}
void RecordedCardBios::undeliver_event(std::uint32_t event_class, std::uint32_t spec) {
    observe("undeliver_event", {event_class, spec}).result_source = "void";
}
std::uint32_t RecordedCardBios::enter_critical_section() {
    return constant(observe("enter_critical_section"), 1);
}
void RecordedCardBios::exit_critical_section() {
    observe("exit_critical_section").result_source = "void";
}

std::uint32_t RecordedCardBios::open(std::uint32_t name_address, std::string_view name,
                                     std::uint32_t mode) {
    auto &request = observe("open", {name_address, mode}, {std::string(name)});
    const auto result = recorded(request, "open");
    if (result < 0x80000000U)
        offsets_[result] = (mode & 0x8000U) ? std::nullopt : std::optional<std::uint32_t>{0};
    return result;
}

void RecordedCardBios::transfer(Request &request, std::uint32_t fd, std::uint32_t count,
                                std::uint32_t result) {
    const auto found = offsets_.find(fd);
    if (found == offsets_.end())
        return;
    request.offset = found->second;
    if (result >= 0x80000000U || !found->second)
        return;
    if (result > count || result > std::numeric_limits<std::uint32_t>::max() - *found->second) {
        found->second.reset();
        return;
    }
    *found->second += result;
}

RecordedCardBios::ReadResult RecordedCardBios::read(std::uint32_t fd, std::uint32_t buffer,
                                                    std::uint32_t count) {
    auto &request = observe("read", {fd, buffer, count});
    auto result = take("read");
    request.result = result.value;
    request.result_source = "recorded";
    request.response = result.bytes;
    if (result.bytes.size() > count)
        throw InputError("A recorded card read exceeds the native request count");
    transfer(request, fd, count, result.value);
    return {result.value, std::move(result.bytes)};
}
std::uint32_t RecordedCardBios::write(std::uint32_t fd, std::uint32_t buffer,
                                      std::span<const std::uint8_t> bytes) {
    auto &request = observe("write", {fd, buffer, static_cast<std::uint32_t>(bytes.size())});
    request.payload.assign(bytes.begin(), bytes.end());
    const auto result = recorded(request, "write");
    transfer(request, fd, static_cast<std::uint32_t>(bytes.size()), result);
    return result;
}
std::uint32_t RecordedCardBios::close(std::uint32_t fd) {
    const auto result = constant(observe("close", {fd}), fd);
    offsets_.erase(fd);
    return result;
}
std::uint32_t RecordedCardBios::format(std::uint32_t device_address, std::string_view device) {
    return recorded(observe("format", {device_address}, {std::string(device)}), "format");
}
std::optional<RecordedCardBios::DirectoryEntry> RecordedCardBios::entry(Request &request,
                                                                        std::uint32_t directory) {
    auto result = take(request.kind);
    request.result = result.value;
    request.result_source = "recorded";
    request.response = result.bytes;
    if (result.value == 0)
        return std::nullopt;
    if (result.value != directory)
        throw InputError("A recorded directory result differs from the native destination");
    DirectoryEntry bytes{};
    std::ranges::copy(result.bytes, bytes.begin());
    return bytes;
}
std::optional<RecordedCardBios::DirectoryEntry>
RecordedCardBios::first_file(std::uint32_t pattern_address, std::string_view pattern,
                             std::uint32_t directory) {
    return entry(observe("first", {pattern_address, directory}, {std::string(pattern)}), directory);
}
std::optional<RecordedCardBios::DirectoryEntry>
RecordedCardBios::next_file(std::uint32_t directory) {
    return entry(observe("next", {directory}), directory);
}
std::uint32_t RecordedCardBios::rename(std::uint32_t from_address, std::string_view from,
                                       std::uint32_t to_address, std::string_view to) {
    return recorded(
        observe("rename", {from_address, to_address}, {std::string(from), std::string(to)}),
        "rename");
}
std::uint32_t RecordedCardBios::erase(std::uint32_t name_address, std::string_view name) {
    return constant(observe("erase", {name_address}, {std::string(name)}), 1);
}
std::uint32_t RecordedCardBios::kanji_address(std::uint32_t code) {
    return recorded(observe("kanji", {code}), "kanji");
}
std::uint32_t RecordedCardBios::rom_halfword(std::uint32_t address) {
    return recorded(observe("rom", {address}), "rom");
}
std::uint32_t RecordedCardBios::card_info(std::uint32_t port) {
    return recorded(observe("info", {port}), "info");
}
RecordedCardBios::CardPatch RecordedCardBios::init_card(std::uint32_t pad_enable) {
    auto &request = observe("init", {pad_enable});
    auto result = take("init");
    // init_card's returned value is the patch bytes, not the unused input word.
    request.result_source = "recorded_patch";
    request.response = result.bytes;
    CardPatch bytes{};
    std::ranges::copy(result.bytes, bytes.begin());
    return bytes;
}
void RecordedCardBios::start_card() { observe("start_card").result_source = "void"; }

} // namespace xem::analysis
