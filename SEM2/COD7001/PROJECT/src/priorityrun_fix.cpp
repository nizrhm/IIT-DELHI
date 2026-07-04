void Scheduler::priorityrun(string label) {
    if (getTaskcount() == 0) return;

    // 1. Preemption Logic: Check if waiting tasks should bump running tasks
    if (!priorityqueue.empty()) {
        for (int i = 0; i < num_cores; ++i) {
            if (coreSlots[i] != nullptr) {
                Task* waiting = priorityqueue.top();
                Task* running = coreSlots[i];
                if (waiting->getpriority() > running->getpriority()) {
                    // Preempt!
                    running->setstate("Ready");
                    running->current_core = -1;
                    priorityqueue.push(running); // Put back
                    coreSlots[i] = nullptr; 
                    coreLastTaskIds[i] = -1;
                    if (runtime) runtime->setActivity("[" + label + " PREEMPT] Core " + to_string(i) + ": T" + to_string(waiting->getid()) + " bumped T" + to_string(running->getid()));
                    // The assignment logic below will pick up the 'waiting' task
                }
            }
        }
    }

    bool system_busy = false;
    for (int i = 0; i < num_cores; ++i) {
        // 2. Assignment logic: Fill empty cores
        if (coreSlots[i] == nullptr && !priorityqueue.empty()) {
            Task* best = priorityqueue.top();
            
            // Ensure task has resources metadata
            if (best->max_demand.size() < deadlockDetector.available.size()) {
                best->max_demand = deadlockDetector.available;
                best->max_demand[0] = 1; 
                best->allocation.assign(deadlockDetector.available.size(), 0);
            }

            std::vector<int> request(deadlockDetector.available.size(), 0);
            request[0] = 1; // Request 1 CPU

            if (isSafe(best, request)) {
                priorityqueue.pop();
                for (size_t j = 0; j < deadlockDetector.available.size(); j++) {
                    deadlockDetector.available[j] -= request[j];
                    best->allocation[j] += request[j];
                }
                coreSlots[i] = best;
                coreSlots[i]->setstate("Running");
                coreSlots[i]->current_core = i;
                coreSlots[i]->finished_by_core = i; 
                if (metrics && coreLastTaskIds[i] != best->getid()) metrics->contextswitch();
                coreLastTaskIds[i] = best->getid();
                if (runtime) runtime->setActivity(label + " Core " + to_string(i) + ": Started T" + to_string(best->getid()));
            } else {
                if (runtime) runtime->setActivity("[BANKER] " + label + " T" + to_string(best->getid()) + " deferred");
            }
        }

        // 3. Execution logic
        if (coreSlots[i] != nullptr) {
            system_busy = true;
            coreSlots[i]->execute();

            // 4. State Management
            if (coreSlots[i]->getstate() == "Waiting" || coreSlots[i]->getstate() == "Finished") {
                if (coreSlots[i]->getstate() == "Waiting") waitingqueue.push(coreSlots[i]);
                else if (metrics) {
                    coreSlots[i]->finished_by_core = i;
                    metrics->completetask(i);
                }
                
                // Release resources on finish/wait
                releaseResources(coreSlots[i]);

                coreSlots[i]->current_core = -1;
                coreSlots[i] = nullptr;
                coreLastTaskIds[i] = -1;
            }
        }
    }

    globaltime++;
    if (!system_busy) idletime++;
    applyAging();
    updatewaitingqueue();
}
