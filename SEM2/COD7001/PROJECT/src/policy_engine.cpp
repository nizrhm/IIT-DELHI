#include "policy_engine.h"
#include "scheduler.h"

Scheduling_Policy policyEngine::decidepolicy(double cpuUsage, double memoryUsage) {
  if (cpuUsage > 80.0 || memoryUsage > 9.0) {
    return ADAPTIVE; // Heavy stress: Use adaptive deferral logic
  } else if (cpuUsage > 60.0) {
    return MLFQ; // High load: Use MLFQ for fairness
  } else if (cpuUsage > 30.0 || memoryUsage > 4.5) {
    return ROUNDROBIN; // Moderate load: Use time-slicing
  } else {
    return PRIORITY; // Low load: Standard priority
  }
}