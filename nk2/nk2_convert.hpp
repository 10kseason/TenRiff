#pragma once

#include "nk2/chart.hpp"
#include "nk2_report.hpp"

namespace keyconv::nk2 {

struct NK2ConversionResult {
    Chart chart;
    NK2Report report;
};

NK2Report analyzeReportOnly(const Chart& chart, const NK2Options& options);
NK2ConversionResult convertChart(const Chart& chart, const NK2Options& options);

}  // namespace keyconv::nk2
