#include <array>
#include <string>

#if defined(_WIN32)
#include <WinSock2.h>
#else
#include <unistd.h>
#endif

std::string GetHostName() {
    std::array<char, 256> hostname_buf;
    if (gethostname(hostname_buf.data(), hostname_buf.size()) == 0) {
        return std::string(hostname_buf.data());
    } else {
        return "unknown-host";
    }
}