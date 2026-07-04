/**
 * @file Metrics.h
 * @brief Performance metric tracking for the Resource Manager.
 */

#ifndef METRICS_H
#define METRICS_H
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>
using namespace std;
class Metrics {
private:
  int completed_task = 0;
  int context_switch = 0;
  int total = 0;
  int idle = 0;
  double total_overhead = 0;
  std::vector<int> completions_per_core;
  chrono::time_point<chrono::high_resolution_clock> start_time;
  chrono::time_point<chrono::high_resolution_clock> end_time;

public:
  Metrics();
  void starttime();
  void endtime();
  void completetask(int core_id = -1);
  int getcompletedtask();
  int getCoreCompletions(int core_id);
  void print();
  void saveToCSV();
  void contextswitch();
  int getcontextswitch();
  void settotaltime(int t);
  void setidletime(int t);
  void addoverhead(double t);
  double getoverhead();
  double getActualDuration();
  void reset();
};

#endif