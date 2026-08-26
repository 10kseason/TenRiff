#include <iostream>
#include <string>

#include "app/OnlineRecordsClient.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: online_records_smoke <base-url> <chart-sha256>\n";
        return 2;
    }
    tenriff::app::OnlineRecordsResponse response;
    std::string error;
    if (!tenriff::app::fetch_online_records_once(argv[1], argv[2], response,
                                                  error)) {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << "chart=" << response.chart_sha256
              << " records=" << response.records.size();
    if (!response.records.empty()) {
        std::cout << " first=" << response.records.front().player_name
                  << " verification="
                  << response.records.front().verification_status;
    }
    std::cout << '\n';
    return 0;
}
