#pragma once

#include "exit_codes.h"
#include "timer.h"
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

// volatile sig_atomic_t terminationRequired = false;

bool lp_isTerminationRequired(void);
int lp_getTerminationExitCode(void);
void lp_eventLoopRun(void);
void lp_registerTerminationHandler(void);
void lp_terminate(int exitCode);
void lp_terminationHandler(int signalNumber);