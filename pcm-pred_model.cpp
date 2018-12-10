/*
Copyright (c) 2009-2017, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
* Neither the name of Intel Corporation nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


/*!     \file pcm-pred_model.cpp
\brief Example of using CPU counters: implements a simple performance counter monitoring utility
*/
#define HACK_TO_REMOVE_DUPLICATE_ERROR
#include <iostream>
#ifdef _MSC_VER
#include <windows.h>
#include "../PCM_Win/windriver.h"
#else
#include <unistd.h>
#include <signal.h>   // for atexit()
#include <sys/time.h> // for gettimeofday()
#endif
#include <math.h>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cstring>
#include <sstream>
#include <assert.h>
#include <bitset>
#include <fcntl.h>
#include "cpucounters.h"
#include "utils.h"

#define SIZE (10000000)
#define PCM_DELAY_DEFAULT 0.1 // in seconds
#define PCM_DELAY_MIN 0.015 // 15 milliseconds is practical on most modern CPUs
#define PCM_CALIBRATION_INTERVAL 50 // calibrate clock only every 50th iteration
#define MAX_CORES 4096

using namespace std;

template <class IntType>
double float_format(IntType n)
{
    return double(n) / 1e6;
}

unsigned long long int total_tick_old[9];
unsigned long long int idle_old[9];

double freq(int Num){

	FILE *freq_read;
	double freq;
	double FREQ;
	char read_file[100];
	sprintf(read_file, "/sys/devices/system/cpu/cpufreq/policy%d/scaling_cur_freq", Num);
	freq_read = fopen(read_file, "r");
	if(freq_read == NULL){
		perror ("Error");
	}
	
	fscanf(freq_read, "%lf", &freq);
	FREQ = freq/1000000.0;
	fclose(freq_read);

	return FREQ;
}

double util(int Num){
	FILE *stat_read;
	long long int fields[9][11];
	long long int total_tick;
	long long int idle;
	long long int del_total_tick, del_idle;
	int i = Num + 1, j;
	double percent_usage;
	int retval;
	char buffer[1000];

	stat_read = fopen("/proc/stat", "r");
	if(stat_read == NULL){
		perror("Error");
	}
	

	for(int i = 0; i < 9; i++){
		retval = fscanf(stat_read, "%s %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld",
				buffer,
    	                        &fields[i][0], 
    	                        &fields[i][1], 
    	                        &fields[i][2], 
    	                        &fields[i][3], 
    	                        &fields[i][4], 
    	                        &fields[i][5], 
    	                        &fields[i][6], 
    	                        &fields[i][7], 
    	                        &fields[i][8], 
    	                        &fields[i][9]);
	}
	
	if(retval < 4)
	{
		perror("Error");
	}

	
	for(j = 0, total_tick = 0; j < 10; j++){
			total_tick += fields[i][j];
	}
	idle = fields[i][3];
	del_total_tick = total_tick - total_tick_old[i];
	del_idle = idle - idle_old[i];
	total_tick_old[i] = total_tick;
	idle_old[i] = idle;

	percent_usage = ((del_total_tick - del_idle) / (double) del_total_tick);
	if(del_total_tick == 0) percent_usage = 0.0;
	fclose(stat_read);
	return percent_usage;
}

void print_output(PCM * m,
    const SystemCounterState& sstate1,
    const SystemCounterState& sstate2,
    const bool show_system_output,
    FILE *output_file
    )
{
    cout.precision(2);
    cout << std::fixed;

	fprintf(output_file, "%f ", getExecUsage(sstate1, sstate2));
	fprintf(output_file, "%f ", getIPC(sstate1, sstate2));
	fprintf(output_file, "%f ", getRelativeFrequency(sstate1, sstate2));
	fprintf(output_file, "%f ", getActiveRelativeFrequency(sstate1, sstate2));
	fprintf(output_file, "%f ", double(getL3CacheMisses(sstate1, sstate2)) / getInstructionsRetired(sstate1, sstate2));
	fprintf(output_file, "%f ", getBytesReadFromMC(sstate1, sstate2) / double(1e9));
	fprintf(output_file, "%f ", getBytesWrittenToMC(sstate1, sstate2) / double(1e9));
	fprintf(output_file, "%f ", float_format(getInstructionsRetired(sstate1, sstate2)));
	fprintf(output_file, "%f ", getCoreIPC(sstate1, sstate2));
	fprintf(output_file, "%f ", getTotalExecUsage(sstate1, sstate2));
	fprintf(output_file, "%f ", getConsumedJoules(sstate1, sstate2));
	fprintf(output_file, "%f\n", util(-1));

}


int main(int argc, char * argv[])
{
    set_signal_handlers();

#ifdef PCM_FORCE_SILENT
    null_stream nullStream1, nullStream2;
    std::cout.rdbuf(&nullStream1);
    std::cerr.rdbuf(&nullStream2);
#endif

    cerr << endl;
    cerr << " Processor Counter Monitor " << PCM_VERSION << endl;
    cerr << endl;
    
    cerr << endl;

    // if delay is not specified: use either default (1 second),
    // or only read counters before or after PCM started: keep PCM blocked
    double delay = -1.0;

    char *sysCmd = NULL;
    char **sysArgv = NULL;
    bool show_system_output = true;
    bool csv_output = false;
    bool reset_pmu = false;
    bool allow_multiple_instances = false;
    bool disable_JKT_workaround = false; // as per http://software.intel.com/en-us/articles/performance-impact-when-sampling-certain-llc-events-on-snb-ep-with-vtune

    long diff_usec = 0; // deviation of clock is useconds between measurements
    int calibrated = PCM_CALIBRATION_INTERVAL - 2; // keeps track is the clock calibration needed
    unsigned int numberOfIterations = 0; // number of iterations
    string program = string(argv[0]);

    PCM * m = PCM::getInstance();

    if (reset_pmu)
    {
        cerr << "\n Resetting PMU configuration" << endl;
        m->resetPMU();
    }

    if (allow_multiple_instances)
    {
        m->allowMultipleInstances();
    }

    // program() creates common semaphore for the singleton, so ideally to be called before any other references to PCM
    PCM::ErrorCode status = m->program();

    switch (status)
    {
    case PCM::Success:
        break;
    case PCM::MSRAccessDenied:
        cerr << "Access to Processor Counter Monitor has denied (no MSR or PCI CFG space access)." << endl;
        exit(EXIT_FAILURE);
    case PCM::PMUBusy:
        cerr << "Access to Processor Counter Monitor has denied (Performance Monitoring Unit is occupied by other application). Try to stop the application that uses PMU." << endl;
        cerr << "Alternatively you can try running PCM with option -r to reset PMU configuration at your own risk." << endl;
        exit(EXIT_FAILURE);
    default:
        cerr << "Access to Processor Counter Monitor has denied (Unknown error)." << endl;
        exit(EXIT_FAILURE);
    }

    std::vector<CoreCounterState> cstates1, cstates2;
    std::vector<SocketCounterState> sktstate1, sktstate2;
    SystemCounterState sstate1, sstate2;
    uint64 TimeAfterSleep = 0;
    PCM_UNUSED(TimeAfterSleep);

    if ((sysCmd != NULL) && (delay <= 0.0)) {
        // in case external command is provided in command line, and
        // delay either not provided (-1) or is zero
        m->setBlocked(true);
    }
    else {
        m->setBlocked(false);
    }

    if (csv_output) {
        
    }
    else {
        // for non-CSV mode delay < 1.0 does not make a lot of practical sense:
        // hard to read from the screen, or
        // in case delay is not provided in command line => set default
        if (((delay<1.0) && (delay>0.0)) || (delay <= 0.0)) delay = PCM_DELAY_DEFAULT;
    }
    // cerr << "DEBUG: Delay: " << delay << " seconds. Blocked: " << m->isBlocked() << endl;

    m->getAllCounterStates(sstate1, sktstate1, cstates1);

    if (sysCmd != NULL) {
        MySystem(sysCmd, sysArgv);
    }

    unsigned int i = 1;

    FILE *file;
    char output_file[1000]="current_state";

    while ((i <= numberOfIterations) || (numberOfIterations == 0))
    {
        if (!csv_output) cout << std::flush;
        int delay_ms = int(delay * 1000);
        int calibrated_delay_ms = delay_ms;
#ifdef _MSC_VER
        // compensate slow Windows console output
        if (TimeAfterSleep) delay_ms -= (uint32)(m->getTickCount() - TimeAfterSleep);
        if (delay_ms < 0) delay_ms = 0;
#else
        // compensation of delay on Linux/UNIX
        // to make the sampling interval as monotone as possible
        struct timeval start_ts, end_ts;
        if (calibrated == 0) {
            gettimeofday(&end_ts, NULL);
            diff_usec = (end_ts.tv_sec - start_ts.tv_sec)*1000000.0 + (end_ts.tv_usec - start_ts.tv_usec);
            calibrated_delay_ms = delay_ms - diff_usec / 1000.0;
        }
#endif

        if (sysCmd == NULL || numberOfIterations != 0 || m->isBlocked() == false)
        {
            MySleepMs(calibrated_delay_ms);
        }

#ifndef _MSC_VER
        calibrated = (calibrated + 1) % PCM_CALIBRATION_INTERVAL;
        if (calibrated == 0) {
            gettimeofday(&start_ts, NULL);
        }
#endif
        TimeAfterSleep = m->getTickCount();

        m->getAllCounterStates(sstate2, sktstate2, cstates2);

        file = fopen(output_file, "w");
        print_output(m, sstate1, sstate2, show_system_output, file);
	fclose(file);

        // sanity checks
        assert(getNumberOfCustomEvents(0, sstate1, sstate2) == getL3CacheMisses(sstate1, sstate2));
        if (m->useSkylakeEvents()) {
            assert(getNumberOfCustomEvents(1, sstate1, sstate2) == getL3CacheHits(sstate1, sstate2));
            assert(getNumberOfCustomEvents(2, sstate1, sstate2) == getL2CacheMisses(sstate1, sstate2));
        }
        else {
            assert(getNumberOfCustomEvents(1, sstate1, sstate2) == getL3CacheHitsNoSnoop(sstate1, sstate2));
            assert(getNumberOfCustomEvents(2, sstate1, sstate2) == getL3CacheHitsSnoop(sstate1, sstate2));
        }
        assert(getNumberOfCustomEvents(3, sstate1, sstate2) == getL2CacheHits(sstate1, sstate2));

        std::swap(sstate1, sstate2);

        if (m->isBlocked()) {
            // in case PCM was blocked after spawning child application: break monitoring loop here
            break;
        }

        ++i;
    }

    exit(EXIT_SUCCESS);
}
    
