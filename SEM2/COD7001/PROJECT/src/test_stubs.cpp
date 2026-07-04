#include "runtime.h"
#include <iostream>

// Minimal implementation of Runtime for unit tests to satisfy the linker
void Runtime::setActivity(std::string activity) {
    std::cout << "[TEST_RUNTIME] " << activity << std::endl;
}

// Stub other needed methods if any
void Runtime::refreshDashboard() {}
void Runtime::runComparison() {}

Scheduling_Policy Scheduling_Policy_from_string(const std::string& s) {
    return FIFO;
}
