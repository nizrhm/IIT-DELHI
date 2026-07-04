#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <vector>
#include "task.h"

class Runtime;

class Watchdog {
private:
    int timeout;
    Runtime* runtime;

public:
    Watchdog(int timeout_val = 30);
    void setRuntime(Runtime* r);
    void check(const std::vector<Task*>& coreSlots);
};

#endif
