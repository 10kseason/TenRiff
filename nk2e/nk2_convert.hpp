#pragma once

#include "nk2e/chart.hpp"
#include "nk2_report.hpp"

namespace keyweaver_e::nk2 {

struct NK2ConversionResult {
    Chart chart;
    NK2Report report;
};

NK2Report analyzeReportOnly(const Chart& chart, const NK2Options& options);
NK2ConversionResult convertChart(const Chart& chart, const NK2Options& options);

}  // namespace keyweaver_e::nk2
