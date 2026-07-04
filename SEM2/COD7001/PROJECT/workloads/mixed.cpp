#include <vector>
#include "workload.h"

using namespace std;

void mixedTask() {
    // Perform some memory allocation and some CPU processing
    vector<int> v(50000, 1); // ~200KB
    for (int i = 0; i < 50000; i++) {
        v[i] = i * i;
    }
}