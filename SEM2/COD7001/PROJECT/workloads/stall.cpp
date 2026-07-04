#include "workload.h"
#include <iostream>

void stallTask() {
    // This task does literally nothing, simulating a hung process or infinite loop 
    // that doesn't contribute to progress.
    // In our simulation, Task::execute() increments progress based on steps.
    // If we want it to stall, we can't just define a function that does nothing,
    // because Task::execute() usually increments progress independent of the function.
    
    // WAIT: I should check Task::execute() implementation!
}
