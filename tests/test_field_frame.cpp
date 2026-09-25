// Invented registers, regions and requests exercise the geometry coprocessor
// model, libgpu packet helpers, owned regions and frame service results used
// by the field frame. They describe no original content or observation.
#include "xem/reconstruction/gpu.hpp"
#include "xem/reconstruction/gte.hpp"
#include "xem/reconstruction/program.hpp"

#include <deque>
#include <iostream>
#include <stdexcept>

namespace game = xem::reconstruction;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Error, typename Call> void expect_error(Call call, const char *message) {
    try {
        call();
    } catch (const Error &) {
        return;
    }
    throw std::runtime_error(message);
}

void geometry() {
    game::Gte gte;
    gte.transform.r = {4096, 0, 0, 0, 4096, 0, 0, 0, 4096};
    gte.screen.h = 0x100;
    gte.set_vector(0, {0x100, -0x80, 0x400});
    gte.rtps();
    check(gte.sz(3) == 0x400, "RTPS keeps the depth of an identity transform");
    check(gte.sxy(2) == (0x40U | 0xffe0U << 16U), "RTPS projects by H / SZ");
    check(gte.otz() == 0 && gte.flag() == 0, "RTPS leaves OTZ and raises no flag");

    gte.set_ir({0x123, -0x456, 0x789});
    gte.mvmva(0, 3, 3);
    check(gte.ir(1) == 0x123 && gte.ir(2) == -0x456 && gte.ir(3) == 0x789,
          "MVMVA of IR by the identity returns IR");

    // OP takes its diagonal from rotation words 0, 2 and 4.
    gte.set_control(0, 0);
    gte.set_control(2, 0);
    gte.set_control(4, 0x1000);
    gte.set_ir({0x1000, 0, 0});
    gte.execute(0x0cU | 1U << 19U);
    check(gte.mac(1) == 0 && gte.mac(2) == 0x1000 && gte.mac(3) == 0,
          "OP crosses the diagonal with IR");

    gte.set_data(30, 0x10);
    check(gte.data(31) == 27, "LZCR counts the leading zeros of LZCS");
    gte.set_data(30, 0xfffffff0U);
    check(gte.data(31) == 28, "LZCR counts the leading ones of a negative LZCS");
}

void packets() {
    namespace gpu = game::gpu;
    check(gpu::texture_page(1, 1, 0x298, 0x1c0) == 0xba, "GetTPage packs mode, rate and page");
    const std::array<std::int16_t, 4> area{0x60, 0x1c0, 0xc, 8};
    check(gpu::texture_window(&area) ==
              (0xe2000000U | 0x18U << 15U | 0xcU << 10U | 0x1fU << 5U | 0x1eU),
          "The texture window keeps offsets and masks in 8-pixel units");
    check(gpu::texture_window(nullptr) == 0, "No texture window clears it");

    game::OriginalRegions regions;
    regions.add("first", 0x80100000, {1, 2, 3, 4});
    check(regions.contains(0x80100001, 2) && !regions.contains(0x80100003, 2),
          "A region owns its bytes only");
    check(regions.word(0x80100000) == 0x04030201U, "Region words are little-endian");
    regions.put(0x80100002, 0xaabb, 2);
    check(regions.word(0x80100000) == 0xaabb0201U, "A region write replaces its bytes");
    expect_error<std::exception>([&] { regions.add("second", 0x80100003, {0}); },
                                 "Overlapping regions are refused");
}

void services() {
    std::deque<std::uint32_t> results{7};
    check(game::take_service(results, "test") == 7, "A supplied result is consumed in order");
    expect_error<game::ServiceUnavailable>(
        [&] { static_cast<void>(game::take_service(results, "test")); },
        "A missing result is reported, never invented");
}
} // namespace

int main() {
    try {
        geometry();
        packets();
        services();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
