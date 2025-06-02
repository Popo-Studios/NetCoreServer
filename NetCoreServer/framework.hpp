#pragma once

#define MSGPACK_USE_CPP17

#include <enet/enet.h>
#include <msgpack.hpp>
#include <uuid.h>

#include <boost/lockfree/queue.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <functional>
#include <ctime>
#include <map>
#include <limits>
#include <optional>
#include <algorithm>
#include <future>
#include <type_traits>
#include <mutex>

typedef float float32_t;
typedef double float64_t;

#if defined(_WIN32) || defined(_WIN64)
#include <time.h>
#endif

inline std::tm safe_localtime(const std::time_t* time) {
    std::tm result{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&result, time);
#else
    localtime_r(time, &result);
#endif
    return result;
}

inline std::string toLower(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return result;
}