#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>
#include <map>
#include <mutex>
#include <climits>
#include "config_parser.h"

using namespace std;

class SimpleServer {
public:
    string ip;
    int port;
    atomic<bool> healthy;
    atomic<int> connections;
    atomic<long> total_requests;
    
    // Constructor
    SimpleServer(const string& ip_addr, int port_num) 
        : ip(ip_addr), port(port_num), healthy(false), connections(0), total_requests(0) {}
    
    // Copy constructor
    SimpleServer(const SimpleServer& other)
        : ip(other.ip), port(other.port), 
          healthy(other.healthy.load()), 
          connections(other.connections.load()),
          total_requests(other.total_requests.load()) {}
    
    // Assignment operator
    SimpleServer& operator=(const SimpleServer& other) {
        if (this != &other) {
            ip = other.ip;
            port = other.port;
            healthy = other.healthy.load();
            connections = other.connections.load();
            total_requests = other.total_requests.load();
        }
        return *this;
    }
};

vector<SimpleServer> servers;
atomic<bool> running{true};
string lb_ip;
int lb_port;
int algorithm = 0; // 0 = Round Robin, 1 = Least Connections
mutex file_location_mutex;
map<string, size_t> file_locations; // filename -> server_index

bool load_config() {
    ConfigParser parser;
    if (!parser.load("config.json")) {
        cerr << "Failed to load config.json" << endl;
        return false;
    }
    
    lb_ip = parser.getString("lb_ip", "127.0.0.1");
    lb_port = parser.getInt("lb_port", 8000);
    
    servers.clear();
    for (int i = 1; i <= 4; i++) {
        string ip_key = "server" + to_string(i) + "_ip";
        string port_key = "server" + to_string(i) + "_port";
        
        string ip = parser.getString(ip_key, "127.0.0.1");
        int port = parser.getInt(port_key, 9000 + i);
        
        servers.emplace_back(ip, port);
    }
    
    return true;
}

void health_check() {
    while (running) {
        for (auto& server : servers) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                server.healthy = false;
                continue;
            }
            
            sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(server.port);
            
            if (inet_pton(AF_INET, server.ip.c_str(), &addr.sin_addr) <= 0) {
                server.healthy = false;
                close(sock);
                continue;
            }
            
            timeval timeout{1, 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
            
            bool healthy = (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0);
            if (healthy) {
                send(sock, "HEALTH_CHECK\n", 13, 0);
                char buf[100];
                healthy = (recv(sock, buf, sizeof(buf), 0) > 0);
            }
            
            server.healthy = healthy;
            close(sock);
        }
        this_thread::sleep_for(chrono::seconds(1));
    }
}

SimpleServer* select_server(const string& filename = "", bool is_put = false) {
    if (algorithm == 0) {
        // Round Robin
        static size_t last = 0;
        for (size_t i = 0; i < servers.size(); i++) {
            size_t idx = (last + i) % servers.size();
            if (servers[idx].healthy) {
                last = (idx + 1) % servers.size();
                
                // For PUT, remember file location
                if (is_put && !filename.empty()) {
                    lock_guard<mutex> lock(file_location_mutex);
                    file_locations[filename] = idx;
                }
                
                return &servers[idx];
            }
        }
    } else {
        // Least Connections
        SimpleServer* best_server = nullptr;
        int min_connections = INT_MAX;
        
        for (auto& server : servers) {
            if (server.healthy && server.connections < min_connections) {
                min_connections = server.connections;
                best_server = &server;
            }
        }
        
        if (best_server && is_put && !filename.empty()) {
            // Find server index
            for (size_t i = 0; i < servers.size(); i++) {
                if (&servers[i] == best_server) {
                    lock_guard<mutex> lock(file_location_mutex);
                    file_locations[filename] = i;
                    break;
                }
            }
        }
        
        return best_server;
    }
    return nullptr;
}

SimpleServer* get_server_for_file(const string& filename) {
    lock_guard<mutex> lock(file_location_mutex);
    auto it = file_locations.find(filename);
    if (it != file_locations.end() && it->second < servers.size() && servers[it->second].healthy) {
        return &servers[it->second];
    }
    return nullptr;
}

void handle_client(int client_sock) {
    char buffer[4096];
    ssize_t bytes = recv(client_sock, buffer, sizeof(buffer)-1, 0);
    if (bytes <= 0) {
        close(client_sock);
        return;
    }
    buffer[bytes] = '\0';
    string request(buffer);
    
    // Parse request type and filename
    istringstream iss(request);
    string command, filename;
    iss >> command >> filename;
    
    SimpleServer* target = nullptr;
    
    if (command == "PUT") {
        target = select_server(filename, true);
    } else if (command == "GET") {
        // Try to find the server where the file was originally stored
        target = get_server_for_file(filename);
        if (!target) {
            // If not found, use normal selection
            target = select_server();
        }
    } else {
        target = select_server();
    }
    
    if (!target) {
        send(client_sock, "ERROR: No servers available\n", 27, 0);
        close(client_sock);
        return;
    }
    
    int backend_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (backend_sock < 0) {
        close(client_sock);
        return;
    }
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(target->port);
    
    if (inet_pton(AF_INET, target->ip.c_str(), &addr.sin_addr) <= 0) {
        send(client_sock, "ERROR: Invalid server address\n", 29, 0);
        close(backend_sock);
        close(client_sock);
        return;
    }
    
    if (connect(backend_sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
        target->connections++;
        target->total_requests++;
        
        send(backend_sock, buffer, bytes, 0);
        
        char response[4096];
        ssize_t response_bytes = recv(backend_sock, response, sizeof(response), 0);
        if (response_bytes > 0) {
            send(client_sock, response, response_bytes, 0);
        }
        
        target->connections--;
    } else {
        send(client_sock, "ERROR: Backend connection failed\n", 32, 0);
    }
    
    close(backend_sock);
    close(client_sock);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments for algorithm
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--algorithm" && i + 1 < argc) {
            string algo = argv[++i];
            if (algo == "round_robin" || algo == "0") {
                algorithm = 0;
            } else if (algo == "least_connections" || algo == "1") {
                algorithm = 1;
            }
        }
    }
    
    if (!load_config()) {
        cerr << "Failed to load configuration" << endl;
        return 1;
    }
    
    cout << "Load Balancer Configuration:" << endl;
    cout << "LB: " << lb_ip << ":" << lb_port << endl;
    cout << "Algorithm: " << (algorithm == 0 ? "Round Robin" : "Least Connections") << endl;
    cout << "Backend Servers:" << endl;
    for (const auto& server : servers) {
        cout << "  " << server.ip << ":" << server.port << endl;
    }
    
    thread(health_check).detach();
    
    // Wait for initial health checks
    this_thread::sleep_for(chrono::seconds(2));
    
    int lb_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (lb_sock < 0) {
        perror("socket");
        return 1;
    }
    
    int opt = 1;
    setsockopt(lb_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(lb_port);
    
    if (bind(lb_sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(lb_sock);
        return 1;
    }
    
    if (listen(lb_sock, 10) < 0) {
        perror("listen");
        close(lb_sock);
        return 1;
    }
    
    cout << "Load Balancer started successfully on port " << lb_port << endl;
    
    while (running) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(lb_sock, (sockaddr*)&client_addr, &client_len);
        
        if (client_sock >= 0) {
            thread(handle_client, client_sock).detach();
        }
    }
    
    close(lb_sock);
    return 0;
}