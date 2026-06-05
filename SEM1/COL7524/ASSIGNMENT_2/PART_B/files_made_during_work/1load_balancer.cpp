#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <queue>
#include <algorithm>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iomanip>
#include <ctime>
#include "./config_parser.h"

using namespace std;
using namespace std::chrono;

struct BackendServer {
    string ip;
    int port;
    atomic<bool> healthy;
    atomic<int> active_connections;
    atomic<long> total_requests;
    atomic<long long> total_response_time;
    atomic<long long> last_health_check;
    atomic<double> current_load;
    atomic<int> failed_health_checks;
    mutex mtx;
    
    // Constructor
    BackendServer(const string& ip_addr, int port_num) 
        : ip(ip_addr), port(port_num), healthy(false), active_connections(0),
          total_requests(0), total_response_time(0), last_health_check(0),
          current_load(0.0), failed_health_checks(0) {}
    
    // Copy constructor (required for vector operations)
    BackendServer(const BackendServer& other)
        : ip(other.ip), port(other.port), 
          healthy(other.healthy.load()), 
          active_connections(other.active_connections.load()),
          total_requests(other.total_requests.load()),
          total_response_time(other.total_response_time.load()),
          last_health_check(other.last_health_check.load()),
          current_load(other.current_load.load()),
          failed_health_checks(other.failed_health_checks.load()) {}
    
    // Assignment operator
    BackendServer& operator=(const BackendServer& other) {
        if (this != &other) {
            ip = other.ip;
            port = other.port;
            healthy = other.healthy.load();
            active_connections = other.active_connections.load();
            total_requests = other.total_requests.load();
            total_response_time = other.total_response_time.load();
            last_health_check = other.last_health_check.load();
            current_load = other.current_load.load();
            failed_health_checks = other.failed_health_checks.load();
        }
        return *this;
    }
};

class LoadBalancer {
private:
    string lb_ip;
    int lb_port;
    vector<BackendServer> servers;
    atomic<bool> running;
    thread health_check_thread;
    mutex log_mutex;
    ofstream health_log;
    ofstream request_log;
    ofstream metrics_log;
    
    int algorithm;
    atomic<int> total_requests_processed;
    
public:
    LoadBalancer(const string& config_file) : running(false), algorithm(0), total_requests_processed(0) {
        if (!load_config(config_file)) {
            throw runtime_error("Failed to load configuration");
        }
        
        // Create log files with timestamps
        auto now = system_clock::now();
        auto time_t_now = system_clock::to_time_t(now);
        stringstream timestamp;
        timestamp << put_time(localtime(&time_t_now), "%Y%m%d_%H%M%S");
        
        health_log.open("health_checks_" + timestamp.str() + ".log");
        request_log.open("requests_" + timestamp.str() + ".log");
        metrics_log.open("metrics_" + timestamp.str() + ".log");
        
        health_log << "timestamp,server_ip,server_port,healthy,response_time_ms,failed_checks" << endl;
        request_log << "timestamp,client_ip,server_ip,server_port,operation,filename,request_size" << endl;
        metrics_log << "timestamp,total_requests,healthy_servers,algorithm" << endl;
        
        cout << "Log files created with timestamp: " << timestamp.str() << endl;
    }
    
    ~LoadBalancer() {
        stop();
        if (health_log.is_open()) health_log.close();
        if (request_log.is_open()) request_log.close();
        if (metrics_log.is_open()) metrics_log.close();
    }
    
    bool load_config(const string& config_file) {
        ConfigParser parser;
        if (!parser.load(config_file)) {
            return false;
        }
        
        lb_ip = parser.getString("lb_ip", "127.0.0.1");
        lb_port = parser.getInt("lb_port", 8000);
        
        servers.clear();
        for (int i = 1; i <= 4; i++) {
            string ip_key = "server" + to_string(i) + "_ip";
            string port_key = "server" + to_string(i) + "_port";
            
            string server_ip = parser.getString(ip_key, "127.0.0.1");
            int server_port = parser.getInt(port_key, 9000 + i);
            
            // Use push_back instead of emplace_back to avoid move issues
            servers.push_back(BackendServer(server_ip, server_port));
        }
        
        return true;
    }
    
    void start() {
        running = true;
        
        // Start health check thread
        health_check_thread = thread(&LoadBalancer::health_check_loop, this);
        
        // Create load balancer socket
        int lb_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (lb_socket < 0) {
            throw runtime_error("Failed to create socket");
        }
        
        // Set socket options for reuse
        int opt = 1;
        if (setsockopt(lb_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            throw runtime_error("Failed to set socket options");
        }
        
        // Bind to address
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = inet_addr(lb_ip.c_str());
        address.sin_port = htons(lb_port);
        
        if (bind(lb_socket, (sockaddr*)&address, sizeof(address)) < 0) {
            throw runtime_error("Failed to bind socket to " + lb_ip + ":" + to_string(lb_port));
        }
        
        if (listen(lb_socket, 100) < 0) {
            throw runtime_error("Failed to listen on socket");
        }
        
        cout << "==========================================" << endl;
        cout << "Load Balancer Started Successfully" << endl;
        cout << "==========================================" << endl;
        cout << "LB Address: " << lb_ip << ":" << lb_port << endl;
        cout << "Algorithm: " << (algorithm == 0 ? "Weighted Response Time" : "Least Connections") << endl;
        cout << "Backend Servers:" << endl;
        for (const auto& server : servers) {
            cout << "  " << server.ip << ":" << server.port << endl;
        }
        cout << "==========================================" << endl;
        
        // Main accept loop
        while (running) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_socket = accept(lb_socket, (sockaddr*)&client_addr, &client_len);
            
            if (client_socket >= 0) {
                thread client_thread(&LoadBalancer::handle_client, this, client_socket);
                client_thread.detach();
            } else if (running) {
                cerr << "Accept failed" << endl;
            }
        }
        
        close(lb_socket);
    }
    
    void stop() {
        running = false;
        if (health_check_thread.joinable()) {
            health_check_thread.join();
        }
    }
    
    void set_algorithm(int algo) {
        if (algo == 0 || algo == 1) {
            algorithm = algo;
            cout << "Load balancing algorithm set to: " 
                 << (algorithm == 0 ? "Weighted Response Time" : "Least Connections") << endl;
        } else {
            cerr << "Invalid algorithm. Use 0 for Weighted Response Time or 1 for Least Connections" << endl;
        }
    }
    
private:
    void health_check_loop() {
        while (running) {
            vector<thread> health_threads;
            
            for (auto& server : servers) {
                health_threads.emplace_back(&LoadBalancer::check_server_health, this, ref(server));
            }
            
            for (auto& thread : health_threads) {
                thread.join();
            }
            
            // Log metrics
            log_metrics();
            
            // Sleep for exactly 1 second as required
            this_thread::sleep_for(seconds(1));
        }
    }
    
    void check_server_health(BackendServer& server) {
        auto start_time = steady_clock::now();
        
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            update_server_health(server, false, duration_cast<milliseconds>(steady_clock::now() - start_time).count());
            return;
        }
        
        // Set timeouts for health check
        timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        
        sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(server.port);
        inet_pton(AF_INET, server.ip.c_str(), &serv_addr.sin_addr);
        
        bool healthy = (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) == 0);
        
        if (healthy) {
            // Send health check message
            string health_msg = "HEALTH_CHECK\n";
            if (send(sock, health_msg.c_str(), health_msg.length(), 0) <= 0) {
                healthy = false;
            } else {
                // Wait for response
                char buffer[256];
                ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
                healthy = (bytes_received > 0);
                
                if (healthy) {
                    buffer[bytes_received] = '\0';
                    healthy = (string(buffer).find("HEALTH_OK") != string::npos);
                }
            }
        }
        
        close(sock);
        
        auto response_time = duration_cast<milliseconds>(steady_clock::now() - start_time).count();
        update_server_health(server, healthy, response_time);
    }
    
    void update_server_health(BackendServer& server, bool healthy, long long response_time) {
        if (!healthy) {
            server.failed_health_checks++;
        } else {
            server.failed_health_checks = 0;
        }
        
        server.healthy = healthy;
        server.last_health_check = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        
        // Update server load metric based on response time
        if (healthy && response_time > 0) {
            // Lower response time = higher load score (better performance)
            server.current_load = 1000.0 / response_time;
        } else {
            server.current_load = 0.0;
        }
        
        // Log health check
        lock_guard<mutex> lock(log_mutex);
        auto now = system_clock::now();
        auto timestamp = duration_cast<milliseconds>(now.time_since_epoch()).count();
        
        health_log << timestamp << "," 
                   << server.ip << "," 
                   << server.port << "," 
                   << (healthy ? "true" : "false") << "," 
                   << response_time << ","
                   << server.failed_health_checks << endl;
    }
    
    void log_metrics() {
        lock_guard<mutex> lock(log_mutex);
        auto now = system_clock::now();
        auto timestamp = duration_cast<milliseconds>(now.time_since_epoch()).count();
        
        int healthy_count = 0;
        for (const auto& server : servers) {
            if (server.healthy) healthy_count++;
        }
        
        metrics_log << timestamp << ","
                   << total_requests_processed << ","
                   << healthy_count << ","
                   << algorithm << endl;
    }
    
    BackendServer* select_server() {
        vector<BackendServer*> healthy_servers;
        
        for (auto& server : servers) {
            if (server.healthy) {
                healthy_servers.push_back(&server);
            }
        }
        
        if (healthy_servers.empty()) {
            cerr << "No healthy backend servers available!" << endl;
            return nullptr;
        }
        
        if (algorithm == 0) {
            return select_weighted_response_time(healthy_servers);
        } else {
            return select_least_connections(healthy_servers);
        }
    }
    
    BackendServer* select_weighted_response_time(vector<BackendServer*>& healthy_servers) {
        BackendServer* best_server = healthy_servers[0];
        double best_score = best_server->current_load;
        
        for (size_t i = 1; i < healthy_servers.size(); i++) {
            double score = healthy_servers[i]->current_load;
            if (score > best_score) {
                best_score = score;
                best_server = healthy_servers[i];
            }
        }
        
        return best_server;
    }
    
    BackendServer* select_least_connections(vector<BackendServer*>& healthy_servers) {
        BackendServer* best_server = healthy_servers[0];
        int min_connections = best_server->active_connections;
        
        for (size_t i = 1; i < healthy_servers.size(); i++) {
            if (healthy_servers[i]->active_connections < min_connections) {
                min_connections = healthy_servers[i]->active_connections;
                best_server = healthy_servers[i];
            }
        }
        
        return best_server;
    }
    
    void handle_client(int client_socket) {
        char buffer[8192];
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            close(client_socket);
            return;
        }
        
        buffer[bytes_received] = '\0';
        string request(buffer);
        
        // Get client information for logging
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        getpeername(client_socket, (sockaddr*)&client_addr, &addr_len);
        string client_ip = inet_ntoa(client_addr.sin_addr);
        
        // Select backend server using load balancing algorithm
        BackendServer* selected_server = select_server();
        
        if (!selected_server) {
            string error_msg = "ERROR: No healthy backend servers available\n";
            send(client_socket, error_msg.c_str(), error_msg.length(), 0);
            close(client_socket);
            return;
        }
        
        // Forward request to selected backend server
        forward_request(client_socket, request, client_ip, *selected_server);
    }
    
    void forward_request(int client_socket, const string& request, const string& client_ip, BackendServer& server) {
        int backend_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (backend_socket < 0) {
            string error_msg = "ERROR: Failed to create connection to backend\n";
            send(client_socket, error_msg.c_str(), error_msg.length(), 0);
            close(client_socket);
            return;
        }
        
        // Connect to backend server
        sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(server.port);
        inet_pton(AF_INET, server.ip.c_str(), &serv_addr.sin_addr);
        
        if (connect(backend_socket, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            string error_msg = "ERROR: Backend server connection failed\n";
            send(client_socket, error_msg.c_str(), error_msg.length(), 0);
            close(client_socket);
            close(backend_socket);
            return;
        }
        
        // Update server metrics
        server.active_connections++;
        server.total_requests++;
        total_requests_processed++;
        
        // Parse request for logging
        string operation = "UNKNOWN";
        string filename = "UNKNOWN";
        size_t request_size = request.size();
        
        istringstream iss(request);
        string command;
        iss >> command >> filename;
        
        if (command == "PUT") {
            operation = "UPLOAD";
        } else if (command == "GET") {
            operation = "DOWNLOAD";
        }
        
        // Log the request
        {
            lock_guard<mutex> lock(log_mutex);
            auto now = system_clock::now();
            auto timestamp = duration_cast<milliseconds>(now.time_since_epoch()).count();
            
            request_log << timestamp << "," 
                       << client_ip << "," 
                       << server.ip << "," 
                       << server.port << "," 
                       << operation << "," 
                       << filename << ","
                       << request_size << endl;
        }
        
        cout << "[" << operation << "] " << filename << " -> " 
             << server.ip << ":" << server.port 
             << " (Connections: " << server.active_connections << ")" << endl;
        
        // Forward the request to backend
        if (send(backend_socket, request.c_str(), request.length(), 0) <= 0) {
            cerr << "Failed to send request to backend" << endl;
        } else {
            // Relay data between client and backend
            relay_data(client_socket, backend_socket);
        }
        
        // Cleanup
        server.active_connections--;
        close(backend_socket);
        close(client_socket);
    }
    
    void relay_data(int client_socket, int backend_socket) {
        fd_set readfds;
        char buffer[8192];
        
        while (true) {
            FD_ZERO(&readfds);
            FD_SET(client_socket, &readfds);
            FD_SET(backend_socket, &readfds);
            
            int max_fd = max(client_socket, backend_socket);
            int activity = select(max_fd + 1, &readfds, nullptr, nullptr, nullptr);
            
            if (activity < 0) {
                break;
            }
            
            // Data from client to backend
            if (FD_ISSET(client_socket, &readfds)) {
                ssize_t bytes = recv(client_socket, buffer, sizeof(buffer), 0);
                if (bytes <= 0) break;
                if (send(backend_socket, buffer, bytes, 0) <= 0) break;
            }
            
            // Data from backend to client
            if (FD_ISSET(backend_socket, &readfds)) {
                ssize_t bytes = recv(backend_socket, buffer, sizeof(buffer), 0);
                if (bytes <= 0) break;
                if (send(client_socket, buffer, bytes, 0) <= 0) break;
            }
        }
    }
};

int main(int argc, char* argv[]) {
    cout << "Load Balancer Starting..." << endl;
    
    try {
        LoadBalancer lb("config.json");
        
        // Parse command line arguments
        int algorithm = 0; // Default: Weighted Response Time
        for (int i = 1; i < argc; i++) {
            string arg = argv[i];
            if (arg == "--algorithm" || arg == "-a") {
                if (i + 1 < argc) {
                    string algo = argv[++i];
                    if (algo == "weighted" || algo == "0") {
                        algorithm = 0;
                    } else if (algo == "least_connections" || algo == "1") {
                        algorithm = 1;
                    } else {
                        cerr << "Unknown algorithm: " << algo << endl;
                        cerr << "Use: weighted (0) or least_connections (1)" << endl;
                        return 1;
                    }
                }
            } else if (arg == "--help" || arg == "-h") {
                cout << "Usage: " << argv[0] << " [options]" << endl;
                cout << "Options:" << endl;
                cout << "  --algorithm, -a <algorithm>  Set load balancing algorithm" << endl;
                cout << "                               (weighted/0 or least_connections/1)" << endl;
                cout << "  --help, -h                   Show this help message" << endl;
                return 0;
            }
        }
        
        lb.set_algorithm(algorithm);
        lb.start();
        
    } catch (const exception& e) {
        cerr << "Fatal Error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}