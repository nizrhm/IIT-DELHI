#include <vector>
#include <chrono>
#include <thread>
#include <iostream>
#include "workload.h"

using namespace std;

void phaseChangeTask() {
    // Phase 1: Light Workload (CPU < 10%)
    for (int i = 0; i < 3; i++) {
        volatile long sum = 0;
        for (long j = 0; j < 50000; j++) {
            sum += j;
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }

    // Phase 2: Sudden Stress (CPU Spike + Memory Pressure)
    // Simplified for faster simulation but still heavy enough to trigger adaptation
    const int size = 500000; // ~2MB allocation
    vector<int> data(size, 1);
    
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < size; j++) {
            data[j] = data[j] * 2 + i;
        }
    }
}
