#include "MutexHelper.h"


std::lock_guard<std::timed_mutex> lockWithTimeout(
    std::timed_mutex& mutex,
    const std::string& identifier,
    std::chrono::milliseconds timeout
) {
    static std::unordered_map<std::string, bool> hasWarnedMap;
    bool& hasWarned = hasWarnedMap[identifier];
    if (!mutex.try_lock_for(timeout)) {
        if (!hasWarned) {
            std::cerr << "[Mutex Timeout Warning] Mutex blocked for more than "
                      << timeout.count() << "ms at: "
                      << (identifier.empty() ? "unknown location" : identifier)
                      << std::endl;
            hasWarned = true;
        }
        mutex.lock(); // Proceed and block
    }

    return std::lock_guard<std::timed_mutex>(mutex, std::adopt_lock);
}
