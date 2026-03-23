#pragma once

#if defined(__APPLE__) || defined(__FreeBSD__)
#include "poller_bsd.h" // IWYU pragma: export
#elif defined(__linux__)
#include "poller_linux.h" // IWYU pragma: export
#endif                    // defined(__APPLE__) || defined(__FreeBSD__)

#include "poller_common.h" // IWYU pragma: export
