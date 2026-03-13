#pragma once

#include "config/Config.h"

namespace tenriff::app {

bool migrate_bms_first_runtime_config(config::RuntimeConfig& config);

}  // namespace tenriff::app
