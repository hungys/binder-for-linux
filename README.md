binder-for-linux
================

binder-for-linux is an experimental project to evaluate the feasibility of porting Android Binder IPC subsystem to Ubuntu Linux.

# Environment

- Ubuntu 14.04.4 LTS
- Linux Kernel 4.2.0-27
- gcc 4.9.3

# Codebase

- Binder driver: mainline kernel **4.2.0-27-generic**
- Android Framework version: **android-6.0.1_r46**

# Modifications

- Make binder & ashmem driver dynamically loadable
    - Modified as a standard loadable kernel module
    - Use `kallsyms` and a shell script to generate symbol dependencies that are not exposed by kernel (e.g. `get_vm_area`)
- Fix compatability issues after replacing Bionic C library with glibc
- Fix compatibility issues of atomic library
    - Some are changed to use standard C++ atomic library (C++11 and gcc 4.9.3+ are required)
    - `cutils/atomic.h` is re-implemented with x86 atomic instructions
- Dependencies to Android log daemon and SELinux library are removed
- And other small fixes...

# Get Started

To compile all from source,

```
$ ./project.sh makeall
```

Now you can install kernel modules (binder & ashmem) by,

```
$ ./project.sh insmod
```

Then run Service Manager in background,

```
$ sudo servicemanager/servicemanager &
```

We also prepared a benchmark program to perform correctness test and performance test,

```
$ sudo test/binderAddInts -n 100 -p 0   # correctness test with 100 iterations
$ sudo test/binderAddInts -n 10000 -p 4096   # performance test with 4K payload and 10000 iterations
```

# Results

![Performance Evaluation](http://i.imgur.com/Oa8csYS.png)

We found an obvious fact that when the payload size is greater than 16K, the transmission latency grows much more rapidly compared with a small payload. After tracing more source code, we inferred that this may be due to the use of the single **global lock**. The Binder driver heavily uses a global lock to protect the critical sections, this may lead to low utilization in high concurrency situation. Also, during the driver initialization phase, the Binder driver created a bottom half – workqueue, to handle the release of system resource and buffer. During the benchmark with larger payload and higher iterations (e.g. 10000 per test), the workqueue need to handle the release request more frequently; however, the implementation of release function also need to acquire the global lock, which leads to much lower performance for real data transmission.