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
#include <iomanip>
#include "config_parser.h"

using namespace std;

struct SimpleServer {
    string ip;
    int port;
    bool healthy;
    int connections;
    long total_requests;
};

vector<SimpleServer> servers;
atomic<bool> running{true};
string lb_ip;
int lb_port;
int algorithm = 0;
mutex servers_mutex;
mutex file_location_mutex;
mutex log_mutex;
map<string, int> file_locations;

// Log files
ofstream health_log;
ofstream forward_log;

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
        
        servers.push_back({ip, port, false, 0, 0});
    }
    
    return true;
}

string get_current_timestamp() {
    auto now = chrono::system_clock::now();
    auto time_t_now = chrono::system_clock::to_time_t(now);
    
    // Format: 2025-10-18T00:05:03
    stringstream timestamp;
    timestamp << put_time(localtime(&time_t_now), "%Y-%m-%dT%H:%M:%S");
    return timestamp.str();
}

void initialize_logs() {
    auto now = chrono::system_clock::now();
    auto time_t_now = chrono::system_clock::to_time_t(now);
    stringstream timestamp;
    timestamp << put_time(localtime(&time_t_now), "%Y%m%d_%H%M%S");
    
    health_log.open("health_log_" + timestamp.str() + ".csv");
    forward_log.open("forward_log_" + timestamp.str() + ".csv");
    
    cout << "Log files initialized with timestamp: " << timestamp.str() << endl;
}

void log_health_check(int server_id, bool healthy, int response_time) {
    lock_guard<mutex> lock(log_mutex);
    string timestamp = get_current_timestamp();
    string status = healthy ? "UP" : "DOWN";
    
    // Format: 2025-10-18T00:05:03,1,0,UP
    health_log << timestamp << "," 
               << server_id << "," 
               << response_time << "," 
               << status << endl;
}

void log_forward_request(const string& operation, const string& filename, int server_id = -1) {
    lock_guard<mutex> lock(log_mutex);
    string timestamp = get_current_timestamp();
    
    if (operation == "ARRIVE") {
        // Format: 2025-10-17T23:56:49,ARRIVE,-1,PUT file1_remote.txt
        forward_log << timestamp << ",ARRIVE,-1," << operation << " " << filename << endl;
    } else {
        // Format for forwarded requests (we'll add this if needed)
        // forward_log << timestamp << ",FORWARD," << server_id << "," << operation << " " << filename << endl;
    }
}

void log_forward_complete(const string& operation, const string& filename, int server_id) {
    lock_guard<mutex> lock(log_mutex);
    string timestamp = get_current_timestamp();
    
    // Format: 2025-10-18T00:05:03,FORWARD,1,PUT file1_remote.txt
    forward_log << timestamp << ",FORWARD," << server_id << "," << operation << " " << filename << endl;
}

void health_check() {
    while (running) {
        {
            lock_guard<mutex> lock(servers_mutex);
            for (size_t i = 0; i < servers.size(); i++) {
                auto& server = servers[i];
                auto start_time = chrono::steady_clock::now();
                
                int sock = socket(AF_INET, SOCK_STREAM, 0);
                if (sock < 0) {
                    server.healthy = false;
                    log_health_check(i + 1, false, -1);
                    continue;
                }
                
                sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_port = htons(server.port);
                
                if (inet_pton(AF_INET, server.ip.c_str(), &addr.sin_addr) <= 0) {
                    server.healthy = false;
                    log_health_check(i + 1, false, -1);
                    close(sock);
                    continue;
                }
                
                timeval timeout{1, 0};
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
                setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
                
                bool healthy = (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0);
                int response_time = 0;
                
                if (healthy) {
                    send(sock, "HEALTH_CHECK\n", 13, 0);
                    char buf[100];
                    auto send_time = chrono::steady_clock::now();
                    healthy = (recv(sock, buf, sizeof(buf), 0) > 0);
                    auto end_time = chrono::steady_clock::now();
                    response_time = chrono::duration_cast<chrono::milliseconds>(end_time - start_time).count();
                } else {
                    response_time = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start_time).count();
                }
                
                server.healthy = healthy;
                log_health_check(i + 1, healthy, response_time);
                close(sock);
            }
        }
        this_thread::sleep_for(chrono::seconds(1));
    }
}

int select_server(const string& filename = "", bool is_put = false) {
    lock_guard<mutex> lock(servers_mutex);
    
    if (algorithm == 0) {
        // Round Robin
        static int last = 0;
        for (size_t i = 0; i < servers.size(); i++) {
            int idx = (last + i) % servers.size();
            if (servers[idx].healthy) {
                last = (idx + 1) % servers.size();
                
                if (is_put && !filename.empty()) {
                    lock_guard<mutex> file_lock(file_location_mutex);
                    file_locations[filename] = idx;
                }
                
                return idx;
            }
        }
    } else {
        // Least Connections
        int best_idx = -1;
        int min_connections = INT_MAX;
        
        for (size_t i = 0; i < servers.size(); i++) {
            if (servers[i].healthy && servers[i].connections < min_connections) {
                min_connections = servers[i].connections;
                best_idx = i;
            }
        }
        
        if (best_idx != -1 && is_put && !filename.empty()) {
            lock_guard<mutex> file_lock(file_location_mutex);
            file_locations[filename] = best_idx;
        }
        
        return best_idx;
    }
    return -1;
}

int get_server_for_file(const string& filename) {
    lock_guard<mutex> lock(file_location_mutex);
    auto it = file_locations.find(filename);
    if (it != file_locations.end() && it->second >= 0 && it->second < servers.size()) {
        lock_guard<mutex> server_lock(servers_mutex);
        if (servers[it->second].healthy) {
            return it->second;
        }
    }
    return -1;
}

void handle_client(int client_sock) {
    // Get client IP for logging
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    getpeername(client_sock, (sockaddr*)&client_addr, &addr_len);
    string client_ip = inet_ntoa(client_addr.sin_addr);
    
    char buffer[4096];
    ssize_t bytes = recv(client_sock, buffer, sizeof(buffer)-1, 0);
    if (bytes <= 0) {
        close(client_sock);
        return;
    }
    buffer[bytes] = '\0';
    string request(buffer);
    
    istringstream iss(request);
    string command, filename;
    iss >> command >> filename;
    
    // Log request arrival
    log_forward_request("ARRIVE", filename);
    
    int server_idx = -1;
    
    if (command == "PUT") {
        server_idx = select_server(filename, true);
    } else if (command == "GET") {
        server_idx = get_server_for_file(filename);
        if (server_idx == -1) {
            server_idx = select_server();
        }
    } else {
        server_idx = select_server();
    }
    
    if (server_idx == -1) {
        send(client_sock, "ERROR: No servers available\n", 27, 0);
        close(client_sock);
        return;
    }
    
    SimpleServer* target;
    {
        lock_guard<mutex> lock(servers_mutex);
        target = &servers[server_idx];
        target->connections++;
        target->total_requests++;
    }
    
    // Log forward completion
    log_forward_complete(command, filename, server_idx + 1);
    
    cout << "[" << command << "] " << filename << " -> " 
         << target->ip << ":" << target->port 
         << " (Server " << (server_idx + 1) << ")" << endl;
    
    int backend_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (backend_sock < 0) {
        lock_guard<mutex> lock(servers_mutex);
        target->connections--;
        close(client_sock);
        return;
    }
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(target->port);
    
    if (inet_pton(AF_INET, target->ip.c_str(), &addr.sin_addr) <= 0) {
        lock_guard<mutex> lock(servers_mutex);
        target->connections--;
        send(client_sock, "ERROR: Invalid server address\n", 29, 0);
        close(backend_sock);
        close(client_sock);
        return;
    }
    
    if (connect(backend_sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
        send(backend_sock, buffer, bytes, 0);
        
        char response[4096];
        ssize_t response_bytes = recv(backend_sock, response, sizeof(response), 0);
        if (response_bytes > 0) {
            send(client_sock, response, response_bytes, 0);
        }
    } else {
        send(client_sock, "ERROR: Backend connection failed\n", 32, 0);
    }
    
    {
        lock_guard<mutex> lock(servers_mutex);
        target->connections--;
    }
    
    close(backend_sock);
    close(client_sock);
}

void cleanup() {
    running = false;
    if (health_log.is_open()) health_log.close();
    if (forward_log.is_open()) forward_log.close();
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
        } else if (arg == "--help" || arg == "-h") {
            cout << "Usage: " << argv[0] << " [options]" << endl;
            cout << "Options:" << endl;
            cout << "  --algorithm <algorithm>  Set load balancing algorithm" << endl;
            cout << "                           (round_robin/0 or least_connections/1)" << endl;
            cout << "  --help, -h               Show this help message" << endl;
            return 0;
        }
    }
    
    // Set up cleanup on exit
    atexit(cleanup);
    
    if (!load_config()) {
        cerr << "Failed to load configuration" << endl;
        return 1;
    }
    
    // Initialize logging
    initialize_logs();
    
    cout << "==========================================" << endl;
    cout << "Load Balancer Configuration:" << endl;
    cout << "LB: " << lb_ip << ":" << lb_port << endl;
    cout << "Algorithm: " << (algorithm == 0 ? "Round Robin" : "Least Connections") << endl;
    cout << "Backend Servers:" << endl;
    for (size_t i = 0; i < servers.size(); i++) {
        cout << "  Server " << (i + 1) << ": " << servers[i].ip << ":" << servers[i].port << endl;
    }
    cout << "==========================================" << endl;
    cout << "Logging to:" << endl;
    cout << "  - Health checks: health_log_*.csv" << endl;
    cout << "  - Request forwarding: forward_log_*.csv" << endl;
    
    // Start health checking thread
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