#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "Usage: " << argv[0] << " <put|get> <filename>" << endl;
        return 1;
    }
    
    string command = argv[1];
    string filename = argv[2];
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8000);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }
    
    if (command == "PUT") {
        ifstream file(filename);
        if (!file) {
            cerr << "Cannot open file: " << filename << endl;
            close(sock);
            return 1;
        }
        
        stringstream content;
        content << file.rdbuf();
        string request = "PUT " + filename + "\n" + content.str();
        
        send(sock, request.c_str(), request.size(), 0);
        
        char response[4096];
        ssize_t bytes = recv(sock, response, sizeof(response)-1, 0);
        if (bytes > 0) {
            response[bytes] = '\0';
            cout << response;
        }
    } else if (command == "GET") {
        string request = "GET " + filename + "\n";
        send(sock, request.c_str(), request.size(), 0);
        
        char response[8192];
        ssize_t bytes = recv(sock, response, sizeof(response)-1, 0);
        if (bytes > 0) {
            response[bytes] = '\0';
            string response_str(response);
            if (response_str.find("OK:") == 0) {
                size_t pos = response_str.find("\n") + 1;
                string content = response_str.substr(pos);
                ofstream out("downloaded_" + filename);
                out << content;
                cout << "File downloaded as downloaded_" << filename << endl;
            } else {
                cout << response_str;
            }
        }
    } else {
        cerr << "Unknown command: " << command << endl;
    }
    
    close(sock);
    return 0;
}