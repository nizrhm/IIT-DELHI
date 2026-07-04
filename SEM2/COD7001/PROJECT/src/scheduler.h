/**
 * @file scheduler.h
 * @brief Task scheduling core with multiple policy support.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "Metrics.h"
#include "deadlock_detector.h"
#include "task.h"
#include "watchdog.h"
#include <queue>
#include <map>
#include <vector>

using namespace std;
enum Scheduling_Policy { FIFO, ROUNDROBIN, PRIORITY, ADAPTIVE, MLFQ };

struct TaskComparator {
  bool operator()(Task *a, Task *b) {
    if (a->getpriority() != b->getpriority()) {
      return a->getpriority() < b->getpriority();
    }
    return a->get_arrival_time() > b->get_arrival_time();
  }
};

class Runtime;
class Scheduler {
private:
  Runtime *runtime;
  Metrics *metrics;
  vector<Task *> taskTable;
  std::queue<Task *> readyQueue;
  std::vector<std::queue<Task *>> mlfq_queues;
  std::priority_queue<Task *, std::vector<Task *>, TaskComparator>
      priorityqueue;
  queue<Task *> waitingqueue;
  vector<Task *> pendingTasks;
  Scheduling_Policy currentpolicy = ADAPTIVE;
  int globaltime = 0;
  int idletime = 0;
  int num_cores = 1;
  std::vector<Task *> coreSlots;
  std::vector<int> coreLastTaskIds;
  std::vector<int> coreQuantums;
  double last_cpu = 0;
  double last_mem = 0;

  // Deadlock Recovery & Smart Logging
  int consecutive_deferred_ticks = 0;
  int last_logged_task_id = -1;
  int last_logged_tick = -1;
  const int MAX_QUANTUM = 2;

  // Externalized features
  DeadlockDetector deadlockDetector;
  Watchdog watchdog;
  
  // IPC Mailbox
  std::map<int, std::queue<Message>> mailboxes;

public:
  bool use_banker = false; // Flag to enable/disable Banker's safety check

  Scheduler();
  ~Scheduler(); // New: Destructor for memory cleanup
  void setMetrics(Metrics *m);
  void addTask(Task *task);
  void run(double, double);
  void fiforun();
  void roundrobinrun();
  void priorityrun(string label = "Priority");
  void mlfqrun();
  void setpolicy(Scheduling_Policy policy);
  int getTaskcount();
  vector<Task *> getTaskTable();
  vector<Task *> getalltasks(); // Declaration for the implementation in cpp
  void setRuntime(Runtime *r);
  void setNumCores(int n);
  void rebalanceQueues();
  void updatewaitingqueue();
  int getglobaltime();
  int getidletime();
  void reset();
  void checkArrivals();
  void adptivescheduling(double, double);
  void applyAging();
  void recoverFromDeadlock(); // NEW: Deadlock Recovery logic
  void simulateIPC(); // FEATURE: Inter-Task Communication

  // Delegation helpers
  void releaseResources(Task *t);
  bool isSafe(Task *t, std::vector<int> request) {
    return deadlockDetector.isSafe(t, request, taskTable);
  }
};

#endif