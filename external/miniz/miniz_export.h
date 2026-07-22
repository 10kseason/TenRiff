#pragma once

// TenRiff links miniz as a private static library, so no DLL import/export
// decoration is required. Upstream normally generates this header via CMake.
#define MINIZ_EXPORT
