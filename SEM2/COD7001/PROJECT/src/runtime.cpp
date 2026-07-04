#include "runtime.h"
#include "../workloads/workload.h"
#include "Metrics.h"
#include "monitor.h"
#include "scheduler.h"
#include "task.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>


using namespace std;

function<void()> Runtime::getTaskFunction(const string &name) {
  if (name == "cpuTask")
    return cpuTask;
  if (name == "memoryTask")
    return memoryTask;
  if (name == "mixedTask")
    return mixedTask;
  if (name == "phaseChangeTask")
    return phaseChangeTask;
  return nullptr;
}

TaskType Runtime::getTaskType(const std::string &type) {
  if (type == "CPU_BOUND")
    return CPU_BOUND;
  if (type == "MEMORY_BOUND")
    return MEMORY_BOUND;
  if (type == "MIXED")
    return MIXED;
  return CPU_BOUND;
}

vector<TaskBurst> Runtime::parseburst(string &s) {
  vector<TaskBurst> bursts;
  string standardized = s;
  replace(standardized.begin(), standardized.end(), ',', ';');

  stringstream ss(standardized);
  string segment;
  while (getline(ss, segment, ';')) {
    if (segment.empty())
      continue;
    char c = segment[0];
    try {
      int duration = stoi(segment.substr(1));
      BurstType type = (c == 'C' || c == 'c') ? BurstType::CPU : BurstType::IO;
      bursts.push_back({type, duration, duration});
    } catch (...) {
      continue;
    }
  }
  return bursts;
}

void Runtime::initialize(string filename) {
  adaptive_mode = (policy == ADAPTIVE);
  scheduler.setRuntime(this);
  scheduler.setMetrics(&metrics);

  ifstream file(filename);
  if (!file.is_open()) {
    cerr << "Error: Could not open workload file " << filename << endl;
    return;
  }

  string line;
  if (!getline(file, line))
    return;

  while (getline(file, line)) {
    if (line.empty() || line[0] == '#')
      continue;

    stringstream ss(line);
    string id_str, func_name, type_str, arrival_str, priority_str, bursts_str;

    if (!getline(ss, id_str, ','))
      continue;
    if (!getline(ss, func_name, ','))
      continue;
    if (!getline(ss, type_str, ','))
      continue;
    if (!getline(ss, arrival_str, ','))
      continue;
    if (!getline(ss, priority_str, ','))
      continue;

    if (!getline(ss, bursts_str))
      continue;

    bursts_str.erase(0, bursts_str.find_first_not_of(" \t\""));
    bursts_str.erase(bursts_str.find_last_not_of(" \t\"") + 1);

    try {
      int id = stoi(id_str);
      TaskType type = getTaskType(type_str);
      function<void()> func = getTaskFunction(func_name);
      int arrival = stoi(arrival_str);
      int priority = stoi(priority_str);

      if (bursts_str.find_first_of("CcIi") != string::npos) {
        vector<TaskBurst> burst_list = parseburst(bursts_str);
        Task *newTask = new Task(id, func, type, priority, burst_list, arrival);
        if (func_name.find("FAULTY") != std::string::npos ||
            func_name.find("STALL") != std::string::npos) {
          newTask->makeBuggy();
        }

       
        newTask->max_demand[0] =1; 
        newTask->max_demand[1] = 1 + ((id * 2) % 4);
        newTask->max_demand[2] = 1 + (id % 3);

        scheduler.addTask(newTask);
      }
    } catch (...) {
      cerr << "Warning: Skipping malformed workload line: " << line << endl;
    }
  }
  file.close();
}

void Runtime::runstep() {
  metrics.starttime();
  int step_count = 0;
  while (scheduler.getTaskcount() > 0) {
    double cpu = last_cpu;
    double memory = last_mem;
     if (step_count >= next_monitor_step) {
      double prev_cpu = cpu;
      auto start_monitor = chrono::high_resolution_clock::now();
      cpu = monitor.getCPUUsage();
      memory = monitor.getMemoryUsage();
      auto end_monitor = chrono::high_resolution_clock::now();
      metrics.addoverhead(
          chrono::duration<double>(end_monitor - start_monitor).count());
     double delta = std::abs(cpu - prev_cpu);
      if (delta < 2.0) {
          monitor_interval = min(50, monitor_interval * 2);
      } else if (delta > 5.0) {
          monitor_interval = 5;
      }
      next_monitor_step = step_count + monitor_interval;

     
      double slope = cpu - prev_cpu;
      if (slope > 15.0 && cpu > 60.0) {
          setActivity("[PREDICTIVE] High CPU Surge detected (Slope: " + to_string((int)slope) + ")! Transitioning to protective mode.");
          
          integral_error += 50.0; 
      }
    }
    step_count++;

    if (adaptive_mode) {
      Scheduling_Policy temp = policyobj.decidepolicy(cpu, memory);
      if (temp != last_policy_val) {
        last_policy_val = temp;
        setActivity("Policy changed to " + searchpolicy(temp));
        scheduler.setpolicy(temp);
      }
    }

    
    double error = cpu - target_cpu;
    integral_error += error;
    double derivative = error - last_error;

    
    double output = (kp * error) + (ki * integral_error) + (kd * derivative);

    if (output > 0) {
      throttle_delay_ms = output;
    
      if (throttle_delay_ms > 10.0) {
        setActivity("[PID] High load (" + to_string((int)cpu) +
                    "%)! Throttling by " + to_string((int)throttle_delay_ms) +
                    "ms");
      }
    } else {
      throttle_delay_ms = 0;
      integral_error = 0;
    }

    if (throttle_delay_ms > 100)
      throttle_delay_ms = 100;
    last_error = error;

    updateMetrics(cpu, memory, getpolicy());
    scheduler.run(cpu, memory);

    if (!is_test_mode) {
      refreshDashboard();
     
      this_thread::sleep_for(chrono::milliseconds(1 + (int)throttle_delay_ms));
    }
  }
  if (!is_test_mode)
    refreshDashboard(); 
  metrics.endtime();
  metrics.settotaltime(scheduler.getglobaltime());
  metrics.setidletime(scheduler.getidletime());

  if (!is_test_mode) {
    generateFinalReport();
  } else {
    metrics.print();
  }
}

void Runtime::renderProgressBar(std::string label, double value,
                                double max_value) {
  const int barWidth = 30;
  double percentage = (value / max_value) * 100.0;
  if (percentage > 100.0)
    percentage = 100.0;
  if (percentage < 0)
    percentage = 0;

  string color = GREEN;
  if (percentage > 50)
    color = YELLOW;
  if (percentage > 85)
    color = RED;

  std::cout << BOLD << label << RESET << ": [";
  int pos = barWidth * (percentage / 100.0);
  for (int i = 0; i < barWidth; ++i) {
    if (i < pos)
      std::cout << color << "■" << RESET;
    else
      std::cout << " ";
  }
  std::cout << "] " << BOLD << color << int(percentage) << "%" << RESET << "\n";
}

void Runtime::PrintDashboard(double cpu, double memory,
                             Scheduling_Policy temp) {
  std::cout << "\033[H\033[J";
  std::cout << CYAN << BOLD
            << "╔══════════════════════════════════════════════════════════╗"
            << RESET << "\n";
  std::cout << CYAN << BOLD
            << "║             RESOURCE MANAGER - LIVE RUNTIME              ║"
            << RESET << "\n";
  std::cout << CYAN << BOLD
            << "╚══════════════════════════════════════════════════════════╝"
            << RESET << "\n";

  renderProgressBar("CPU LOAD   ", cpu, 100.0);
  renderProgressBar("MEM USAGE  ", memory, 20.0);

  std::cout << "\n" << CYAN << BOLD << "  SYSTEM STATUS:" << RESET << "\n";
  std::cout << "    Policy:   " << BG_BLUE << BOLD << " " << searchpolicy(temp)
            << " " << RESET << " | ";
  std::cout << "Active Tasks: " << BOLD << scheduler.getTaskcount() << RESET
            << " | ";
  std::cout << "Time: " << BOLD << scheduler.getglobaltime() << RESET << " | ";
  std::cout << "Context Switches: " << BOLD << metrics.getcontextswitch() << RESET << "\n";

  std::cout << "\n"
            << BOLD << "CPU CORES STATUS (" << num_cores << "):" << RESET
            << "\n";
  for (int i = 0; i < num_cores; ++i) {
    std::cout << "  Core " << i << ": ";
    bool found = false;
    for (Task *task : scheduler.getTaskTable()) {
      if (task->getCore() == i && task->getstate() == "Running") {
        std::cout << "[" << GREEN << "RUNNING" << RESET << "] T"
                  << task->getid();
        found = true;
        break;
      }
    }
    if (!found)
      std::cout << "[" << YELLOW << "IDLE" << RESET << "]";
    std::cout << "\n";
  }

  std::cout << "\n" << BOLD << "LIVE ACTIVITY FEED:" << RESET << "\n";
  std::cout << "  " << GREEN << "» " << RESET << BOLD << current_activity
            << RESET << "\n";
  std::cout << CYAN
            << "────────────────────────────────────────────────────────────"
            << RESET << "\n";
}

void Runtime::PrintTaskTable() {
  std::cout << BOLD << "ID   | STATE     | CORE | PRIO | WAIT | PROGRESS"
            << RESET << "\n";
  std::cout << "─────┼───────────┼──────┼──────┼──────┼────────────────────\n";
  for (Task *t : scheduler.getTaskTable()) {
    string stateColor = RESET;
    if (t->getstate() == "Running")
      stateColor = GREEN + BOLD;
    else if (t->getstate() == "Waiting")
      stateColor = YELLOW;
    else if (t->getstate() == "Finished")
      stateColor = CYAN;
    else if (t->getstate() == "Killed")
      stateColor = RED + BOLD;
    else if (t->getstate() == "Pending")
      stateColor = RESET;
    else
      stateColor = BLUE;

    string coreStr = "N/A";
    if (t->getstate() == "Running")
      coreStr = to_string(t->getCore());
    else if (t->getstate() == "Finished") {
      if (t->finished_by_core != -1)
        coreStr = "C" + to_string(t->finished_by_core);
      else
        coreStr = "INIT"; // Rarely happens but safe
    }

    printf("T%-3d | %s%-9s%s | %4s | %4d | %4d | [", t->getid(),
           stateColor.c_str(), t->getstate().c_str(), RESET.c_str(),
           coreStr.c_str(), t->getpriority(), t->getWaitingTime());

    double progress = t->get_progress();
    int progWidth = 15;
    int pos = progWidth * (progress / 100.0);
    for (int i = 0; i < progWidth; ++i) {
      if (i < pos)
        std::cout << (t->getstate() == "Finished" ? CYAN : GREEN) << "■"
                  << RESET;
      else
        std::cout << " ";
    }
    printf("] %3d%%\n", (int)progress);
  }
  std::cout << CYAN
            << "════════════════════════════════════════════════════════════"
            << RESET << "\n";
}

void Runtime::setCoreCount(int n) {
  if (n > MAX_CORES) {
    setActivity("Warning: Maximum core limit is " + to_string(MAX_CORES) +
                ". Capping at " + to_string(MAX_CORES) + ".");
    n = MAX_CORES;
  }
  num_cores = n;
  scheduler.setNumCores(n);
}

void Runtime::setpolicy(Scheduling_Policy temp) {
  policy = temp;
  adaptive_mode = (temp == ADAPTIVE);
  scheduler.setpolicy(temp);
}

Scheduling_Policy Runtime::getpolicy() { return policy; }

string Runtime::searchpolicy(int n) {
  switch (n) {
  case FIFO:
    return "FIFO";
  case ROUNDROBIN:
    return "ROUNDROBIN";
  case PRIORITY:
    return "PRIORITY";
  case ADAPTIVE:
    return "ADAPTIVE";
  case MLFQ:
    return "MLFQ";
  default:
    return "UNKNOWN";
  }
}

void Runtime::setActivity(std::string activity) {
  if (current_activity == activity)
    return;
  current_activity = activity;
  std::ofstream logFile("adaptation_log.txt", std::ios::app);
  if (logFile.is_open()) {
    logFile << "[Time: " << scheduler.getglobaltime() << "] " << activity
            << std::endl;
    logFile.close();
  }
}

void Runtime::updateMetrics(double cpu, double mem, Scheduling_Policy p) {
  last_cpu = cpu;
  last_mem = mem;
  last_policy_val = p;
}

void Runtime::refreshDashboard() {
  PrintDashboard(last_cpu, last_mem, last_policy_val);
  PrintTaskTable();
}

void Runtime::runComparison() {
  is_test_mode = true;
  std::cout << "\033[H\033[J";
  std::cout << CYAN << BOLD
            << "╔══════════════════════════════════════════════════════════╗"
            << RESET << "\n";
  std::cout << CYAN << BOLD
            << "║             RESOURCE MANAGER - COMPARISON TEST           ║"
            << RESET << "\n";
  std::cout << CYAN << BOLD
            << "╚══════════════════════════════════════════════════════════╝"
            << RESET << "\n";
  std::cout << "Running same workload under all policies...\n";

  struct Result {
    string name;
    int time;
    int switches;
    double overhead;
  };
  vector<Result> results;
  Scheduling_Policy policies[] = {FIFO, ROUNDROBIN, PRIORITY, ADAPTIVE, MLFQ};

  for (auto p : policies) {
    std::cout << "  Testing " << searchpolicy(p) << "... " << std::flush;
    scheduler.reset();
    metrics.reset();
    setpolicy(p);
    scheduler.setpolicy(p);
    initialize();
    runstep();
    results.push_back({searchpolicy(p), scheduler.getglobaltime(),
                       metrics.getcontextswitch(), metrics.getoverhead()});
    std::cout << GREEN << "Done" << RESET << "\n";
  }

  std::cout << "\n"
            << BOLD << "       PERFORMANCE COMPARISON SUMMARY" << RESET << "\n";
  std::cout << CYAN
            << "────────────────────────────────────────────────────────────"
            << RESET << "\n";
  printf("%-12s | %-12s | %-12s | %-12s\n", "POLICY", "TOTAL TIME",
         "CONTEXT SW", "OVERHEAD");
  std::cout << "─────────────┼──────────────┼──────────────┼──────────────\n";
  for (const auto &r : results) {
    printf("%-12s | %-12d | %-12d | %-12.4f\n", r.name.c_str(), r.time,
           r.switches, r.overhead);
  }
  std::cout << CYAN
            << "────────────────────────────────────────────────────────────"
            << RESET << "\n\n";

  // FEATURE: Vertical Terminal Histogram for Context Switches
  std::cout << BOLD << "  VISUAL COMPARISON: CONTEXT SWITCH OVERHEAD" << RESET << "\n\n";
  int max_switches = 0;
  for (const auto &r : results) {
    if (r.switches > max_switches)
      max_switches = r.switches;
  }

  const int chartHeight = 10;
  std::cout << "  ^ " << BOLD << "Context Switches" << RESET << " (Max: " << max_switches << ")\n";
  
  // Header row for values
  std::cout << "    | ";
  for (const auto &r : results) {
    printf("  %3d    ", r.switches);
  }
  std::cout << "\n";

  for (int h = chartHeight; h > 0; --h) {
    std::cout << "    | ";
    for (const auto &r : results) {
      int barH = (max_switches > 0) ? (int)(r.switches * chartHeight / max_switches) : 0;
      if (r.switches > 0 && barH == 0) barH = 1; 
      
      if (barH >= h) {
        std::cout << GREEN << "  [■■]   " << RESET << " ";
      } else {
        std::cout << "         " << " ";
      }
    }
    std::cout << "\n";
  }

  std::cout << "    └";
  for (size_t i = 0; i < results.size(); ++i) std::cout << "──────────";
  std::cout << "\n      ";
  for (const auto &r : results) {
    string shortName = r.name.substr(0, 5);
    printf("  %-5s   ", shortName.c_str());
  }
  std::cout << "\n\n";

  std::cout << CYAN
            << "────────────────────────────────────────────────────────────"
            << RESET << "\n\n";
  is_test_mode = false;
}

void Runtime::generateFinalReport() {
  double total_time = scheduler.getglobaltime();
  double overhead = metrics.getoverhead();
  int completed = metrics.getcompletedtask();

  std::cout << "\n"
            << BOLD << CYAN
            << "╔══════════════════════════════════════════════════════════╗"
            << RESET << "\n";
  std::cout << CYAN << BOLD
            << "║             FINAL SYSTEM EVALUATION REPORT               ║"
            << RESET << "\n";
  std::cout << CYAN << BOLD
            << "╚══════════════════════════════════════════════════════════╝"
            << RESET << "\n";

  double actual_time = metrics.getActualDuration();
  cout << "  " << left << setw(25) << "Tasks Completed" << " : " << BOLD
       << completed << RESET << "\n";
  cout << "  " << left << setw(25) << "Total Sim Time (Cycles)" << " : " << BOLD
       << (int)total_time << " cycles" << RESET << "\n";
  cout << "  " << left << setw(25) << "Actual Exec Time (Sec)" << " : " << BOLD
       << fixed << setprecision(2) << actual_time << " s" << RESET << "\n";
  cout << "  " << left << setw(25) << "Monitor Overhead" << " : " << BOLD
       << fixed << setprecision(2) << overhead << " s" << RESET << "\n";

  double overhead_pct = (actual_time > 0) ? (overhead / actual_time) * 100.0 : 0;
  string overhead_color =
      (overhead_pct < 5.0) ? GREEN : (overhead_pct < 10.0 ? YELLOW : RED);
  cout << "  " << left << setw(25) << "Overhead Impact" << " : "
       << overhead_color << BOLD << fixed << setprecision(4) << overhead_pct
       << " %" << RESET << "\n";

  double throughput = (total_time > 0) ? ((double)completed / total_time) * 1000.0 : 0;
  cout << "  " << left << setw(25) << "System Throughput" << " : " << BOLD
       << fixed << setprecision(3) << throughput << " tasks/1000 cycles" << RESET
       << "\n";
  cout << "  " << left << setw(25) << "Context Switches" << " : " << BOLD
       << metrics.getcontextswitch() << RESET << "\n";

  std::cout << "\n"
            << CYAN << BOLD << "  CORE COMPLETION BREAKDOWN:" << RESET << "\n";
  for (int i = 0; i < num_cores; ++i) {
    printf("    Core %d completed : %-2d tasks %s\n", i,
           metrics.getCoreCompletions(i),
           (metrics.getCoreCompletions(i) > 0 ? "✔" : ""));
  }

  std::cout << CYAN
            << "────────────────────────────────────────────────────────────"
            << RESET << "\n";
  std::cout << BOLD << "  CONCLUSION:" << RESET << "\n";
  if (overhead_pct < 5.0) {
    std::cout << "  " << GREEN << "✔ " << RESET
              << "The runtime maintains minimal interference with application "
                 "logic.\n";
  } else {
    std::cout << "  " << YELLOW << "! " << RESET
              << "Monitoring overhead is noticeable; consider increasing "
                 "sampling intervals.\n";
  }
  std::cout << CYAN
            << "════════════════════════════════════════════════════════════"
            << RESET << "\n\n";
  metrics.saveToCSV();
}