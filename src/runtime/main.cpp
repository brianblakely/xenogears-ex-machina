#include "xem/build_info.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char *argv[]) {
    if (argc == 2 && std::string_view{argv[1]} == "--version") {
        std::cout << "Xenogears Ex Machina " << xem::version << '\n';
        return 0;
    }
    if (argc == 1 || (argc == 2 && std::string_view{argv[1]} == "--help")) {
        std::cout << "Xenogears Ex Machina: Phase 0 build baseline.\n"
                     "No game runtime is implemented yet.\n"
                     "Usage: xem-baseline [--help | --version]\n";
        return 0;
    }
    std::cerr << "Unsupported argument. Usage: xem-baseline [--help | --version]\n";
    return 2;
}
