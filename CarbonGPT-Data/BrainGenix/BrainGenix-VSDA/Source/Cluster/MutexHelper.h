#pragma once

#include <mutex>
#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>

std::lock_guard<std::timed_mutex> lockWithTimeout(
    std::timed_mutex& mutex,
    const std::string& identifier = "",
    std::chrono::milliseconds timeout = std::chrono::milliseconds(3000)
);