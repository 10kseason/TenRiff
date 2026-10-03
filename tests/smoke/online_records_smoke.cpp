#include <iostream>
#include <string>

#include "app/OnlineRecordsClient.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: online_records_smoke <base-url> <chart-sha256> OR --sites <public-chart-title>\n";
        return 2;
    }
    tenriff::app::OnlineRecordsResponse response;
    std::string error;
    const bool sites = std::string(argv[1]) == "--sites";
    const bool ok = sites ? tenriff::app::fetch_sites_records_once("", argv[2], 0, response, error)
                          : tenriff::app::fetch_online_records_once(argv[1], argv[2], response, error);
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
