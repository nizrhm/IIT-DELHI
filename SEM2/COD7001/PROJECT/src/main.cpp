#include "monitor.h"
#include "runtime.h"
#include "scheduler.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

int main(int argc, char *argv[]) {
  Runtime runtime;
  std::string filename = "workload.csv";

  // Pre-parse for filename to initialize correctly
  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
       if (std::string(argv[i]) == "-f" && i + 1 < argc) {
         filename = argv[i + 1];
         break;
       }
    }
  }

  runtime.initialize(filename);

  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];

      if (arg == "-f" && i + 1 < argc) {
          i++; // Already handled, skip
          continue;
      }
      
      // Handle core count: -cores N
      if (arg == "-cores" && i + 1 < argc) {
        try {
          int c = std::stoi(argv[i + 1]);
          runtime.setCoreCount(c);
          i++; // Skip the next argument
          continue;
        } catch (...) {
        }
      }

      if (arg == "-banker") {
        runtime.enableDeadlockDetector();
        continue;
      }

      // Normalize for policy checking
      std::string lowerArg = arg;
      for (auto &c : lowerArg)
        c = std::tolower(c);

      if (lowerArg == "-test" || lowerArg == "--test") {
        runtime.runComparison();
        return 0;
      }

      // Map string to policy enum (Case-insensitive)
      if (lowerArg == "fifo") {
        runtime.setpolicy(FIFO);
      } else if (lowerArg == "rr") {
        runtime.setpolicy(ROUNDROBIN);
      } else if (lowerArg == "prio" || lowerArg == "priority") {
        runtime.setpolicy(PRIORITY);
      } else if (lowerArg == "adaptive") {
        runtime.setpolicy(ADAPTIVE);
      } else if (lowerArg == "mlfq") {
        runtime.setpolicy(MLFQ);
      }
    }
  }

  runtime.runstep();

  return 0;
}