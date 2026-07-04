/**
 * @file Metrics.cpp
 * @brief Implementation of system metrics and CSV logging.
 */

#include "Metrics.h"
#include "runtime.h"
#include <iostream>
void Metrics::starttime() { start_time = chrono::high_resolution_clock::now(); }
void Metrics::endtime() { end_time = chrono::high_resolution_clock::now(); }
Metrics::Metrics() {
  completed_task = 0;
  context_switch = 0;
  total = 0;
  idle = 0;
  total_overhead = 0;
  completions_per_core.assign(4, 0); // Default to 4 cores max
}
void Metrics::completetask(int core_id) {
  completed_task++;
  if (core_id >= 0) {
    if (core_id >= (int)completions_per_core.size()) {
      completions_per_core.resize(core_id + 1, 0);
    }
    completions_per_core[core_id]++;
  }
}
int Metrics::getCoreCompletions(int core_id) {
  if (core_id >= 0 && core_id < (int)completions_per_core.size()) {
    return completions_per_core[core_id];
  }
  return 0;
}
int Metrics::getcompletedtask() { return completed_task; }
void Metrics::print() {
  cout << "Total Time:  " << total << endl;
  cout << "IDLE Time:  " << idle << endl;
  cout << "Complete Tasks: " << completed_task << endl;
  cout << "Total Context Switch: " << getcontextswitch() << endl;
  cout << "Monitoring Overhead: " << total_overhead << " seconds" << endl;
  saveToCSV();
}
void Metrics::saveToCSV() {
  std::string filename = "evaluation_results.csv";
  std::ifstream check(filename);
  bool exists = check.good();
  check.close();

  std::ofstream out(filename, std::ios::app);
  if (out.is_open()) {
    if (!exists) {
      out << "Total_Cycles,Idle_Cycles,Completed_Tasks,Context_Switches,Monitoring_Overhead_Sec\n";
    }

    out << total << "," << idle << "," << completed_task << ","
        << context_switch << "," << total_overhead << "\n";
    out.close();
    cout << "Results saved to evaluation_results.csv" << endl;
  }
}
void Metrics::addoverhead(double t) { total_overhead += t; }
double Metrics::getoverhead() { return total_overhead; }
void Metrics::contextswitch() { context_switch++; }
int Metrics::getcontextswitch() { return context_switch; }
void Metrics::settotaltime(int t) { total = t; }
void Metrics::setidletime(int t) { idle = t; }
double Metrics::getActualDuration() {
  auto duration =
      chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
  return duration.count() / 1000.0;
}

void Metrics::reset() {
  completed_task = 0;
  context_switch = 0;
  total = 0;
  idle = 0;
  total_overhead = 0;
  completions_per_core.assign(4, 0);
}