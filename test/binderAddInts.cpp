/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/*
 * Binder add integers benchmark
 *
 * Measures the rate at which a short binder IPC operation can be
 * performed.  The operation consists of the client sending a parcel
 * that contains two integers.  For each parcel that the server
 * receives, it adds the two integers and sends the sum back to
 * the client.
 *
 * This benchmark supports the following command-line options:
 *
 *   -c cpu - bind client to specified cpu (default: unbound)
 *   -s cpu - bind server to specified cpu (default: unbound)
 *   -n num - perform IPC operation num times (default: 1000)
 *   -d time - delay specified amount of seconds after each
 *             IPC operation. (default 1e-3)
 */

#include <cerrno>
#include <grp.h>
#include <iostream>
#include <libgen.h>
#include <time.h>
#include <unistd.h>
#include <float.h>

#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <utils/Log.h>
#include "testUtil.h"

using namespace android;
using namespace std;

const int unbound = -1; // Indicator for a thread not bound to a specific CPU

String16 serviceName("test.binderAddInts");

struct options {
    int serverCPU;
    int clientCPU;
    unsigned int iterations;
    float        iterDelay; // End of iteration delay in seconds
} options = { // Set defaults
    unbound, // Server CPU
    unbound, // Client CPU
    1000,    // Iterations
    1e-3,    // End of iteration delay
};

class AddIntsService : public BBinder
{
  public:
    AddIntsService(int cpu = unbound);
    virtual ~AddIntsService() {}

    enum command {
        ADD_INTS = 0x120,
    };

    virtual status_t onTransact(uint32_t code,
                                const Parcel& data, Parcel* reply,
                                uint32_t flags = 0);

  private:
    int cpu_;
};

// File scope function prototypes
static void server(void);
static void client(void);
static void bindCPU(unsigned int cpu);
static ostream &operator<<(ostream &stream, const String16& str);
static ostream &operator<<(ostream &stream, const cpu_set_t& set);

int main(int argc, char *argv[])
{
    int rv;

    // Determine CPUs available for use.
    // This testcase limits its self to using CPUs that were
    // available at the start of the benchmark.
    cpu_set_t availCPUs;
    if ((rv = sched_getaffinity(0, sizeof(availCPUs), &availCPUs)) != 0) {
        cerr << "sched_getaffinity failure, rv: " << rv
            << " errno: " << errno << endl;
        exit(1);
    }

    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "s:c:n:d:?")) != -1) {
        char *chptr; // character pointer for command-line parsing

        switch (opt) {
        case 'c': // client CPU
        case 's': { // server CPU
            // Parse the CPU number
            int cpu = strtoul(optarg, &chptr, 10);
            if (*chptr != '\0') {
                cerr << "Invalid cpu specified for -" << (char) opt
                    << " option of: " << optarg << endl;
                exit(2);
            }

            // Is the CPU available?
            if (!CPU_ISSET(cpu, &availCPUs)) {
                cerr << "CPU " << optarg << " not currently available" << endl;
                cerr << "  Available CPUs: " << availCPUs << endl;
                exit(3);
            }

            // Record the choice
            *((opt == 'c') ? &options.clientCPU : &options.serverCPU) = cpu;
            break;
        }

        case 'n': // iterations
            options.iterations = strtoul(optarg, &chptr, 10);
            if (*chptr != '\0') {
                cerr << "Invalid iterations specified of: " << optarg << endl;
                exit(4);
            }
            if (options.iterations < 1) {
                cerr << "Less than 1 iteration specified by: "
                    << optarg << endl;
                exit(5);
            }
            break;

        case 'd': // Delay between each iteration
            options.iterDelay = strtod(optarg, &chptr);
            if ((*chptr != '\0') || (options.iterDelay < 0.0)) {
                cerr << "Invalid delay specified of: " << optarg << endl;
                exit(6);
            }
            break;

        case '?':
        default:
            cerr << basename(argv[0]) << " [options]" << endl;
            cerr << "  options:" << endl;
            cerr << "    -s cpu - server CPU number" << endl;
            cerr << "    -c cpu - client CPU number" << endl;
            cerr << "    -n num - iterations" << endl;
            cerr << "    -d time - delay after operation in seconds" << endl;
            exit(((optopt == 0) || (optopt == '?')) ? 0 : 7);
        }
    }

    // Display selected options
    cout << "serverCPU: ";
    if (options.serverCPU == unbound) {
        cout << " unbound";
    } else {
        cout << options.serverCPU;
    }
    cout << endl;
    cout << "clientCPU: ";
    if (options.clientCPU == unbound) {
        cout << " unbound";
    } else {
        cout << options.clientCPU;
    }
    cout << endl;
    cout << "iterations: " << options.iterations << endl;
    cout << "iterDelay: " << options.iterDelay << endl;

    // Fork client, use this process as server
    fflush(stdout);
    switch (pid_t pid = fork()) {
    case 0: // Child
        client();
        return 0;

    default: // Parent
        server();

        // Wait for all children to end
        do {
            int stat;
            rv = wait(&stat);
            if ((rv == -1) && (errno == ECHILD)) { break; }
            if (rv == -1) {
                cerr << "wait failed, rv: " << rv << " errno: "
                    << errno << endl;
                perror(NULL);
                exit(8);
            }
        } while (1);
        return 0;

    case -1: // Error
        exit(9);
    }

    return 0;
}

static void server(void)
{
    int rv;

    // Add the service
    sp<ProcessState> proc(ProcessState::self());
    sp<IServiceManager> sm = defaultServiceManager();
    if ((rv = sm->addService(serviceName,
        new AddIntsService(options.serverCPU))) != 0) {
        cerr << "addService " << serviceName << " failed, rv: " << rv
            << " errno: " << errno << endl;
    }

    // Start threads to handle server work
    proc->startThreadPool();
}

static void client(void)
{
    int rv;
    sp<IServiceManager> sm = defaultServiceManager();
    double min = FLT_MAX, max = 0.0, total = 0.0; // Time in seconds for all
                                                  // the IPC calls.

    // If needed bind to client CPU
    if (options.clientCPU != unbound) { bindCPU(options.clientCPU); }

    // Attach to service
    sp<IBinder> binder;
    do {
        binder = sm->getService(serviceName);
        if (binder != 0) break;
        cout << serviceName << " not published, waiting..." << endl;
        usleep(500000); // 0.5 s
    } while(true);

    // Perform the IPC operations
    for (unsigned int iter = 0; iter < options.iterations; iter++) {
        Parcel send, reply;

        // Create parcel to be sent.  Will use the iteration cound
        // and the iteration count + 3 as the two integer values
        // to be sent.
        int val1 = iter;
        int val2 = iter + 3;
        int expected = val1 + val2;  // Expect to get the sum back
        send.writeInt32(val1);
        send.writeInt32(val2);

        // Send the parcel, while timing how long it takes for
        // the answer to return.
        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);
        if ((rv = binder->transact(AddIntsService::ADD_INTS,
            send, &reply)) != 0) {
            cerr << "binder->transact failed, rv: " << rv
                << " errno: " << errno << endl;
            exit(10);
        }
        struct timespec current;
        clock_gettime(CLOCK_MONOTONIC, &current);

        // Calculate how long this operation took and update the stats
        struct timespec deltaTimespec = tsDelta(&start, &current);
        double delta = ts2double(&deltaTimespec);
        min = (delta < min) ? delta : min;
        max = (delta > max) ? delta : max;
        total += delta;
        int result = reply.readInt32();
        if (result != (int) (iter + iter + 3)) {
            cerr << "Unexpected result for iteration " << iter << endl;
            cerr << "  result: " << result << endl;
            cerr << "expected: " << expected << endl;
        }

        if (options.iterDelay > 0.0) { testDelaySpin(options.iterDelay); }
    }

    // Display the results
    cout << "Time per iteration min: " << min
        << " avg: " << (total / options.iterations)
        << " max: " << max
        << endl;
}

AddIntsService::AddIntsService(int cpu): cpu_(cpu) {
    if (cpu != unbound) { bindCPU(cpu); }
}

// Server function that handles parcels received from the client
status_t AddIntsService::onTransact(uint32_t code, const Parcel &data,
                                    Parcel* reply, uint32_t flags) {
    int val1, val2;
    status_t rv(0);
    int cpu;

    // If server bound to a particular CPU, check that
    // were executing on that CPU.
    if (cpu_ != unbound) {
        cpu = sched_getcpu();
        if (cpu != cpu_) {
            cerr << "server onTransact on CPU " << cpu << " expected CPU "
                  << cpu_ << endl;
            exit(20);
        }
    }

    // Perform the requested operation
    switch (code) {
    case ADD_INTS:
        val1 = data.readInt32();
        val2 = data.readInt32();
        reply->writeInt32(val1 + val2);
        break;

    default:
      cerr << "server onTransact unknown code, code: " << code << endl;
      exit(21);
    }

    return rv;
}

static void bindCPU(unsigned int cpu)
{
    int rv;
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    rv = sched_setaffinity(0, sizeof(cpuset), &cpuset);

    if (rv != 0) {
        cerr << "bindCPU failed, rv: " << rv << " errno: " << errno << endl;
        perror(NULL);
        exit(30);
    }
}

static ostream &operator<<(ostream &stream, const String16& str)
{
    for (unsigned int n1 = 0; n1 < str.size(); n1++) {
        if ((str[n1] > 0x20) && (str[n1] < 0x80)) {
            stream << (char) str[n1];
        } else {
            stream << '~';
        }
    }

    return stream;
}

static ostream &operator<<(ostream &stream, const cpu_set_t& set)
{
    for (unsigned int n1 = 0; n1 < CPU_SETSIZE; n1++) {
        if (CPU_ISSET(n1, &set)) {
            if (n1 != 0) { stream << ' '; }
            stream << n1;
        }
    }

    return stream;
}
