#include "workload.h"
#include <iostream>
#include <vector>

using namespace std;


void cpuTask() {
    volatile long sum = 0;
    for (long i = 0; i < 1000000; i++) { 
        sum += i;
    }
}