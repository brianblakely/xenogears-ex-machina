#pragma once

#include "xem/reconstruction/menu_overlay.hpp"

#include <deque>
#include <map>
#include <string>

namespace xem::analysis {

// External results only. Request observations are produced from native callers;
// no expected request or later game image is accepted by this adapter.
class RecordedCardBios final : public reconstruction::menu::CardBios {
  public:
    struct Request {
        std::string kind;
        std::vector<std::uint32_t> arguments;
        std::vector<std::string> names;
        std::vector<std::uint8_t> payload;
        std::optional<std::uint32_t> result;
        std::string result_source;
        std::vector<std::uint8_t> response;
        // Observation only: known synchronous bytes before this request.
        // Unknown handles and async modes have no qualified offset.
        std::optional<std::uint32_t> offset;
    };
    struct InputError : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    void add(const std::string &kind, std::uint32_t value, std::vector<std::uint8_t> bytes);
    [[nodiscard]] std::size_t unconsumed() const;
    [[nodiscard]] const std::vector<Request> &requests() const { return requests_; }

    void bu_init() override;
    std::uint32_t open_event(std::uint32_t event_class, std::uint32_t spec, std::uint32_t mode,
                             std::uint32_t handler) override;
    std::uint32_t close_event(std::uint32_t event) override;
    std::uint32_t test_event(std::uint32_t event) override;
    std::uint32_t enable_event(std::uint32_t event) override;
    void undeliver_event(std::uint32_t event_class, std::uint32_t spec) override;
    std::uint32_t enter_critical_section() override;
    void exit_critical_section() override;
    std::uint32_t open(std::uint32_t name_address, std::string_view name,
                       std::uint32_t mode) override;
    ReadResult read(std::uint32_t fd, std::uint32_t buffer, std::uint32_t count) override;
    std::uint32_t write(std::uint32_t fd, std::uint32_t buffer,
                        std::span<const std::uint8_t> bytes) override;
    std::uint32_t close(std::uint32_t fd) override;
    std::uint32_t format(std::uint32_t device_address, std::string_view device) override;
    std::optional<DirectoryEntry> first_file(std::uint32_t pattern_address,
                                             std::string_view pattern,
                                             std::uint32_t directory) override;
    std::optional<DirectoryEntry> next_file(std::uint32_t directory) override;
    std::uint32_t rename(std::uint32_t from_address, std::string_view from,
                         std::uint32_t to_address, std::string_view to) override;
    std::uint32_t erase(std::uint32_t name_address, std::string_view name) override;
    std::uint32_t kanji_address(std::uint32_t code) override;
    std::uint32_t rom_halfword(std::uint32_t address) override;
    std::uint32_t card_info(std::uint32_t port) override;
    CardPatch init_card(std::uint32_t pad_enable) override;
    void start_card() override;

  private:
    struct Result {
        std::uint32_t value;
        std::vector<std::uint8_t> bytes;
    };
    Result take(const std::string &kind);
    Request &observe(std::string kind, std::vector<std::uint32_t> arguments = {},
                     std::vector<std::string> names = {});
    std::uint32_t recorded(Request &request, const std::string &kind);
    std::uint32_t constant(Request &request, std::uint32_t value);
    std::optional<DirectoryEntry> entry(Request &request, std::uint32_t directory);
    void transfer(Request &request, std::uint32_t fd, std::uint32_t count, std::uint32_t result);
    std::map<std::string, std::deque<Result>> results_;
    std::vector<Request> requests_;
    std::map<std::uint32_t, std::optional<std::uint32_t>> offsets_;
    std::uint32_t poll_{};
};

} // namespace xem::analysis
