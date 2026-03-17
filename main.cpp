#include <iostream>

#ifdef _WIN32
#include "app/CommandLine.h"
#include "app/CrashLogger.h"
#include "app/MenuApp.h"
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    tenriff::app::install_crash_handlers();

    try {
        auto parsed = tenriff::app::CommandLine::parse(argc, argv);
        for (const auto& warning : parsed.warnings) {
            std::cerr << "[warn] " << warning << std::endl;
        }

        tenriff::app::MenuApp app;
        if (!app.initialize(parsed.options)) {
            std::cerr << "[error] Failed to initialize TenRiff menu." << std::endl;
            const int exit_code = app.exit_code();
            return exit_code != 0 ? exit_code : 1;
        }

        app.run();
        app.shutdown();
        return app.exit_code();
    } catch (const std::exception& e) {
        tenriff::app::write_current_exception_log("Unhandled std::exception escaped main()");
        std::cerr << "[error] Unhandled exception escaped main: " << e.what() << std::endl;
        return 14;
    } catch (...) {
        tenriff::app::write_current_exception_log("Unhandled non-std exception escaped main()");
        std::cerr << "[error] Unhandled non-standard exception escaped main." << std::endl;
        return 14;
    }
#else
    std::cerr << "TenRiff menu UI is currently Windows-only.\n"
                 "Build and run the Windows binary to see the GUI main menu.\n";
    return 13;
#endif
}
