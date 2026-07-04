#ifndef DEADLOCK_DETECTOR_H
#define DEADLOCK_DETECTOR_H

#include <vector>
#include <string>
#include "task.h"

class DeadlockDetector {
public:
    std::vector<int> available;
    std::vector<int> total_resources;
    std::vector<std::string> resourceNames;

    DeadlockDetector();
    void initialize(int num_resources, const std::vector<int>& totals);
    void normalizeTask(Task* t); // NEW: Capping demands to system total
    bool isSafe(Task* requestingTask, std::vector<int> request, const std::vector<Task*>& allTasks);
    void releaseResources(Task* t);
};

#endif
