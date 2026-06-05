#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <csignal>
#include <fstream>
#include <filesystem>
using namespace std;
namespace fs = std::filesystem;
using steady = chrono::steady_clock;
using time_point = chrono::time_point<steady>;

// ---------------- CONFIG ----------------
struct Config {
    string server_ip;
    int server_port;
    int server_threads;
    int client_threads;
};

Config read_config(const string &path="config.json") {
    ifstream f(path);
    if(!f.is_open()){ cerr<<"Cannot open config.json\n"; exit(1);}
    string s((istreambuf_iterator<char>(f)),istreambuf_iterator<char>());
    auto get_str=[&](string key){ regex r("\""+key+"\"\\s*:\\s*\"([^\"]+)\""); smatch m;
        if(regex_search(s,m,r)) return m[1].str();
        cerr<<"Missing "<<key<<"\n"; exit(1);
    };
    auto get_int=[&](string key){ regex r("\""+key+"\"\\s*:\\s*([0-9]+)"); smatch m;
        if(regex_search(s,m,r)) return stoi(m[1]);
        cerr<<"Missing "<<key<<"\n"; exit(1);
    };
    return Config{get_str("server_ip"), get_int("server_port"), get_int("server_threads"), get_int("client_threads")};
}

// ---------------- REQUEST ----------------
struct Request {
    int id;
    int client_fd;
    size_t job_size;
    time_point arrival,start,finish;
    string command;
    double waiting=0, response=0;
};

enum SchedPolicy{FCFS,SJF,RR};
mutex q_mutex; condition_variable q_cv;
deque<Request*> req_queue;
bool done=false;
SchedPolicy policy=FCFS;
int quantum=0;
int packet_lines=1;

vector<Request*> completed;
mutex comp_mutex;
mutex log_mutex; // serialize console/file logging
int server_fd = -1;
string base_dir = "./server_files";

// ---------------- HELPERS ----------------
double ms_between(time_point a,time_point b){
    return chrono::duration_cast<chrono::microseconds>(b-a).count()/1000.0;
}

ssize_t recv_all(int fd,char* buf,size_t len){
    size_t rec=0;
    while(rec<len){
        ssize_t n=recv(fd,buf+rec,len-rec,0);
        if(n<=0) break;
        rec+=n;
    }
    return rec;
}

ssize_t send_all(int fd,const char* buf,size_t len){
    size_t sent=0;
    while(sent<len){
        ssize_t n=send(fd,buf+sent,len-sent,0);
        if(n<=0) break;
        sent+=n;
    }
    return sent;
}

// ---------------- PROCESS REQUEST ----------------
void process_request(Request* r){
    r->start = steady::now();
    r->waiting = ms_between(r->arrival, r->start);

    istringstream iss(r->command);
    string op,fname; iss >> op >> fname;

    {
        lock_guard<mutex> lg(log_mutex);
        cout << "[Request #" << r->id << "] START processing: " << r->command
             << " | Waiting Time: " << fixed << setprecision(3) << r->waiting << " ms\n";
    }

    if(op == "PUT"){
        fs::path dest_path = fname;
        if(!dest_path.has_parent_path())
            dest_path = fs::path(base_dir) / dest_path;

        ofstream out(dest_path, ios::binary);
        if(!out){
            string reply="ERROR: cannot open file\n";
            send_all(r->client_fd, reply.c_str(), reply.size());
            close(r->client_fd);
            return;
        }

        size_t fsize=0;
        if(recv_all(r->client_fd,(char*)&fsize,sizeof(fsize))!=sizeof(fsize)){
            close(r->client_fd); return;
        }

        char buf[4096]; size_t received=0;
        while(received<fsize){
            size_t chunk=min(sizeof(buf), fsize-received);
            ssize_t n=recv_all(r->client_fd, buf, chunk);
            if(n<=0) break;
            out.write(buf,n);
            received+=n;
        }
        out.close();

        string reply="OK\n"; send_all(r->client_fd,reply.c_str(),reply.size());
    }
    else if(op == "GET"){
        fs::path full_path = fname;
        if(!full_path.has_parent_path())
            full_path = fs::path(base_dir) / full_path;

        ifstream in(full_path, ios::binary);
        if(!in){
            string reply="ERROR: file not found\n"; send_all(r->client_fd,reply.c_str(),reply.size());
            close(r->client_fd);
            return;
        }

        in.seekg(0, ios::end); size_t fsize=in.tellg(); in.seekg(0, ios::beg);
        send_all(r->client_fd,(char*)&fsize,sizeof(fsize));

        char buf[4096]; size_t sent=0;
        while(sent<fsize){
            size_t chunk=min(sizeof(buf), fsize-sent);
            in.read(buf, chunk);
            send_all(r->client_fd, buf, in.gcount());
            sent+=in.gcount();
        }
        in.close();
    }
    else{
        string reply="ERROR: unknown command\n"; send_all(r->client_fd,reply.c_str(),reply.size());
    }

    r->finish=steady::now();
    close(r->client_fd);
    r->response = ms_between(r->arrival, r->finish);

    {
        lock_guard<mutex> lg(log_mutex);
        cout << "[Request #" << r->id << "] FINISHED " << r->command
             << " | Response Time: " << fixed << setprecision(3) << r->response
             << " ms (Waiting: " << r->waiting << " ms)\n";
    }

    {
        ofstream log("server_log.csv", ios::app);
        log << r->id << "," << r->command << ","
            << chrono::duration_cast<chrono::microseconds>(r->arrival.time_since_epoch()).count() << ","
            << chrono::duration_cast<chrono::microseconds>(r->start.time_since_epoch()).count() << ","
            << chrono::duration_cast<chrono::microseconds>(r->finish.time_since_epoch()).count() << ","
            << fixed << setprecision(3) << r->waiting << "," << r->response << "\n";
    }

    { lock_guard<mutex> lg(comp_mutex); completed.push_back(r); }
}

// ---------------- WORKER THREAD ----------------
void worker_thread_func(int tid){
    while(true){
        Request* r=nullptr;
        { 
            unique_lock<mutex> lk(q_mutex);
            q_cv.wait(lk,[&]{ return !req_queue.empty() || done; });
            if(done && req_queue.empty()) return;

            if(policy==FCFS){ r=req_queue.front(); req_queue.pop_front();}
            else if(policy==SJF){
                auto it=min_element(req_queue.begin(),req_queue.end(),
                    [](auto*a,auto*b){return a->job_size<b->job_size;});
                r=*it; req_queue.erase(it);
            } else if(policy==RR){
                r=req_queue.front(); req_queue.pop_front();
            }
        }
        process_request(r);
    }
}

// ---------------- SUMMARY ----------------
void print_summary() {
    lock_guard<mutex> lg(log_mutex);
    if(completed.empty()) {
        cout << "\n[Summary] No requests processed.\n";
        return;
    }

    double total_wait = 0, total_resp = 0;
    for(auto *r : completed) {
        total_wait += r->waiting;
        total_resp += r->response;
    }
    double avg_wait = total_wait / completed.size();
    double avg_resp = total_resp / completed.size();

    cout << "\n========== SERVER SUMMARY ==========\n";
    cout << "Total Requests: " << completed.size() << "\n";
    cout << "Average Waiting Time: " << fixed << setprecision(3) << avg_wait << " ms\n";
    cout << "Average Response Time: " << fixed << setprecision(3) << avg_resp << " ms\n";
    cout << "====================================\n";
}

// ---------------- SIGNAL ----------------
void handle_signal(int sig){
    { lock_guard<mutex> lg(log_mutex);
        cout << "\n[Server] Caught signal ("<<sig<<"). Shutting down...\n"; }
    done = true;
    q_cv.notify_all();
    if(server_fd != -1) close(server_fd);
}

// ---------------- MAIN ----------------
int main(int argc,char*argv[]){
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    string sched_str="fcfs";
    for(int i=1;i<argc;i++){
        string a=argv[i];
        if(a=="--sched"&&i+1<argc) sched_str=argv[++i];
        else if(a=="--quantum"&&i+1<argc) quantum=stoi(argv[++i]);
        else if(a=="--file"&&i+1<argc) base_dir=argv[++i];
        else if(a=="--p"&&i+1<argc) packet_lines=stoi(argv[++i]);
    }

    if(sched_str=="fcfs") policy=FCFS;
    else if(sched_str=="sjf") policy=SJF;
    else if(sched_str=="rr") policy=RR;
    else { cerr<<"Invalid --sched\n"; exit(1); }
    if(policy==RR && quantum<=0){ cerr<<"RR requires --quantum\n"; exit(1); }

    fs::create_directories(base_dir);
    Config cfg = read_config();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd<0){ perror("socket"); exit(1); }
    int opt=1; setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(cfg.server_port);
    addr.sin_addr.s_addr = inet_addr(cfg.server_ip.c_str());
    if(bind(server_fd,(sockaddr*)&addr,sizeof(addr))<0){ perror("bind"); exit(1);}
    if(listen(server_fd,128)<0){ perror("listen"); exit(1);}
    cout<<"[Server] listening "<<cfg.server_ip<<":"<<cfg.server_port
        <<" sched="<<sched_str<<" p="<<packet_lines<<" threads="<<cfg.server_threads<<"\n";

    vector<thread> workers;
    for(int i=0;i<cfg.server_threads;i++) workers.emplace_back(worker_thread_func,i);

    int req_id=0;
    while(!done){
        sockaddr_in client_addr{}; socklen_t len=sizeof(client_addr);
        int client_fd = accept(server_fd,(sockaddr*)&client_addr,&len);
        if(client_fd<0){ if(done) break; perror("accept"); continue; }

        { lock_guard<mutex> lg(log_mutex);
            cout << "[+] New connection from " << inet_ntoa(client_addr.sin_addr)
                 << ":" << ntohs(client_addr.sin_port) << "\n";
        }

        string cmd; char c; bool valid=true;
        while(true){
            ssize_t n=recv(client_fd,&c,1,0);
            if(n<=0){ close(client_fd); valid=false; break; }
            if(c=='\n') break;
            cmd += c;
        }
        if(!valid) continue;

        auto *r = new Request{req_id++, client_fd, cmd.size(), steady::now(), {}, {}, cmd};
        { lock_guard<mutex> lg(q_mutex); req_queue.push_back(r); }
        q_cv.notify_one();
        { lock_guard<mutex> lg(log_mutex); cout << "[Request #" << r->id << "] ARRIVED: " << r->command << "\n"; }
    }

    q_cv.notify_all();
    for(auto &t:workers) t.join();

    print_summary();
    cout << "[Server] Shutdown complete.\n";
    return 0;
}