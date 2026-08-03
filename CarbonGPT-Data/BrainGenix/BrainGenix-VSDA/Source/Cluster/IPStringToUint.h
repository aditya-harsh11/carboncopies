#pragma once


#include <arpa/inet.h>  // For inet_pton, in_addr, AF_INET
#include <netinet/in.h> // For ntohl




// Helper function to convert an IP string (e.g., "224.0.0.123") to an unsigned int
unsigned int IpStringToUint(const std::string& ipStr) {
    struct in_addr addr;
    inet_pton(AF_INET, ipStr.c_str(), &addr);
    return ntohl(addr.s_addr);
}

