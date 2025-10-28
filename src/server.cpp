#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <thread>
#include <mutex>
#include <algorithm>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "protocol.hpp"

struct ClientInfo {
    int fd;
    sockaddr_in addr{};
    std::string buffer; // partial data until '\n'
    bool sent_end = false;
};

static volatile std::sig_atomic_t g_stop = 0;
void handle_sigint(int) { g_stop = 1; }

static void set_reuseaddr(int sock) {
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./server <port> <#clients>\n";
        return 1;
    }
    int port = std::stoi(argv[1]);
    int expected_clients = std::stoi(argv[2]);
    if (expected_clients < 0 || expected_clients > 1000) expected_clients = 100;

    std::signal(SIGINT, handle_sigint);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }
    set_reuseaddr(listen_fd);

    sockaddr_in srv{}; 
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);

    if (bind(listen_fd, (sockaddr*)&srv, sizeof(srv)) < 0) { perror("bind"); return 1; }
    if (listen(listen_fd, 128) < 0) { perror("listen"); return 1; }

    std::unordered_map<int, ClientInfo> clients; // fd -> info
    int max_fd = listen_fd;

    auto broadcast_raw = [&](const std::string& bytes) {
        for (auto it = clients.begin(); it != clients.end(); ) {
            int fd = it->first;
            ssize_t n = send(fd, bytes.data(), bytes.size(), 0);
            if (n < 0) {
                // Drop dead client
                close(fd);
                it = clients.erase(it);
            } else {
                ++it;
            }
        }
    };

    auto handle_complete_line = [&](ClientInfo& ci, const std::string& line) -> int {
    if (line.empty()) return 0;

    uint8_t type = static_cast<uint8_t>(line[0]);

    if (type == MSG_TYPE_DATA) {
        // payload after the type byte
        std::string payload_no_nl = line.substr(1);

        uint32_t ip_be  = ci.addr.sin_addr.s_addr; // network order
        uint16_t port_be = ci.addr.sin_port;       // network order
        std::string framed = pack_server_data(ip_be, port_be, payload_no_nl);
        broadcast_raw(framed);

        return 0; // <-- make sure to return here
    } 
    else if (type == MSG_TYPE_END) {
        ci.sent_end = true;

        int end_count = 0;
        for (const auto& kv : clients)
            if (kv.second.sent_end) ++end_count;

        if (end_count >= expected_clients) {
            std::string endmsg = pack_end();
            broadcast_raw(endmsg);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            for (auto& kv : clients) close(kv.first);
            close(listen_fd);
            return 2; // special code to exit
        }
        return 0;
    }

        // Unknown type
        return 0;
    };


    std::cout << "Server listening on port " << port 
              << " expecting " << expected_clients << " clients\n";

    while (!g_stop) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listen_fd, &rfds);
        max_fd = listen_fd;
        for (auto& kv : clients) {
            FD_SET(kv.first, &rfds);
            if (kv.first > max_fd) max_fd = kv.first;
        }

        int rv = select(max_fd + 1, &rfds, nullptr, nullptr, nullptr);
        if (rv < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        // New connection?
        if (FD_ISSET(listen_fd, &rfds)) {
            sockaddr_in cli{};
            socklen_t cl = sizeof(cli);
            int cfd = accept(listen_fd, (sockaddr*)&cli, &cl);
            if (cfd >= 0) {
                ClientInfo ci{};
                ci.fd = cfd;
                ci.addr = cli;
                clients[cfd] = std::move(ci);
            }
        }

        // Existing clients
        std::vector<int> to_close;
        for (auto& kv : clients) {
            int fd = kv.first;
            if (!FD_ISSET(fd, &rfds)) continue;

            char buf[4096];
            ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) {
                to_close.push_back(fd);
                continue;
            }
            kv.second.buffer.append(buf, n);

            // Process complete lines (terminated by '\n')
            while (true) {
                auto& s = kv.second.buffer;
                auto pos = s.find('\n');
                if (pos == std::string::npos) break;
                std::string one = s.substr(0, pos); // without '\n'
                s.erase(0, pos + 1);

                int code = handle_complete_line(kv.second, one);
                if (code == 2) return 0; // graceful global shutdown
            }
        }

        for (int fd : to_close) {
            close(fd);
            clients.erase(fd);
        }
    }

    // SIGINT or loop exit: try to notify clients end (best-effort)
    std::string endmsg = pack_end();
    // broadcast_raw may invalidate iterator; copy fds first
    for (auto& kv : clients) send(kv.first, endmsg.data(), endmsg.size(), 0);
    for (auto& kv : clients) close(kv.first);
    close(listen_fd);
    return 0;
}
