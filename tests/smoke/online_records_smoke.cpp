#include <iostream>
#include <string>

#include "app/OnlineRecordsClient.h"
#include "util/Utf8Compat.h"

namespace {
int read_records(const std::string& source, const std::string& chart) {
    tenriff::app::OnlineRecordsResponse response;
    std::string error;
    const bool ok = source == "--sites" ? tenriff::app::fetch_sites_records_once("", chart, 0, response, error)
                                        : tenriff::app::fetch_online_records_once(source, chart, response, error);
    if (!ok) {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << "chart=" << response.chart_sha256
              << " boards=" << response.boards.size() << " records=" << response.records.size();
    if (!response.records.empty()) {
        std::cout << " verification="
                  << response.records.front().verification_status;
    }
    std::cout << '\n';
    return 0;
}

int usage() {
    std::cerr << "Usage: online_records_smoke <base-url> <chart-sha256> OR --sites <public-chart-title>\n";
    return 2;
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return usage();
    return read_records(tenriff::util::utf8_from_wide_lossy(argv[1]),
                        tenriff::util::utf8_from_wide_lossy(argv[2]));
}
#else
int main(int argc, char** argv) {
    if (argc != 3) return usage();
    return read_records(argv[1], argv[2]);
}
#endif
