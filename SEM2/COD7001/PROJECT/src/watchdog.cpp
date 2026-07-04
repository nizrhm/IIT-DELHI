#include "watchdog.h"
#include "runtime.h"
#include <iostream>

Watchdog::Watchdog(int timeout_val) : timeout(timeout_val), runtime(nullptr) {}

void Watchdog::setRuntime(Runtime* r) {
    runtime = r;
}

void Watchdog::check(const std::vector<Task*>& coreSlots) {
    for (Task* t : coreSlots) {
        if (t == nullptr) continue;
        
        // Only monitor tasks that are supposed to be making progress
        if (t->getstate() == "Running") {
            double current_prog = t->get_progress();
            if (current_prog == t->last_progress) {
                t->stall_counter++;
                if (t->stall_counter > timeout) {
                    if (runtime) runtime->setActivity("[WATCHDOG] Killing T" + std::to_string(t->getid()) + " (Stalled on Core " + std::to_string(t->current_core) + ")");
                    t->setstate("Killed");
                    // Note: Resource release will be handled by the scheduler after detecting the Killed state
                }
            } else {
                t->last_progress = current_prog;
                t->stall_counter = 0;
            }
        }
    }
}
