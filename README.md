# ebpfpub

ebpfpub is a Linux library that can be used to record system call activity. It works by generating eBPF programs that are then attached to the system tracepoints.

| | |
|-|-|
| CI Status | ![](https://github.com/trailofbits/ebpfpub/workflows/Build/badge.svg) |

## How it works

### Tracepoint events
Tracing is performed by attaching eBPF programs to the system tracepoints. For each syscall defined on the system, two distinct events are emitted: **enter** and **exit**.

The **enter** event happens as soon as the syscall is called, and carries all the parameters that have been passed from user mode. It does not contain the exit code, and all the **out** buffers are yet to be filled.

The **exit** event in constrast does not contain any of the **enter** parameters. It does provide the exit code however, and by this event is emitted all the **out** parameters have been filled.

### Handling events
ebfpub generates two different programs in order to handle both events; the **enter** program is responsible for taking a snapshot of all the parameters and is automatically generated by the library. This is enough to capture all parameters by value, but will not dereference pointers.

The **exit** program is special, because each system call behaves differently, so the library can only initialize the code and then defer the rest of the code generation step to a serializer that knows how each specific system call works. Since all the parameters have already been copied automatically by the library, the serializer's job is to dereference pointers (example: the sockaddr_in structure of the connect() system call) and make sure they are returned back to the library.

Provided with the library, a **generic** system call serializer is used as a fallback whenever a more specialized implementation is not available. This generator is capable of serializing null-terminated `char *` strings, making it suitable for handling more than half the system calls offered by the Linux kernel.

### Program generation
Programs are generated by parsing the tracepoint event format file found in the [ftrace](https://www.kernel.org/doc/Documentation/trace/ftrace.txt) directory. The **enter** program is responsible for generating the event entry, containing the event header (process id, thread id, timestamp, etc...) and the system call parameters. The **exit** program will then locate the generated event entry and, if necessary, perform additional tasks such as capturing strings and buffers. Once event entry is ready, it is sent back to user mode using the perf_events output.

### Program resources
Whenever a pair of eBPF programs is allocated to trace a system call, certain resources are required:

* **Event map**: A per-syscall map, used to store event entries. These entries are generated by the **enter** program, passed to the **exit** program, and then sent to user mode.
* **Event stack map**: A per-syscall map, used as stack space to work around the program stack limitations provided by the eBPF architecture. This is automatically allocated by the library.
* **Buffer storage**: This object is a combination of a set of indices and a map containing arrays. It is used to capture strings and buffers, and can be shared across multiple system call tracepoints. It is also possible to allocate private buffer storages for system calls that are more memory hungry than others.
* **Buffer storage stack**: A per-syscall map, not unlike the **Event stack map**, that is used to work around the eBPF stack limitations. It is automatically allocated by the library.
* **Perf output**: This object is the communication channel between the probe and user mode. It can be shared across multiple tracepoints, but it is also possible to create 

Some of the above resources require the use of per-CPU maps, which perform the same allocation once per CPU core. The Buffer Storage and Perf Output objects tend to consume quite a bit of memory, depending on how they are allocated. Both classes have methods to retrieve the exact amount of bytes allocated.

## Building

### Prerequisites
* A recent Clang/LLVM installation (8.0 or better), compiled with BPF support
* A recent libc++ or stdc++ library, supporting C++17
* CMake >= 3.16.2. A pre-built binary can be downloaded from the [CMake's download page](https://cmake.org/download/).
* Linux kernel >= 4.18 (Ubuntu 18.10)

Please note that LLVM itself must be compiled with libc++ when enabling the `EBPF_COMMON_ENABLE_LIBCPP` option, since ebfpub will directly link against the LLVM libraries.

### Dependencies
* [ebpf-common](https://github.com/trailofbits/ebpf-common)

### Building with the osquery toolchain (preferred)

**This should work fine on any recent Linux distribution.**

The osquery-toolchain needs to be obtained first, but version 1.0.0 does not yet ship with LLVM/Clang libraries. It is possible to download the 1.0.1 prerelease from https://alessandrogar.io/downloads/osquery-toolchain-1.0.1.tar.xz. See the following PR for more information: https://github.com/osquery/osquery-toolchain/pull/14

1. Obtain the source code: `git clone --recursive https://github.com/trailofbits/ebpfpub`
2. In case the `--recursive` flag was not provided, run `git submodule update --init --recursive`
3. Enter the source folder: `cd ebpfpub`
4. Create the build folder: `mkdir build && cd build`
5. Configure the project: `cmake -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo -DEBPF_COMMON_TOOLCHAIN_PATH:PATH=/path/to/osquery-toolchain -DEBPFPUB_ENABLE_INSTALL:BOOL=true -DEBPFPUB_ENABLE_TOOLS:BOOL=true -DEBPF_COMMON_ENABLE_TESTS:BOOL=true ..`
6. Build the project: `cmake --build . -j $(($(nproc) + 1))`
7. Run the tests: `cmake --build . --target run-ebpf-common-tests`

### Building with the system toolchain

**Note that this will fail unless clang and the C++ library both support C++17**. Recent distributions should be compatible (tested on Arch Linux, Ubuntu 19.10).

1. Obtain the source code: `git clone --recursive https://github.com/trailofbits/ebpfpub`
2. In case the `--recursive` flag was not provided, run `git submodule update --init --recursive`
3. Enter the source folder: `cd ebpfpub`
4. Create the build folder: `mkdir build && cd build`
5. Configure the project: `cmake -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo -DCMAKE_C_COMPILER:STRING=clang -DCMAKE_CXX_COMPILER=clang++ -DEBPFPUB_ENABLE_INSTALL:BOOL=true -DEBPFPUB_ENABLE_TOOLS:BOOL=true -DEBPF_COMMON_ENABLE_TESTS:BOOL=true ..`
6. Build the project: `cmake --build . -j $(($(nproc) + 1))`
7. Run the tests: `cmake --build . --target run-ebpf-common-tests`

### Building the packages

## Prerequisites
* DEB: **dpkg** command
* RPM: **rpm** command
* TGZ: **tar** command

## Steps
Run the following commands inside the build folder:

```
mkdir install
export DESTDIR=`realpath install`

cd build
cmake --build . --target install
```

Configure the packaging project:

```
mkdir package
cd package

cmake -DEBPFPUB_INSTALL_PATH:PATH="${DESTDIR}" /path/to/source_folder/package_generator
cmake --build . --target package
```

## eBPFTracer

eBPFTracer is both a sample and a small utility. It can be used to perform system-wide syscall tracing from the command line.

### Screenshots

```
eBPFTracer
Usage: ./ebpftracer [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -v,--verbose                Print the generated eBPF program before execution
  -t,--tracepoint_list TEXT REQUIRED
                              A comma separated list of syscall tracepoints
  -b,--buffer_size UINT       Buffer size (global)
  -c,--buffer_count UINT      Buffer count (global)
  -p,--perf_size UINT         Size of the perf event array. Expressed as 2^N (global, one per CPU core)
  -e,--event_map_size UINT    Amount of entries in the event map (per tracepoint)
```

```
alessandro at octarine in ~
$ sudo ebpftracer --tracepoint_list=mkdir,rmdir,renameat2
Memory usage

 > Buffer storage: 268435456 bytes
 > Perf output: 2162688 bytes

Generating the BPF programs...

 > mkdir (serializer: generic)
 > rmdir (serializer: generic)
 > renameat2 (serializer: generic)

Entering main loop...

timestamp: 37212737042046 process_id: 82848 thread_id: 82848 user_id: 1000 group_id: 1000 exit_code: 0 probe_error: 0
syscall: mkdir mode: 511 pathname: 'tob_<3'

timestamp: 37239095495130 process_id: 82893 thread_id: 82893 user_id: 1000 group_id: 1000 exit_code: 0 probe_error: 0
syscall: renameat2 flags: 1 newname: 'tob_<3_open_source!' newdfd: 4294967196 olddfd: 4294967196 oldname: 'tob_<3'

timestamp: 37245605152148 process_id: 82903 thread_id: 82903 user_id: 1000 group_id: 1000 exit_code: 0 probe_error: 0
syscall: rmdir pathname: 'tob_<3_open_source!'

^C
Terminating...
```
