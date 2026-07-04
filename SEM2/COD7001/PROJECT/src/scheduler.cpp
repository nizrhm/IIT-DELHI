/**
 * @file scheduler.cpp
 * @brief Implementation of FIFO, Priority, Round-Robin, and MLFQ policies with
 * Banker's Algorithm safety checks.
 */

#include "scheduler.h"
#include "Metrics.h"
#include "runtime.h"
#include "task.h"
#include <iostream>
#include <string>
#include <vector>

using namespace std;

Scheduler::Scheduler()
    : runtime(nullptr), metrics(nullptr), globaltime(0), idletime(0),
      num_cores(1), use_banker(false) {
  mlfq_queues.resize(3);
  coreSlots.resize(1, nullptr);
  coreLastTaskIds.resize(1, -1);
  coreQuantums.resize(1, 0);
  consecutive_deferred_ticks = 0;
  last_logged_task_id = -1;
  // Sync deadlock detector with the default core count
  deadlockDetector.available = {1, 32, 32};
  deadlockDetector.total_resources = {1, 32, 32};
}

Scheduler::~Scheduler() {
  for (Task *t : taskTable) {
    if (t)
      delete t;
  }
}

void Scheduler::setNumCores(int n) {
  if (n < 1)
    n = 1;
  num_cores = n;
  coreSlots.assign(n, nullptr);
  coreLastTaskIds.assign(n, -1);
  coreQuantums.assign(n, 0);
  // Update deadlock detector's available cores
  deadlockDetector.available[0] = n;
  deadlockDetector.total_resources[0] = n;
}

void Scheduler::reset() {
  for (Task *t : taskTable) {
    delete t;
  }
  taskTable.clear();
  while (!readyQueue.empty())
    readyQueue.pop();
  while (!priorityqueue.empty())
    priorityqueue.pop();
  while (!waitingqueue.empty())
    waitingqueue.pop();
  pendingTasks.clear();
  globaltime = 0;
  idletime = 0;
  coreSlots.assign(num_cores, nullptr);
  coreLastTaskIds.assign(num_cores, -1);
  coreQuantums.assign(num_cores, 0);
  for (auto &q : mlfq_queues) {
    while (!q.empty())
      q.pop();
  }
  deadlockDetector.available = deadlockDetector.total_resources;
}

void Scheduler::addTask(Task *task) {
  taskTable.push_back(task);
  if (task->get_arrival_time() <= globaltime) {
    if (currentpolicy == MLFQ)
      mlfq_queues[0].push(task);
    else if (currentpolicy == PRIORITY || currentpolicy == ADAPTIVE)
      priorityqueue.push(task);
    else
      readyQueue.push(task);
    task->setstate("Ready");
  } else {
    pendingTasks.push_back(task);
    task->setstate("Pending");
  }
}

void Scheduler::setMetrics(Metrics *m) { metrics = m; }

void Scheduler::setRuntime(Runtime *r) {
  runtime = r;
  watchdog.setRuntime(r);
}

vector<Task *> Scheduler::getTaskTable() { return taskTable; }

void Scheduler::setpolicy(Scheduling_Policy policy) {
  if (currentpolicy != policy) {
    currentpolicy = policy;
    rebalanceQueues();
  }
}

void Scheduler::rebalanceQueues() {
  std::vector<Task *> allActiveTasks;
  while (!readyQueue.empty()) {
    allActiveTasks.push_back(readyQueue.front());
    readyQueue.pop();
  }
  while (!priorityqueue.empty()) {
    allActiveTasks.push_back(priorityqueue.top());
    priorityqueue.pop();
  }
  for (int i = 0; i < 3; ++i) {
    while (!mlfq_queues[i].empty()) {
      allActiveTasks.push_back(mlfq_queues[i].front());
      mlfq_queues[i].pop();
    }
  }
  for (Task *t : allActiveTasks) {
    if (currentpolicy == MLFQ)
      mlfq_queues[t->mlfq_level].push(t);
    else if (currentpolicy == PRIORITY || currentpolicy == ADAPTIVE)
      priorityqueue.push(t);
    else
      readyQueue.push(t);
  }
}

int Scheduler::getTaskcount() {
  int count = readyQueue.size() + priorityqueue.size() + waitingqueue.size() +
              pendingTasks.size();
  for (const auto &q : mlfq_queues)
    count += q.size();
  for (int i = 0; i < num_cores; ++i)
    if (coreSlots[i] != nullptr)
      count++;
  return count;
}

int Scheduler::getglobaltime() { return globaltime; }
int Scheduler::getidletime() { return idletime; }

void Scheduler::run(double cpu, double memory) {
  this->last_cpu = cpu;
  this->last_mem = memory;
  watchdog.check(coreSlots);
  simulateIPC();
  checkArrivals();

  switch (currentpolicy) {
  case FIFO:
    fiforun();
    break;
  case ROUNDROBIN:
    roundrobinrun();
    break;
  case PRIORITY:
    priorityrun();
    break;
  case ADAPTIVE:
    adptivescheduling(cpu, memory);
    priorityrun("ADAPTIVE");
    break;
  case MLFQ:
    mlfqrun();
    break;
  }
}

void Scheduler::fiforun() {
  if (getTaskcount() == 0)
    return;
  bool system_busy = false;
  bool any_ready = !readyQueue.empty();

  for (int i = 0; i < num_cores; ++i) {
    if (coreSlots[i] == nullptr && !readyQueue.empty()) {
      Task *next = readyQueue.front();
      bool safe = true;
      if (use_banker) {
        deadlockDetector.normalizeTask(next);
        safe = deadlockDetector.isSafe(next, next->max_demand, getalltasks());
      }
      if (safe) {
        readyQueue.pop();
        for (int j = 0; j < 3; j++) {
          deadlockDetector.available[j] -= next->max_demand[j];
          next->allocation[j] = next->max_demand[j];
        }
        coreSlots[i] = next;
        coreSlots[i]->setstate("Running");
        coreSlots[i]->current_core = i;
        if (metrics)
          metrics->contextswitch();
        if (runtime)
          runtime->setActivity("Core " + to_string(i) + ": Started T" +
                               to_string(next->getid()));
        system_busy = true;
        consecutive_deferred_ticks = 0;
      } else {
        // Smart Logging: Only log once every 500 ticks for this task
        if (runtime && (globaltime - next->last_logged_tick >= 500)) {
          runtime->setActivity("[BANKER] T" + to_string(next->getid()) +
                               " deferred (Unsafe)");
          next->last_logged_tick = globaltime;
        }
      }
    }
    if (coreSlots[i] != nullptr) {
      system_busy = true;
      coreSlots[i]->last_run_tick = globaltime; // Update for aging
      coreSlots[i]->execute();
      if (coreSlots[i]->getstate() == "Waiting" ||
          coreSlots[i]->getstate() == "Finished") {
        if (coreSlots[i]->getstate() == "Waiting")
          waitingqueue.push(coreSlots[i]);
        else {
          coreSlots[i]->finished_by_core = i;
          if (metrics)
            metrics->completetask(i);
        }
        releaseResources(coreSlots[i]);
        coreSlots[i]->current_core = -1;
        coreSlots[i] = nullptr;
        consecutive_deferred_ticks = 0; // Progress made
      }
    }
  }

  if (!system_busy && any_ready) {
    consecutive_deferred_ticks++;
    if (consecutive_deferred_ticks > 500)
      recoverFromDeadlock();
  }

  globaltime++;
  if (!system_busy)
    idletime++;
  updatewaitingqueue();
}

void Scheduler::roundrobinrun() {
  if (getTaskcount() == 0)
    return;
  bool system_busy = false;
  bool any_ready = !readyQueue.empty();

  for (int i = 0; i < num_cores; ++i) {
    if (coreSlots[i] == nullptr && !readyQueue.empty()) {
      Task *next = readyQueue.front();
      bool safe = true;
      if (use_banker) {
        deadlockDetector.normalizeTask(next);
        safe = deadlockDetector.isSafe(next, next->max_demand, getalltasks());
      }
      if (safe) {
        readyQueue.pop();
        for (int j = 0; j < 3; j++) {
          deadlockDetector.available[j] -= next->max_demand[j];
          next->allocation[j] = next->max_demand[j];
        }
        coreSlots[i] = next;
        coreSlots[i]->setstate("Running");
        coreSlots[i]->current_core = i;
        coreQuantums[i] = 0;
        if (metrics)
          metrics->contextswitch();
        if (runtime)
          runtime->setActivity("Core " + to_string(i) + ": Started T" +
                               to_string(next->getid()) + " (RR)");
        system_busy = true;
        consecutive_deferred_ticks = 0;
      } else {
        if (runtime && (globaltime - next->last_logged_tick >= 500)) {
          runtime->setActivity("[BANKER] RR T" + to_string(next->getid()) +
                               " deferred (Unsafe)");
          next->last_logged_tick = globaltime;
        }
        readyQueue.pop();
        readyQueue.push(next);
      }
    }
    if (coreSlots[i] != nullptr) {
      system_busy = true;
      coreSlots[i]->last_run_tick = globaltime; // Update for aging
      coreSlots[i]->execute();
      coreQuantums[i]++;
      if (coreSlots[i]->getstate() == "Waiting" ||
          coreSlots[i]->getstate() == "Finished") {
        if (coreSlots[i]->getstate() == "Waiting")
          waitingqueue.push(coreSlots[i]);
        else {
          coreSlots[i]->finished_by_core = i;
          if (metrics)
            metrics->completetask(i);
        }
        releaseResources(coreSlots[i]);
        coreSlots[i]->current_core = -1;
        coreSlots[i] = nullptr;
        consecutive_deferred_ticks = 0;
      } else if (coreQuantums[i] >= 2) { // Quantum = 2
        if (!readyQueue.empty()) {
          Task *t = coreSlots[i];
          t->setstate("Ready");
          t->current_core = -1;
          readyQueue.push(t);
          releaseResources(t);
          coreSlots[i] = nullptr;
          consecutive_deferred_ticks = 0;
        } else {
          coreQuantums[i] = 0;
        }
      }
    }
  }

  if (!system_busy && any_ready) {
    consecutive_deferred_ticks++;
    if (consecutive_deferred_ticks > 500)
      recoverFromDeadlock();
  }

  globaltime++;
  if (!system_busy)
    idletime++;
  applyAging();
  updatewaitingqueue();
}

void Scheduler::priorityrun(string label) {
  if (getTaskcount() == 0)
    return;
  bool system_busy = false;
  bool any_ready = !priorityqueue.empty();

  for (int i = 0; i < num_cores; ++i) {
    if (coreSlots[i] == nullptr && !priorityqueue.empty()) {
      Task *next = priorityqueue.top();
      bool safe = true;
      if (use_banker) {
        deadlockDetector.normalizeTask(next);
        safe = deadlockDetector.isSafe(next, next->max_demand, getalltasks());
      }
      if (safe) {
        priorityqueue.pop();
        for (int j = 0; j < 3; j++) {
          deadlockDetector.available[j] -= next->max_demand[j];
          next->allocation[j] = next->max_demand[j];
        }
        coreSlots[i] = next;
        coreSlots[i]->setstate("Running");
        coreSlots[i]->current_core = i;
        if (metrics)
          metrics->contextswitch();
        if (runtime)
          runtime->setActivity(label + " Core " + to_string(i) + ": Started T" +
                               to_string(next->getid()));
        system_busy = true;
        consecutive_deferred_ticks = 0;
      } else {
        if (runtime && (globaltime - next->last_logged_tick >= 500)) {
          runtime->setActivity("[BANKER] " + label + " T" +
                               to_string(next->getid()) + " deferred");
          next->last_logged_tick = globaltime;
        }
      }
    }
    if (coreSlots[i] != nullptr) {
      system_busy = true;
      coreSlots[i]->last_run_tick = globaltime; // Update for aging
      coreSlots[i]->execute();
      if (coreSlots[i]->getstate() == "Waiting" ||
          coreSlots[i]->getstate() == "Finished") {
        if (coreSlots[i]->getstate() == "Waiting")
          waitingqueue.push(coreSlots[i]);
        else {
          coreSlots[i]->finished_by_core = i;
          if (metrics)
            metrics->completetask(i);
        }
        releaseResources(coreSlots[i]);
        coreSlots[i]->current_core = -1;
        coreSlots[i] = nullptr;
        consecutive_deferred_ticks = 0;
      }
    }
  }

  if (!system_busy && any_ready) {
    consecutive_deferred_ticks++;
    if (consecutive_deferred_ticks > 500)
      recoverFromDeadlock();
  }

  globaltime++;
  if (!system_busy)
    idletime++;
  applyAging();
  updatewaitingqueue();
}

void Scheduler::mlfqrun() {
  bool system_busy = false;
  bool any_ready = false;
  for (int level = 0; level < 3; ++level)
    if (!mlfq_queues[level].empty())
      any_ready = true;

  for (int i = 0; i < num_cores; ++i) {
    if (coreSlots[i] == nullptr) {
      for (int level = 0; level < 3; ++level) {
        if (!mlfq_queues[level].empty()) {
          Task *next = mlfq_queues[level].front();
          bool safe = true;
          if (use_banker) {
            deadlockDetector.normalizeTask(next);
            safe =
                deadlockDetector.isSafe(next, next->max_demand, getalltasks());
          }
          if (safe) {
            mlfq_queues[level].pop();
            for (int j = 0; j < 3; j++) {
              deadlockDetector.available[j] -= next->max_demand[j];
              next->allocation[j] = next->max_demand[j];
            }
            coreSlots[i] = next;
            coreSlots[i]->setstate("Running");
            coreSlots[i]->current_core = i;
            coreQuantums[i] = 0;
            if (metrics)
              metrics->contextswitch();
            if (runtime)
              runtime->setActivity("Core " + to_string(i) + ": Started T" +
                                   to_string(next->getid()) + " (MLFQ Q" +
                                   to_string(level) + ")");
            system_busy = true;
            consecutive_deferred_ticks = 0;
            break;
          } else {
            if (runtime && (globaltime - next->last_logged_tick >= 500)) {
              runtime->setActivity("[BANKER] MLFQ T" +
                                   to_string(next->getid()) + " deferred");
              next->last_logged_tick = globaltime;
            }
          }
        }
      }
    }
    if (coreSlots[i] != nullptr) {
      system_busy = true;
      Task *t = coreSlots[i];
      t->last_run_tick = globaltime; // Update for aging
      t->execute();
      coreQuantums[i]++;
      int max_q = (t->mlfq_level == 0) ? 2 : (t->mlfq_level == 1 ? 4 : 8);
      if (t->getstate() == "Waiting" || t->getstate() == "Finished") {
        if (t->getstate() == "Waiting")
          waitingqueue.push(t);
        else {
          t->finished_by_core = i;
          if (metrics)
            metrics->completetask(i);
        }
        releaseResources(t);
        coreSlots[i] = nullptr;
        consecutive_deferred_ticks = 0;
      } else if (coreQuantums[i] >= max_q) {
        if (t->mlfq_level < 2)
          t->mlfq_level++;
        t->setstate("Ready");
        mlfq_queues[t->mlfq_level].push(t);
        releaseResources(t);
        coreSlots[i] = nullptr;
        consecutive_deferred_ticks = 0;
      }
    }
  }

  if (!system_busy && any_ready) {
    consecutive_deferred_ticks++;
    if (consecutive_deferred_ticks > 500)
      recoverFromDeadlock();
  }

  globaltime++;
  if (!system_busy)
    idletime++;
  applyAging();
  updatewaitingqueue();
}

void Scheduler::checkArrivals() {
  auto it = pendingTasks.begin();
  while (it != pendingTasks.end()) {
    if ((*it)->get_arrival_time() <= globaltime) {
      Task *t = *it;
      if (currentpolicy == MLFQ)
        mlfq_queues[0].push(t);
      else if (currentpolicy == PRIORITY || currentpolicy == ADAPTIVE)
        priorityqueue.push(t);
      else
        readyQueue.push(t);
      t->setstate("Ready");
      it = pendingTasks.erase(it);
    } else
      ++it;
  }
}

void Scheduler::updatewaitingqueue() {
  int size = waitingqueue.size();
  while (size--) {
    Task *t = waitingqueue.front();
    waitingqueue.pop();
    if (t->getDeferred()) {
      if (last_cpu < 80.0) {
        t->setDeferred(false);
        t->setstate("Ready");
        readyQueue.push(t);
      } else
        waitingqueue.push(t);
      continue;
    }
    t->execute();
    if (t->getstate() != "Waiting" && t->getstate() != "Finished") {
      t->setstate("Ready");
      readyQueue.push(t);
    } else if (t->getstate() == "Waiting")
      waitingqueue.push(t);
  }
}

void Scheduler::adptivescheduling(double cpu, double memory) {
  if (priorityqueue.empty())
    return;
  std::vector<Task *> deferred;
  std::priority_queue<Task *, std::vector<Task *>, TaskComparator> temp;
  while (!priorityqueue.empty()) {
    Task *t = priorityqueue.top();
    priorityqueue.pop();
    if ((cpu > 80.0 || memory > 9.0) && t->getpriority() < 5) {
      t->setDeferred(true);
      t->setstate("Waiting");
      waitingqueue.push(t);
    } else
      temp.push(t);
  }
  priorityqueue = temp;
}

void Scheduler::applyAging() {
  // Aging threshold: 50 ticks without running
  const int AGING_THRESHOLD = 50;
  
  for (Task *t : taskTable) {
    if (t->getstate() == "Ready" || t->getstate() == "Waiting") {
      int idle_time = globaltime - t->last_run_tick;
      if (idle_time > AGING_THRESHOLD) {
        int old_prio = t->getpriority();
        t->setPriority(old_prio + 1); // Boost priority
        t->last_run_tick = globaltime; // Reset wait clock
        
        if (runtime) {
          runtime->setActivity("[AGING] Starvation Guard: Boosting T" + 
                               to_string(t->getid()) + " (Prio " + 
                               to_string(old_prio) + " -> " + 
                               to_string(t->getpriority()) + ")");
        }
      }
    }
  }
}

std::vector<Task *> Scheduler::getalltasks() { return taskTable; }

void Scheduler::releaseResources(Task *t) {
  deadlockDetector.releaseResources(t);

  // FEATURE: Memory Leak Detection Simulator
  if (t->getstate() == "Finished" && t->simulated_memory_used > 0) {
    if (runtime) {
      runtime->setActivity("[PROFILER] Leak Detected in T" + to_string(t->getid()) + 
                           ": " + to_string(t->simulated_memory_used) + " MB not freed!");
    }
  }
}

void Scheduler::recoverFromDeadlock() {
  if (runtime)
    runtime->setActivity(
        "[DEADLOCK RECOVERY] System stuck! Aborting lowest priority task.");

  Task *target = nullptr;

  // 1. Try to kill something in readyQueue
  if (!readyQueue.empty()) {
    target = readyQueue.front();
    readyQueue.pop();
  } else if (!priorityqueue.empty()) {
    target = priorityqueue.top();
    priorityqueue.pop();
  } else {
    for (auto &q : mlfq_queues) {
      if (!q.empty()) {
        target = q.front();
        q.pop();
        break;
      }
    }
  }

  if (target) {
    target->setstate("Killed");
    releaseResources(target);
    if (runtime)
      runtime->setActivity("[RECOVERY] Terminated T" +
                           to_string(target->getid()) + " to break deadlock.");
  }

  consecutive_deferred_ticks = 0;
}

void Scheduler::simulateIPC() {
  // Simple simulation: Every 100 ticks, Task A sends a heartbeat to Task B
  if (globaltime > 0 && globaltime % 100 == 0) {
    Task *sender = nullptr;
    Task *receiver = nullptr;

    // Find any two active tasks
    for (Task *t : taskTable) {
      if (t->getstate() == "Running" || t->getstate() == "Ready") {
        if (!sender)
          sender = t;
        else if (!receiver) {
          receiver = t;
          break;
        }
      }
    }

    if (sender && receiver) {
      Message msg = {sender->getid(), "READY_SYNC"};
      mailboxes[receiver->getid()].push(msg);

      if (runtime) {
        runtime->setActivity("[IPC] T" + to_string(sender->getid()) +
                             " sent Sync Message to T" +
                             to_string(receiver->getid()));
      }
    }
  }

  // Check mailboxes for running tasks
  for (int i = 0; i < num_cores; ++i) {
      if (coreSlots[i] != nullptr) {
          int tid = coreSlots[i]->getid();
          if (!mailboxes[tid].empty()) {
              Message msg = mailboxes[tid].front();
              mailboxes[tid].pop();
              if (runtime) {
                  runtime->setActivity("[IPC] T" + to_string(tid) + 
                                       " received message from T" + 
                                       to_string(msg.sender_id));
              }
          }
      }
  }
}
