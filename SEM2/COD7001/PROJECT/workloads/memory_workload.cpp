#include <vector>
#include "workload.h"
void memoryTask() {
    // Allocate 1MB of memory each time it is called
    std::vector<int> v(250000, 1); // ~1MB
}