#ifndef RUNTIME_H
#define RUNTIME_H
#include "../workloads/workload.h"
#include "Metrics.h"
#include "monitor.h"
#include "policy_engine.h"
#include "scheduler.h"
#include "task.h"

using namespace std;

class Runtime {
private:
  const string RESET = "\033[0m";
  const string BOLD = "\033[1m";
  const string RED = "\033[31m";
  const string GREEN = "\033[32m";
  const string YELLOW = "\033[33m";
  const string BLUE = "\033[34m";
  const string CYAN = "\033[36m";
  const string BG_BLUE = "\033[44m";

  Scheduler scheduler;
  Monitor monitor;
  policyEngine policyobj;
  Metrics metrics;
  Scheduling_Policy policy = ADAPTIVE;
  std::string current_activity = "System Idle";
  double last_cpu = 0;
  double last_mem = 0;
  Scheduling_Policy last_policy_val = FIFO;
  bool adaptive_mode = false; // New: Persistent adaptive mode flag

  // PID-based Throttling variables
  double target_cpu = 70.0;
  double kp = 0.8;
  double ki = 0.1;
  double kd = 0.05;
  double integral_error = 0.0;
  double last_error = 0.0;
  double throttle_delay_ms = 0.0;
  int next_monitor_step = 0;
  int monitor_interval = 5;

public:
  bool is_test_mode = false;
  bool use_banker = false; // Optional Banker's check
  int num_cores = 1;
  const int MAX_CORES = 4;

  void initialize(std::string filename = "workload.csv");
  void runstep();
  void renderProgressBar(std::string label, double value, double max_value);
  void PrintDashboard(double cpu, double memory, Scheduling_Policy temp);
  void PrintTaskTable();
  Scheduling_Policy getpolicy();
  void setpolicy(Scheduling_Policy);
  string searchpolicy(int n);
  void runtime_yeild();
  void setActivity(std::string activity);
  void updateMetrics(double cpu, double mem, Scheduling_Policy p);
  void refreshDashboard();
  void runComparison();
  void generateFinalReport();
  void setCoreCount(int n);

  void enableDeadlockDetector() {
    use_banker = true;
    scheduler.use_banker = true;
  }

private:
  std::function<void()> getTaskFunction(const std::string &name);
  TaskType getTaskType(const std::string &type);
  std::vector<TaskBurst> parseburst(std::string &s);
};

#endif