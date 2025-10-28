#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>
#include <cerrno>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/random.h>

#include "protocol.hpp"

// Convert random bytes to hex (like the assignment’s convert)
static int to_hex(const uint8_t* buf, ssize_t buf_size, char* str, ssize_t str_size) {
    if (!buf || !str || buf_size <= 0 || str_size < (buf_size * 2 + 1)) return -1;
    for (int i = 0; i < buf_size; i++) sprintf(str + i * 2, "%02X", buf[i]);
    str[buf_size * 2] = '\0';
    return 0;
}

static std::atomic<bool> g_received_end{false};

static void recv_loop(int sock, std::ofstream& logf) {
    std::string buf;
    buf.reserve(4096);

    while (!g_received_end.load()) {
        char tmp[4096];
        ssize_t n = recv(sock, tmp, sizeof(tmp), 0);
        if (n <= 0) break;
        buf.append(tmp, n);

        // Parse complete messages by newline
        while (true) {
            auto pos = buf.find('\n');
            if (pos == std::string::npos) break;

            std::string line = buf.substr(0, pos); // without '\n'
            buf.erase(0, pos + 1);

            if (line.empty()) continue;

            uint8_t type = static_cast<uint8_t>(line[0]);
            if (type == MSG_TYPE_DATA) {
                // Expect at least 1 + 4 + 2
                if (line.size() < 1 + 4 + 2) continue; // malformed, skip
                uint32_t ip_be;
                uint16_t port_be;
                std::memcpy(&ip_be,  line.data() + 1, 4);
                std::memcpy(&port_be,line.data() + 5, 2);

                std::string payload = line.substr(1 + 4 + 2); // rest is hex text
                std::string ip_str = ip_to_string(ip_be);
                uint16_t port_host = ntohs(port_be);

                // Print + log one line per message, respecting format
                // "%-15s%-10u%s\n"  -> pad ip to 15, port to width 10, then payload
                char header[64];
                snprintf(header, sizeof(header), "%-15s%-10u", ip_str.c_str(), port_host);
                std::cout << header << payload << "\n";
                if (logf.is_open()) {
                    logf << header << payload << "\n";
                    logf.flush();
                }
            } else if (type == MSG_TYPE_END) {
                g_received_end.store(true);
                // No payload expected; exit loop after processing buffer
            } else {
                // Unknown type: ignore
            }
        }
    }
}

static void send_loop(int sock, int num_msgs) {
    // Send num_msgs random messages as hex strings, then END
    for (int i = 0; i < num_msgs; ++i) {
        uint8_t rb[16];
        if (getentropy(rb, sizeof(rb)) != 0) {
            // Fallback: /dev/urandom
            FILE* f = fopen("/dev/urandom", "rb");
            fread(rb, 1, sizeof(rb), f);
            fclose(f);
        }
        char hexbuf[16 * 2 + 1];
        if (to_hex(rb, 16, hexbuf, sizeof(hexbuf)) != 0) continue;

        std::string payload(hexbuf);
        auto msg = pack_client_data(payload);
        ssize_t n = send(sock, msg.data(), msg.size(), 0);
        if (n < 0) break;
        // Optional small delay to vary timing
        // std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Send END
    auto endmsg = pack_end();
    send(sock, endmsg.data(), endmsg.size(), 0);
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: ./client <IP> <port> <#messages> <log_file>\n";
        return 1;
    }
    std::string ip = argv[1];
    int port = std::stoi(argv[2]);
    int num_msgs = std::stoi(argv[3]);
    std::string log_path = argv[4];

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &srv.sin_addr) != 1) {
        std::cerr << "Invalid IP\n"; close(sock); return 1;
    }

    if (connect(sock, (sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("connect"); close(sock); return 1;
    }

    std::ofstream logf(log_path, std::ios::out | std::ios::trunc);
    if (!logf.is_open()) {
        std::cerr << "Warning: failed to open log file: " << log_path << "\n";
    }

    std::thread rt(recv_loop, sock, std::ref(logf));
    std::thread st(send_loop, sock, num_msgs);

    st.join();
    // Wait for END from server
    rt.join();

    if (logf.is_open()) logf.close();
    close(sock);
    return 0;
}
