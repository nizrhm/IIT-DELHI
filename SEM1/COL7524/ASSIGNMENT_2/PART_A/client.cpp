#include <bits/stdc++.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <filesystem>
using namespace std;
namespace fs = std::filesystem;

struct Config {
    string server_ip;
    int server_port;
    int server_threads;
    int client_threads;
};

Config read_config(const string &path="config.json") {
    ifstream f(path);
    if(!f.is_open()){ cerr<<"Cannot open config.json\n"; exit(1); }
    string s((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    auto get_str=[&](string key){ regex r("\""+key+"\"\\s*:\\s*\"([^\"]+)\""); smatch m;
        if(regex_search(s,m,r)) return m[1].str(); cerr<<"Missing "<<key<<"\n"; exit(1);
    };
    auto get_int=[&](string key){ regex r("\""+key+"\"\\s*:\\s*([0-9]+)"); smatch m;
        if(regex_search(s,m,r)) return stoi(m[1]); cerr<<"Missing "<<key<<"\n"; exit(1);
    };
    return Config{get_str("server_ip"), get_int("server_port"), get_int("server_threads"), get_int("client_threads")};
}

bool send_all(int fd, const char* buf, size_t len) {
    size_t sent = 0;
    while(sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if(n <= 0) return false;
        sent += n;
    }
    return true;
}

ssize_t recv_all(int fd, char* buf, size_t len) {
    size_t rec = 0;
    while(rec < len) {
        ssize_t n = recv(fd, buf + rec, len - rec, 0);
        if(n <= 0) return -1;
        rec += n;
    }
    return rec;
}

void do_put(const string &local, const string &remote, const Config &cfg, int tid=-1) {
    ifstream f(local, ios::binary);
    if(!f) { cout << "[T" << tid << "] Local file not found: " << local << "\n"; return; }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg.server_port);
    addr.sin_addr.s_addr = inet_addr(cfg.server_ip.c_str());
    if(connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[PUT connect]");
        return;
    }

    string cmd = "PUT " + remote + "\n";
    send_all(sock, cmd.c_str(), cmd.size());

    f.seekg(0, ios::end);
    size_t fsize = f.tellg();
    f.seekg(0, ios::beg);
    send_all(sock, (char*)&fsize, sizeof(fsize));

    char buf[4096];
    while(f) {
        f.read(buf, sizeof(buf));
        send_all(sock, buf, f.gcount());
    }
    f.close();

    char reply[128];
    ssize_t n = recv(sock, reply, sizeof(reply) - 1, 0);
    if(n > 0) {
        reply[n] = 0;
        cout << "[T" << tid << "] [PUT] Server: " << reply;
    }
    close(sock);
}

void do_get(const string &remote, const string &local, const Config &cfg, int tid=-1) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg.server_port);
    addr.sin_addr.s_addr = inet_addr(cfg.server_ip.c_str());
    if(connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[GET connect]");
        return;
    }

    string cmd = "GET " + remote + "\n";
    send_all(sock, cmd.c_str(), cmd.size());

    size_t fsize;
    if(recv_all(sock, (char*)&fsize, sizeof(fsize)) != sizeof(fsize)) {
        cout << "[T" << tid << "] File not found on server: " << remote << "\n";
        close(sock);
        return;
    }

    fs::path dest(local);
    if(fs::exists(dest) && fs::is_directory(dest)) {
        dest /= fs::path(remote).filename();
    } else if(!dest.has_parent_path()) {
        // if local is just a filename without path, put in current directory
        dest = fs::current_path() / dest;
    }

    fs::create_directories(dest.parent_path());

    ofstream out(dest, ios::binary);
    if(!out) { cout << "[T" << tid << "] Cannot open local file for writing: " << dest << "\n"; close(sock); return; }

    char buf[4096];
    size_t rec = 0;
    while(rec < fsize) {
        size_t chunk = min(sizeof(buf), fsize - rec);
        ssize_t n = recv_all(sock, buf, chunk);
        if(n <= 0) break;
        out.write(buf, n);
        rec += n;
    }
    out.close();
    cout << "[T" << tid << "] [GET] Received " << dest << " (" << rec << " bytes)\n";
    close(sock);
}

// ---------------- BENCH ----------------
void bench_thread_func(const string &workdir, int ops_per_thread, const Config &cfg, int tid) {
    random_device rd; mt19937 gen(rd());
    vector<fs::path> files;

    for(auto &p: fs::directory_iterator(workdir)) {
        if(fs::is_regular_file(p.path())) files.push_back(p.path());
    }
    if(files.empty()) {
        cout << "[T" << tid << "] No files found in " << workdir << "\n";
        return;
    }

    uniform_int_distribution<> dis(0, files.size() - 1);
    fs::path bench_download_dir = "bench_downloads";
    fs::create_directories(bench_download_dir);

    for(int i=0; i<ops_per_thread; i++) {
        int idx = dis(gen);
        fs::path local = files[idx];
        string remote = "bench_t" + to_string(tid) + "_" + local.filename().string();
        if(i % 2 == 0)
            do_put(local.string(), remote, cfg, tid);
        else
            do_get(remote, (bench_download_dir / local.filename()).string(), cfg, tid);
    }
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        cout << "Usage:\n"
             << " ./client PUT <local> <remote>\n"
             << " ./client GET <remote> <local_or_dir>\n"
             << " ./client BENCH <workdir> <ops_per_thread>\n";
        return 1;
    }

    Config cfg = read_config();
    string cmd = argv[1];

    if(cmd == "PUT" && argc == 4)
        do_put(argv[2], argv[3], cfg);
    else if(cmd == "GET" && argc == 4)
        do_get(argv[2], argv[3], cfg);
    else if(cmd == "BENCH" && argc == 4) {
        string workdir = argv[2];
        int ops = stoi(argv[3]);
        vector<thread> threads;
        cout << "[BENCH] Starting " << cfg.client_threads << " threads × " << ops << " ops each\n";
        auto start = chrono::steady_clock::now();

        for(int i=0; i<cfg.client_threads; i++)
            threads.emplace_back(bench_thread_func, workdir, ops, cref(cfg), i);
        for(auto &t : threads) t.join();

        auto end = chrono::steady_clock::now();
        double total = chrono::duration_cast<chrono::milliseconds>(end - start).count();
        cout << "[BENCH] Completed in " << total << " ms (" 
             << cfg.client_threads << " threads × " << ops << " ops)\n";
    }
    else {
        cout << "Usage:\n"
             << " ./client PUT <local> <remote>\n"
             << " ./client GET <remote> <local_or_dir>\n"
             << " ./client BENCH <workdir> <ops_per_thread>\n";
    }
}
