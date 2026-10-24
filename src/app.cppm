export module app;

import std;

export namespace arcade::app {

    constexpr auto version() noexcept -> const char* {
        return "ARCADE UI v0.1.0-rc1";
    }
    
    auto run() -> int {
        std::println("Hello from C++29!");
        std::println("Application version: {}", version());
        return 0;
    }

} // namespace arcade::app
