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

#include "testUtil.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/time.h>
#include <sys/wait.h>

#include <cutils/log.h>

#define ALEN(a) (sizeof(a) / sizeof(a [0]))  // Array length
typedef unsigned int bool_t;
#define true (0 == 0)
#define false (!true)

#define MAXSTR 200

static const char *logCatTag;
static const unsigned int uSecsPerSec = 1000000;
static const unsigned int nSecsPerSec = 1000000000;

// struct timespec to double
double ts2double(const struct timespec *val)
{
    double rv;

    rv = val->tv_sec;
    rv += (double) val->tv_nsec / nSecsPerSec;

    return rv;
}

// struct timeval to double
double tv2double(const struct timeval *val)
{
    double rv;

    rv = val->tv_sec;
    rv += (double) val->tv_usec / uSecsPerSec;

    return rv;
}

// double to struct timespec
struct timespec double2ts(double amt)
{
    struct timespec rv;

    rv.tv_sec = floor(amt);
    rv.tv_nsec = (amt - rv.tv_sec) * nSecsPerSec;
    // TODO: Handle cases where amt is negative
    while ((unsigned) rv.tv_nsec >= nSecsPerSec) {
        rv.tv_nsec -= nSecsPerSec;
        rv.tv_sec++;
    }

    return rv;
}

// double to struct timeval
struct timeval double2tv(double amt)
{
    struct timeval rv;

    rv.tv_sec = floor(amt);
    rv.tv_usec = (amt - rv.tv_sec) * uSecsPerSec;
    // TODO: Handle cases where amt is negative
    while ((unsigned) rv.tv_usec >= uSecsPerSec) {
        rv.tv_usec -= uSecsPerSec;
        rv.tv_sec++;
    }

    return rv;
}

// Delta (difference) between two struct timespec.
// It is expected that the time given by the structure pointed to by
// second, is later than the time pointed to by first.
struct timespec tsDelta(const struct timespec *first,
                        const struct timespec *second)
{
    struct timespec rv;

    assert(first != NULL);
    assert(second != NULL);
    assert(first->tv_nsec >= 0 && first->tv_nsec < nSecsPerSec);
    assert(second->tv_nsec >= 0 && second->tv_nsec < nSecsPerSec);
    rv.tv_sec = second->tv_sec - first->tv_sec;
    if (second->tv_nsec >= first->tv_nsec) {
        rv.tv_nsec = second->tv_nsec - first->tv_nsec;
    } else {
        rv.tv_nsec = (second->tv_nsec + nSecsPerSec) - first->tv_nsec;
        rv.tv_sec--;
    }

    return rv;
}

// Delta (difference) between two struct timeval.
// It is expected that the time given by the structure pointed to by
// second, is later than the time pointed to by first.
struct timeval tvDelta(const struct timeval *first,
                       const struct timeval *second)
{
    struct timeval rv;

    assert(first != NULL);
    assert(second != NULL);
    assert(first->tv_usec >= 0 && first->tv_usec < uSecsPerSec);
    assert(second->tv_usec >= 0 && second->tv_usec < uSecsPerSec);
    rv.tv_sec = second->tv_sec - first->tv_sec;
    if (second->tv_usec >= first->tv_usec) {
        rv.tv_usec = second->tv_usec - first->tv_usec;
    } else {
        rv.tv_usec = (second->tv_usec + uSecsPerSec) - first->tv_usec;
        rv.tv_sec--;
    }

    return rv;
}

void testPrint(FILE *stream, const char *fmt, ...)
{
    char line[MAXSTR];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    if (stream == stderr) {
        ALOG(LOG_ERROR, logCatTag, "%s", line);
    } else {
        ALOG(LOG_INFO, logCatTag, "%s", line);
    }
    vfprintf(stream, fmt, args);
    fputc('\n', stream);
}

// Set tag used while logging to the logcat error interface
void testSetLogCatTag(const char *tag)
{
    logCatTag = tag;
}

// Obtain pointer to current log to logcat error interface tag
const char * testGetLogCatTag(void)
{
    return logCatTag;
}

/*
 * Random
 *
 * Returns a pseudo random number in the range [0:2^32-1].
 *
 * Precondition: srand48() called to set the seed of
 *   the pseudo random number generator.
 */
uint32_t testRand(void)
{
    uint32_t val;

    // Use lrand48() to obtain 31 bits worth
    // of randomness.
    val = lrand48();

    // Make an additional lrand48() call and merge
    // the randomness into the most significant bits.
    val ^= lrand48() << 1;

    return val;
}

/*
 * Random Modulus
 *
 * Pseudo randomly returns unsigned integer in the range [0, mod).
 *
 * Precondition: srand48() called to set the seed of
 *   the pseudo random number generator.
 */
uint32_t testRandMod(uint32_t mod)
{
    // Obtain the random value
    // Use lrand48() when it would produce a sufficient
    // number of random bits, otherwise use testRand().
    const uint32_t lrand48maxVal = ((uint32_t) 1 << 31) - 1;
    uint32_t val = (mod <= lrand48maxVal) ? (uint32_t) lrand48() : testRand();

    /*
     * The contents of individual bytes tend to be less than random
     * across different seeds.  For example, srand48(x) and
     * srand48(x + n * 4) cause lrand48() to return the same sequence of
     * least significant bits.  For small mod values this can produce
     * noticably non-random sequnces.  For mod values of less than 2
     * bytes, will use the randomness from all the bytes.
     */
    if (mod <= 0x10000) {
        val = (val & 0xffff) ^ (val >> 16);

        // If mod less than a byte, can further combine down to
        // a single byte.
        if (mod <= 0x100) {
            val = (val & 0xff) ^ (val >> 8);
        }
    }

    return val % mod;
}

/*
 * Random Boolean
 *
 * Pseudo randomly returns 0 (false) or 1 (true).
 *
 * Precondition: srand48() called to set the seed of
 *   the pseudo random number generator.
 */
int testRandBool(void)
{
    return (testRandMod(2));
}

/*
 * Random Fraction
 *
 * Pseudo randomly return a value in the range [0.0, 1.0).
 *
 * Precondition: srand48() called to set the seed of
 *   the pseudo random number generator.
 */
double testRandFract(void)
{
    return drand48();
}

// Delays for the number of seconds specified by amt or a greater amount.
// The amt variable is of type float and thus non-integer amounts
// of time can be specified.  This function automatically handles cases
// where nanosleep(2) returns early due to reception of a signal.
void testDelay(float amt)
{
    struct timespec   start, current, delta;
    struct timespec   remaining;

    // Get the time at which we started
    clock_gettime(CLOCK_MONOTONIC, &start);

    do {
        // Get current time
        clock_gettime(CLOCK_MONOTONIC, &current);

        // How much time is left
        delta = tsDelta(&start, &current);
        if (ts2double(&delta) > amt) { break; }

        // Request to sleep for the remaining time
        remaining = double2ts(amt - ts2double(&delta));
        (void) nanosleep(&remaining, NULL);
    } while (true);
}

// Delay spins for the number of seconds specified by amt or a greater
// amount.  The amt variable is of type float and thus non-integer amounts
// of time can be specified.  Differs from testDelay() in that
// testDelaySpin() performs a spin loop, instead of using nanosleep().
void testDelaySpin(float amt)
{
    struct timespec   start, current, delta;

    // Get the time at which we started
    clock_gettime(CLOCK_MONOTONIC, &start);

    do {
        // Get current time
        clock_gettime(CLOCK_MONOTONIC, &current);

        // How much time is left
        delta = tsDelta(&start, &current);
        if (ts2double(&delta) > amt) { break; }
    } while (true);
}

/*
 * Hex Dump
 *
 * Displays in hex the contents of the memory starting at the location
 * pointed to by buf, for the number of bytes given by size.
 * Each line of output is indented by a number of spaces that
 * can be set by calling xDumpSetIndent().  It is also possible
 * to offset the displayed address by an amount set by calling
 * xDumpSetOffset.
 */
static uint8_t     xDumpIndent;
static uint64_t    xDumpOffset;
void
testXDump(const void *buf, size_t size)
{
    const unsigned int bytesPerLine = 16;
    int rv;
    char line[MAXSTR];
    // const unsigned char *ptr = buf, *start = buf;
    const unsigned char *ptr = (const unsigned char*) buf;
    const unsigned char *start = (const unsigned char*) buf;
    size_t num = size;
    char *linep = line;

    while (num) {
        if (((ptr - start) % bytesPerLine) == 0) {
            if (linep != line) {
                testPrintE("%s", line);
            }
            linep = line;
            rv = snprintf(linep, ALEN(line) - (linep - line),
                "%*s%06llx:", xDumpIndent, "",
                (long long) (ptr - start) + xDumpOffset);
            linep += rv;
        }

        // Check that there is at least room for 4
        // more characters.  The 4 characters being
        // a space, 2 hex digits and the terminating
        // '\0'.
        assert((ALEN(line) - 4) >= (linep - line));
        rv = snprintf(linep, ALEN(line) - (linep - line),
            " %02x", *ptr++);
        linep += rv;
        num--;
    }
    if (linep != line) {
        testPrintE("%s", line);
    }
}

// Set an indent of spaces for each line of hex dump output
void
testXDumpSetIndent(uint8_t indent)
{
    xDumpIndent = indent;
}

// Obtain the current hex dump indent amount
uint8_t
testXDumpGetIndent(void)
{
    return xDumpIndent;
}

// Set the hex dump address offset amount
void
testXDumpSetOffset(uint64_t offset)
{
    xDumpOffset = offset;
}

// Get the current hex dump address offset amount
uint64_t
testXDumpGetOffset(void)
{
    return xDumpOffset;
}

/*
 * Execute Command
 *
 * Executes the command pointed to by cmd.  Output from the
 * executed command is captured and sent to LogCat Info.  Once
 * the command has finished execution, it's exit status is captured
 * and checked for an exit status of zero.  Any other exit status
 * causes diagnostic information to be printed and an immediate
 * testcase failure.
 */
void testExecCmd(const char *cmd)
{
    FILE *fp;
    int rv;
    int status;
    char str[MAXSTR];

    // Display command to be executed
    testPrintI("cmd: %s", cmd);

    // Execute the command
    fflush(stdout);
    if ((fp = popen(cmd, "r")) == NULL) {
        testPrintE("execCmd popen failed, errno: %i", errno);
        exit(100);
    }

    // Obtain and display each line of output from the executed command
    while (fgets(str, sizeof(str), fp) != NULL) {
        if ((strlen(str) > 1) && (str[strlen(str) - 1] == '\n')) {
            str[strlen(str) - 1] = '\0';
        }
        testPrintI(" out: %s", str);
    }

    // Obtain and check return status of executed command.
    // Fail on non-zero exit status
    status = pclose(fp);
    if (!(WIFEXITED(status) && (WEXITSTATUS(status) == 0))) {
        testPrintE("Unexpected command failure");
        testPrintE("  status: %#x", status);
        if (WIFEXITED(status)) {
            testPrintE("WEXITSTATUS: %i", WEXITSTATUS(status));
        }
        if (WIFSIGNALED(status)) {
            testPrintE("WTERMSIG: %i", WTERMSIG(status));
        }
        exit(101);
    }
}
