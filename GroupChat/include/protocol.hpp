#pragma once
#include <cstdint>
#include <string>
#include <array>
#include <arpa/inet.h>

constexpr size_t MAX_MSG_SIZE = 1024;         // total over the wire
constexpr uint8_t MSG_TYPE_DATA = 0;
constexpr uint8_t MSG_TYPE_END  = 1;

// Convert uint32 IP (network order) to dotted string
inline std::string ip_to_string(uint32_t ip_be) {
    struct in_addr addr{};
    addr.s_addr = ip_be; // already big endian (network order)
    char buf[INET_ADDRSTRLEN];
    const char* s = inet_ntop(AF_INET, &addr, buf, sizeof(buf));
    return s ? std::string(s) : std::string("0.0.0.0");
}

// Pack a server->client DATA message: [type(1)][ip(4)][port(2)][payload...]['\n']
inline std::string pack_server_data(uint32_t ip_be, uint16_t port_be, const std::string& payload_no_newline) {
    std::string out;
    out.reserve(1 + 4 + 2 + payload_no_newline.size() + 1);
    out.push_back(static_cast<char>(MSG_TYPE_DATA));

    // Append binary IP and port in network order
    out.append(reinterpret_cast<const char*>(&ip_be), 4);
    out.append(reinterpret_cast<const char*>(&port_be), 2);

    // Append payload then newline
    out.append(payload_no_newline);
    out.push_back('\n');
    return out;
}

// Pack client->server DATA message: [type(1)][payload...]['\n']
inline std::string pack_client_data(const std::string& payload_no_newline) {
    std::string out;
    out.reserve(1 + payload_no_newline.size() + 1);
    out.push_back(static_cast<char>(MSG_TYPE_DATA));
    out.append(payload_no_newline);
    out.push_back('\n');
    return out;
}

// Pack END message: [type(1)]['\n']
inline std::string pack_end() {
    std::string out;
    out.reserve(2);
    out.push_back(static_cast<char>(MSG_TYPE_END));
    out.push_back('\n');
    return out;
}
