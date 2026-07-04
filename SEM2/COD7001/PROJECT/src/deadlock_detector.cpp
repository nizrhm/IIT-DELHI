#include "deadlock_detector.h"
#include <algorithm>
#include <iostream>

DeadlockDetector::DeadlockDetector() {
    // High defaults to ensure the simulation doesn't deadlock 
    // when tasks request multiple cores/resources.
    available = {16, 32, 32}; 
    total_resources = {16, 32, 32};
    resourceNames = {"CPU", "MEM", "I/O"};
}

void DeadlockDetector::initialize(int num_resources, const std::vector<int>& totals) {
    total_resources = totals;
    available = totals;
    resourceNames.clear();
    for(int i=0; i<num_resources; i++) {
        resourceNames.push_back("Res" + std::to_string(i));
    }
}

void DeadlockDetector::normalizeTask(Task* t) {
    if (total_resources.empty()) return;
    
    // Ensure task demand metadata is the same size as detector's 
    if (t->max_demand.size() < total_resources.size()) {
        t->max_demand.resize(total_resources.size(), 0);
        t->allocation.resize(total_resources.size(), 0);
    }

    for (size_t i = 0; i < total_resources.size(); i++) {
        // CAPPING: A task cannot demand more than what is logically possible in the system
        if (t->max_demand[i] > total_resources[i]) {
            t->max_demand[i] = total_resources[i];
        }
        // SAFETY: Demand cannot be less than current allocation
        if (t->max_demand[i] < t->allocation[i]) {
            t->max_demand[i] = t->allocation[i];
        }
    }
}

bool DeadlockDetector::isSafe(Task* requestingTask, std::vector<int> request, const std::vector<Task*>& allTasks) {
    int m = available.size();
    if (m == 0) return true; 
    
    // Pre-flight check: normalize if needed
    normalizeTask(requestingTask);
    
    if (request.size() < (size_t)m) request.resize(m, 0);

    // Initial check: is the request even possible right now?
    for (int j = 0; j < m; j++) {
        if (request[j] > available[j]) return false;
        if (requestingTask->allocation[j] + request[j] > requestingTask->max_demand[j]) return false;
    }

    // Safety simulation
    std::vector<int> work = available;
    for (int j = 0; j < m; j++) work[j] -= request[j];

    std::vector<Task*> activeTasks;
    for (Task* t : allTasks) {
        if (t->getstate() != "Finished" && t->getstate() != "Killed") {
            activeTasks.push_back(t);
        }
    }

    int n = activeTasks.size();
    if (n == 0) return true;

    std::vector<bool> finish(n, false);
    std::vector<std::vector<int>> current_alloc(n, std::vector<int>(m));
    std::vector<std::vector<int>> need(n, std::vector<int>(m));

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            current_alloc[i][j] = activeTasks[i]->allocation[j];
            if (activeTasks[i] == requestingTask) current_alloc[i][j] += request[j];
            need[i][j] = std::max(0, activeTasks[i]->max_demand[j] - current_alloc[i][j]);
        }
    }

    int count = 0;
    while (count < n) {
        bool found = false;
        for (int i = 0; i < n; i++) {
            if (!finish[i]) {
                bool can_finish = true;
                for (int j = 0; j < m; j++) {
                    if (need[i][j] > work[j]) {
                        can_finish = false;
                        break;
                    }
                }
                if (can_finish) {
                    for (int j = 0; j < m; j++) work[j] += current_alloc[i][j];
                    finish[i] = true;
                    found = true;
                    count++;
                }
            }
        }
        if (!found) break;
    }

    return count == n;
}

void DeadlockDetector::releaseResources(Task* t) {
    for (size_t i = 0; i < available.size() && i < t->allocation.size(); i++) {
        available[i] += t->allocation[i];
        t->allocation[i] = 0;
    }
}
