#include <iostream>
#include <vector>
#include <cassert>
#include "deadlock_detector.h"
#include "watchdog.h"
#include "task.h"
#include "runtime.h"

// Note: Runtime stubs are provided in test_stubs.cpp

void test_bankers() {
    std::cout << "Testing Banker's Algorithm... " << std::flush;
    
    DeadlockDetector detector;
    // Initial: Available {10, 20, 20}
    detector.available = {5, 5, 5};
    detector.total_resources = {10, 10, 10};

    // Task 1 needs {4, 4, 4}, requesting {2, 2, 2}
    // This task is safe because 4-2=2 <= 5-2=3 remaining work
    Task t1(1, nullptr, CPU_BOUND, 10, 100, 0, 0, 0);
    t1.max_demand = {4, 4, 4};
    t1.allocation = {2, 2, 2};
    
    std::vector<Task*> tasks = {&t1};
    bool safe = detector.isSafe(&t1, {1, 1, 1}, tasks);
    assert(safe == true);

    // Over-requesting available
    bool unsafe = detector.isSafe(&t1, {10, 10, 10}, tasks);
    assert(unsafe == false);

    std::cout << "PASSED" << std::endl;
}

void test_watchdog() {
    std::cout << "Testing Task Watchdog... " << std::flush;
    
    Watchdog watchdog(3); // 3 step timeout for testing
    
    Task t1(1, nullptr, CPU_BOUND, 10, 100, 0, 0, 0);
    t1.setstate("Running");
    t1.last_progress = t1.get_progress(); // Ensure they match to trigger stall
    
    std::vector<Task*> coreSlots = {&t1};
    
    // Step 1: No progress change
    watchdog.check(coreSlots);
    assert(t1.stall_counter == 1);
    
    // Step 2: No progress change
    watchdog.check(coreSlots);
    assert(t1.stall_counter == 2);
    
    // Step 3: Progress changed!
    t1.execute(); // Increments progress in Task::execute (remaining becomes 99/100)
    watchdog.check(coreSlots);
    assert(t1.stall_counter == 0); // Reset
    
    // Step 4: Stalling again
    watchdog.check(coreSlots); // Counter 1
    watchdog.check(coreSlots); // Counter 2
    watchdog.check(coreSlots); // Counter 3
    watchdog.check(coreSlots); // Counter 4 -> KILL
    
    assert(t1.getstate() == "Killed");

    std::cout << "PASSED" << std::endl;
}

int main() {
    std::cout << "--- STARTING UNIT TESTS ---" << std::endl;
    try {
        test_bankers();
        test_watchdog();
        std::cout << "--- ALL UNIT TESTS PASSED ---" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
