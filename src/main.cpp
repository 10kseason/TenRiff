#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include "app/CommandLine.h"
#include "app/CrashLogger.h"
#include "app/MenuApp.h"
#include "util/Utf8Compat.h"
#endif

#ifdef _WIN32
int wmain(int argc, wchar_t** wide_argv) {
#else
int main(int argc, char** argv) {
#endif
#ifdef _WIN32
    tenriff::app::install_crash_handlers();

    try {
        // The rest of the application consumes UTF-8. Narrow CRT argv uses the
        // system ANSI code page and can lose even valid Windows path names.
        std::vector<std::string> arguments;
        arguments.reserve(argc);
        for (int i = 0; i < argc; ++i) {
            arguments.push_back(tenriff::util::utf8_from_wide_lossy(wide_argv[i]));
        }
        std::vector<char*> argv;
        argv.reserve(arguments.size() + 1);
        for (auto& argument : arguments) argv.push_back(argument.data());
        argv.push_back(nullptr);
        auto parsed = tenriff::app::CommandLine::parse(argc, argv.data());
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
