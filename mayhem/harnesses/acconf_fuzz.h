// Stand-in for the meson-generated acconf.h + libaegisub/include/lagi_pre.h precompiled header
// (meson_options defaults on Linux, no optional features). Force-included into every TU.
#pragma once
#define AEGISUB_VERSION_MAJOR 3
#define AEGISUB_VERSION_MINOR 4
#define AEGISUB_VERSION_PATCH 2
#define P_DATA "/usr/share/aegisub"
#define P_LOCALE "/usr/share/locale"
#define HAVE_ALLOCA_H 1
#define HAVE_MALLOC_H 1
#ifdef __cplusplus
#include <cassert>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <vector>
#include <boost/range/algorithm.hpp>
#include <boost/range/irange.hpp>
#include <boost/regex.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#endif
