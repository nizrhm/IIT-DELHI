/**
 * @file monitor.h
 * @brief Resource monitoring interface for CPU and Memory sampling.
 */

#ifndef MONITOR_H
#define MONITOR_H

class Monitor {
public:
    double getCPUUsage();
    long getMemoryUsage(); // in KB
};

#endif