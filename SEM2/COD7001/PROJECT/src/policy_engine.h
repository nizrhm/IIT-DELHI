#ifndef POLICY_ENGINE_H
#define POLICY_ENGINE_H

#include "scheduler.h"
class policyEngine{
    public:
     Scheduling_Policy decidepolicy(double cpuUsage,double memoryUsage);
};
#endif
