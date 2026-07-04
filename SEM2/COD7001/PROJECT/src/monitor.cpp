/**
 * @file monitor.cpp
 * @brief Implementation of Linux-specific resource monitoring (CPU/Memory).
 */

#include "monitor.h"
#include <fstream>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include<iostream>

double Monitor::getCPUUsage() {
    long user1, nice1, system1, idle1,iowait1,irq1,softirq1,steal1;
    long user2, nice2, system2, idle2,iowait2,irq2,softirq2,steal2;

    std::ifstream file1("/proc/stat");
    if(!file1.is_open()){
         std::cerr << "Error: Unable to open /proc/stat\n";
        return 0.0;
    }
    std::string cpu;
    file1 >> cpu >> user1 >> nice1 >> system1 >> idle1 >>iowait1>>irq1>>softirq1>>steal1;
    file1.close();

    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    std::ifstream file2("/proc/stat");
     if(!file2.is_open()){
         std::cerr << "Error: Unable to open /proc/stat\n";
        return 0.0;
    }
    file2 >> cpu >> user2 >> nice2 >> system2 >> idle2>>iowait2>>irq2>>softirq2>>steal2;
    file2.close();

    long total1 = user1 + nice1 + system1 + idle1+iowait1+irq1+softirq1+steal1;
    long total2 = user2 + nice2 + system2 + idle2+iowait2+irq2+softirq2+steal2;

    long deltaTotal = total2 - total1;
    long idleTime1=idle1+iowait1;
    long idleTime2=idle2+iowait2;
    long deltaIdle  = idleTime2 - idleTime1;

    if (deltaTotal == 0) return 0.0;

    return 100.0 * (deltaTotal - deltaIdle) / deltaTotal;
}

long Monitor::getMemoryUsage() {
    std::ifstream file("/proc/self/status");
     if(!file.is_open()){
         std::cerr << "Error: Unable to open /proc/status\n";
        return 0.0;
    }
    std::string line;

    while (std::getline(file, line)) {
        if (line.find("VmRSS:") == 0) {
            std::istringstream ss(line);
            std::string key;
            long value;
            std::string unit;
            ss >> key >> value >> unit;
            return value / 1024.0;  // Convert KB to MB
        }
    }
    return 0;
}