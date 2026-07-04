#ifndef TASK_H
#define TASK_H
#include <iostream>
#include <functional>
#include <string>
#include <iomanip>
#include <deque>
#include <vector>

enum TaskType {
    CPU_BOUND,
    MEMORY_BOUND,
    MIXED
};

struct Message {
    int sender_id;
    std::string content;
};

enum class BurstType { CPU, IO };

struct TaskBurst {
    BurstType type;
    int duration;
    int remaining;
};

using namespace std;

class Task {
private:
    int id;
    std::function<void()> func;
    int priority;
    std::deque<TaskBurst> bursts;
    int total_work;
    string state;
    bool isDeferred;
    bool is_buggy = false; // NEW: For simulating stalls
    TaskType tasktype;
    int arrival_time;
    int waiting_time = 0;
public:
    int mlfq_level = 0; // 0=High, 1=Medium, 2=Low
    int current_core = -1;
    int finished_by_core = -1;

    // Resource Management (Banker's Algorithm)
    std::vector<int> max_demand;
    std::vector<int> allocation;

    // Starvation & Leak Detection
    int simulated_memory_used = 0;
    int last_run_tick = 0;
    double last_progress = 0.0;
    int stall_counter = 0;
    int last_logged_tick = -1; // For suppressing redundant logs

    Task(int id, std::function<void()> f, TaskType type, int priority, int cpu, int io, int work, int arrival)
        : id(id), func(f), priority(priority), state("Ready"), isDeferred(false), tasktype(type), arrival_time(arrival), waiting_time(0), current_core(-1) {
        
        // Default 3 resource types for student project
        max_demand = {3, 3, 2}; // Random default
        allocation = {0, 0, 0};
        
        // Backward compatibility: create a simple sequence
        if (cpu > 0) bursts.push_back({BurstType::CPU, cpu, cpu});
        if (io > 0) bursts.push_back({BurstType::IO, io, io});
        if (work > 0) bursts.push_back({BurstType::CPU, work, work});
        
        total_work = 0;
        for (const auto& b : bursts) total_work += b.duration;
        simulated_memory_used = (id % 3 == 0) ? (id * 5) : 0;
        last_run_tick = arrival;
    }

    Task(int id, std::function<void()> f, TaskType type, int priority, const std::vector<TaskBurst>& burst_list, int arrival)
        : id(id), func(f), priority(priority), bursts(burst_list.begin(), burst_list.end()), state("Ready"), isDeferred(false), tasktype(type), arrival_time(arrival), waiting_time(0), current_core(-1) {
        
        max_demand = {3, 3, 2}; // Default for now
        allocation = {0, 0, 0};
        total_work = 0;
        for (const auto& b : bursts) total_work += b.duration;
        simulated_memory_used = (id % 2 == 0) ? (id * 10) : 15;
        last_run_tick = arrival;
    }

    void execute() {
        if (isDeferred) return;
        
        if (bursts.empty()) {
            setstate("Finished");
            return;
        }

        auto& current = bursts.front();
        
        // STALL SIMULATION: If buggy, skip progress 100% of the time for testing
        if (is_buggy) {
            setstate("Running"); 
            return;
        }

        current.remaining--;

        if (current.type == BurstType::CPU) {
            if (func) func();
            setstate("Running");
        } else {
            setstate("Waiting");
        }

        if (current.remaining <= 0) {
            bursts.pop_front();
            if (bursts.empty()) {
                setstate("Finished");
            } else {
                setstate("Ready");
            }
        }
    }

    void incrementWaitingTime() { waiting_time++; }
    void setPriority(int t) { priority = t; }
    int getpriority() const { return priority; }
    bool operator<(const Task& other) const { return priority < other.priority; }
    int getid() const { return id; }
    
    double get_progress() const {
        if (state == "Finished") return 100.0;
        if (total_work <= 0) return 0.0;
        
        int remaining_total = 0;
        for (const auto& b : bursts) remaining_total += b.remaining;
        
        double prog = 100.0 * (1.0 - (double)remaining_total / total_work);
        return (prog > 100.0) ? 100.0 : ((prog < 0.0) ? 0.0 : prog);
    }

    void setstate(string t) { state = t; }
    string getstate() { return state; }
    
    int getcpu() { 
        if (!bursts.empty() && bursts.front().type == BurstType::CPU) return bursts.front().remaining;
        return 0; 
    }
    
    int getio() { 
        if (!bursts.empty() && bursts.front().type == BurstType::IO) return bursts.front().remaining;
        return 0; 
    }

    bool getDeferred() { return isDeferred; }
    void setDeferred(bool v) { isDeferred = v; }
    TaskType gettypetask() { return tasktype; }
    int get_arrival_time() { return arrival_time; }
    int getWaitingTime() { return waiting_time; }
    void resetWaitingTime() { waiting_time = 0; }
    int getCore() { return current_core; }
    void makeBuggy() { is_buggy = true; } // NEW: Inject fault
};

#endif