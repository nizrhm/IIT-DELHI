#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <atomic>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <filesystem>
#include <iomanip>

using namespace std;
namespace fs = filesystem;

class BackendServer {
private:
    string ip;
    int port;
    atomic<bool> running;
    string storage_dir;
    atomic<long> total_requests;
    ofstream server_log;
    
public:
    BackendServer(const string& ip_addr, int port_num) 
        : ip(ip_addr), port(port_num), running(false), total_requests(0) {
        storage_dir = "server_storage_" + to_string(port);
        fs::create_directories(storage_dir);
        
        // Create server log
        auto now = chrono::system_clock::now();
        auto time_t_now = chrono::system_clock::to_time_t(now);
        stringstream timestamp;
        timestamp << put_time(localtime(&time_t_now), "%Y%m%d_%H%M%S");
        server_log.open("server_" + to_string(port) + "_" + timestamp.str() + ".log");
        server_log << "timestamp,operation,filename,result,client_ip" << endl;
    }
    
    ~BackendServer() {
        if (server_log.is_open()) server_log.close();
    }
    
    void start() {
        running = true;
        
        int server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            throw runtime_error("Failed to create socket");
        }
        
        // Set socket options
        int opt = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            throw runtime_error("Failed to set socket options");
        }
        
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = inet_addr(ip.c_str());
        address.sin_port = htons(port);
        
        if (bind(server_socket, (sockaddr*)&address, sizeof(address)) < 0) {
            throw runtime_error("Failed to bind socket to " + ip + ":" + to_string(port));
        }
        
        if (listen(server_socket, 100) < 0) {
            throw runtime_error("Failed to listen on socket");
        }
        
        cout << "Backend Server started on " << ip << ":" << port << endl;
        cout << "Storage directory: " << storage_dir << endl;
        
        while (running) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
            
            if (client_socket >= 0) {
                thread client_thread(&BackendServer::handle_client, this, client_socket, client_addr);
                client_thread.detach();
            }
        }
        
        close(server_socket);
    }
    
    void stop() {
        running = false;
    }
    
private:
    void log_request(const string& operation, const string& filename, const string& result, const string& client_ip) {
        auto now = chrono::system_clock::now();
        auto timestamp = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();
        
        server_log << timestamp << "," 
                   << operation << "," 
                   << filename << "," 
                   << result << "," 
                   << client_ip << endl;
    }
    
    void handle_client(int client_socket, sockaddr_in client_addr) {
        char buffer[8192];
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            close(client_socket);
            return;
        }
        
        buffer[bytes_received] = '\0';
        string request(buffer);
        
        string client_ip = inet_ntoa(client_addr.sin_addr);
        total_requests++;
        
        // Handle health check
        if (request.find("HEALTH_CHECK") != string::npos) {
            string response = "HEALTH_OK\n";
            send(client_socket, response.c_str(), response.length(), 0);
            log_request("HEALTH_CHECK", "", "SUCCESS", client_ip);
            close(client_socket);
            return;
        }
        
        // Parse command
        istringstream iss(request);
        string command, filename;
        iss >> command >> filename;
        
        if (command == "PUT") {
            handle_put(client_socket, filename, request.substr(iss.tellg()), client_ip);
        } else if (command == "GET") {
            handle_get(client_socket, filename, client_ip);
        } else {
            string error_msg = "ERROR: Unknown command\n";
            send(client_socket, error_msg.c_str(), error_msg.length(), 0);
            log_request("UNKNOWN", "", "ERROR", client_ip);
        }
        
        close(client_socket);
    }
    
    void handle_put(int client_socket, const string& filename, const string& filedata, const string& client_ip) {
        if (filename.empty()) {
            string error_msg = "ERROR: No filename specified\n";
            send(client_socket, error_msg.c_str(), error_msg.length(), 0);
            log_request("PUT", "", "ERROR_NO_FILENAME", client_ip);
            return;
        }
        
        string filepath = storage_dir + "/" + filename;
        ofstream file(filepath);
        
        if (!file.is_open()) {
            string error_msg = "ERROR: Cannot create file\n";
            send(client_socket, error_msg.c_str(), error_msg.length(), 0);
            log_request("PUT", filename, "ERROR_CREATE_FILE", client_ip);
            return;
        }
        
        file << filedata;
        file.close();
        
        string success_msg = "OK: File '" + filename + "' uploaded successfully\n";
        send(client_socket, success_msg.c_str(), success_msg.length(), 0);
        
        cout << "File uploaded: " << filepath << " (from " << client_ip << ")" << endl;
        log_request("PUT", filename, "SUCCESS", client_ip);
    }
    
    void handle_get(int client_socket, const string& filename, const string& client_ip) {
        if (filename.empty()) {
            string error_msg = "ERROR: No filename specified\n";
            send(client_socket, error_msg.c_str(), error_msg.length(), 0);
            log_request("GET", "", "ERROR_NO_FILENAME", client_ip);
            return;
        }
        
        string filepath = storage_dir + "/" + filename;
        ifstream file(filepath);
        
        if (!file.is_open()) {
            string error_msg = "ERROR: File not found\n";
            send(client_socket, error_msg.c_str(), error_msg.length(), 0);
            log_request("GET", filename, "ERROR_NOT_FOUND", client_ip);
            return;
        }
        
        stringstream file_content;
        file_content << file.rdbuf();
        string response = "OK:\n" + file_content.str();
        
        send(client_socket, response.c_str(), response.length(), 0);
        
        cout << "File downloaded: " << filepath << " (by " << client_ip << ")" << endl;
        log_request("GET", filename, "SUCCESS", client_ip);
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <ip> <port>" << endl;
        cerr << "Example: " << argv[0] << " 127.0.0.1 9001" << endl;
        return 1;
    }
    
    string ip = argv[1];
    int port = stoi(argv[2]);
    
    try {
        BackendServer server(ip, port);
        server.start();
    } catch (const exception& e) {
        cerr << "Server Error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}