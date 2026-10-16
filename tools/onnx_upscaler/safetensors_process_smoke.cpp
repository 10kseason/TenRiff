#define NOMINMAX
#include "render/SafetensorsModel.h"
#include <iostream>

int wmain(int argc, wchar_t** argv) {
    namespace fs = std::filesystem;
    namespace adapter = tenriff::render::safetensors;
    if (argc != 3) return 2;
    const fs::path python(argv[1]), root(argv[2]);
    if (fs::exists(root)) return 3;
    fs::create_directories(root);
    const auto script = root / L"process fixture.py";
    std::ofstream(script) << "import pathlib, subprocess, sys, time\n"
        "mode, output = sys.argv[1:3]\n"
        "if mode == 'args':\n"
        "    pathlib.Path(output).write_text('|'.join(sys.argv[3:]), encoding='utf-8')\n"
        "elif mode == 'fail':\n"
        "    sys.exit(7)\n"
        "else:\n"
        "    child = subprocess.Popen([sys.executable, '-I', '-c', 'import time; time.sleep(30)'])\n"
        "    pathlib.Path(output).write_text(str(child.pid), encoding='ascii')\n"
        "    time.sleep(30)\n";
    int checks = 0;
    try {
        const auto output = root / L"인용 확인.txt";
        std::vector<std::wstring> arguments{L"-I", script.native(), L"args", output.native(),
            L"space & 한글", L"quoted\"value", L"trailing\\", L""};
        if (adapter::run_converter(python, arguments, [] { return false; }) != 0 ||
            adapter::read_bounded(output, 1000) != u8"space & 한글|quoted\"value|trailing\\|") return 4;
        ++checks;
        arguments = {L"-I", script.native(), L"fail", output.native()};
        if (adapter::run_converter(python, arguments, [] { return false; }) != 7) return 5;
        ++checks;
        for (const bool cancel : {false, true}) {
            const auto pid_file = root / (cancel ? L"cancel-child.txt" : L"timeout-child.txt");
            arguments = {L"-I", script.native(), L"sleep", pid_file.native()};
            const auto started = std::chrono::steady_clock::now();
            bool stopped = false;
            try {
                adapter::run_converter(python, arguments, [&] {
                    return cancel && std::chrono::steady_clock::now() - started > std::chrono::milliseconds(350);
                }, std::chrono::milliseconds(cancel ? 2000 : 350));
            } catch (const std::runtime_error& error) {
                const std::string message(error.what());
                stopped = message.find(cancel ? "cancelled" : "exceeded") != std::string::npos;
            }
            if (!stopped || std::chrono::steady_clock::now() - started > std::chrono::seconds(3)) return 6;
            const auto pid_text = adapter::read_bounded(pid_file, 32);
            if (pid_text.empty()) return 7;
            adapter::Handle child;
            child.value = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(std::stoul(pid_text)));
            if (child.value && WaitForSingleObject(child.value, 1000) != WAIT_OBJECT_0) return 8;
            checks += 2;
        }
        bool stopped = false;
        try { adapter::run_converter(python, arguments, [] { return true; }); }
        catch (const std::runtime_error&) { stopped = true; }
        if (!stopped) return 9;
        ++checks;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 10;
    }
    std::cout << "Safetensors process checks passed: " << checks << "/7\n";
    return 0;
}
