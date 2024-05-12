# **Elevator Kernel Module**

## Description

- This project aims to provide us with a comprehensive understanding of system calls, kernel programming, concurrency, synchronization, and elevator scheduling algorithms. It consists of multiple parts that build upon each other to deepen our knowledge and skills in these areas.

- In Part 1, we start by working with system calls. By adding system calls to a C program and verifying their correctness using the “strace” tool, we gain hands-on experience with system call integration and learn about the available system calls for our machine. This part lays the foundation for understanding how system calls interact with the kernel.

- Part 2 takes us further into kernel programming. We will develop a kernel module called `my_timer` that retrieves and stores the current time using the `ktime_get_real_ts64()` function. This module creates a proc entry and allows us to read the current time and elapsed time since the last call. This part helps us understand how kernel modules work, how to interact with kernel functions, and how to use proc interfaces for communication.

- Part 3 focuses on a more complex task: implementing a scheduling algorithm for a dorm elevator. We create a kernel module representing the elevator, supporting operations like starting, stopping, and issuing requests. The module also provides a `/proc/elevator` entry to display important elevator information. This part challenges us to manage concurrency, synchronization, and efficient scheduling within the kernel environment.

- Each part of the project builds upon the knowledge and skills gained in the previous parts. Part 1 introduces us to system calls and their integration, which forms the basis for kernel programming in Part 2. Part 2 expands our understanding of kernel modules and communication through proc interfaces, setting the stage for the more advanced concepts explored in Part 3.

- By completing this project, we acquire practical experience in system calls, kernel programming, concurrency, synchronization, and scheduling algorithms. These are essential skills for developing efficient and robust software systems, particularly in operating systems and low-level programming domains. Understanding system calls and kernel programming enables us to interact with and extend the functionality of the operating system, while concurrency, synchronization, and scheduling concepts are crucial for efficient resource management and multitasking in complex systems.


## Division of Labor
### Part 1: System-Call Tracing
**Details**:
- Follow the instructions below to complete the task:
 1. Create an empty C program named `empty`.
 2. Make a copy of the "empty" program and name it `part1`.
 3. Add exactly five system calls to the "part1" program. You can find the available system calls for your machine in `/usr/include/unistd.h`.
 4. To verify that you have added the correct number of system calls, execute the following commands in the terminal:
  ```
  $ gcc -o empty empty.c
  $ strace -o empty.trace ./empty
  $ gcc -o part1 part1.c
  $ strace -o part1.trace ./part1
  ```

To minimize the length of the output from strace, try to minimize the use of other function calls (e.g., stdlib.h) in your program.

> [!IMPORTANT]
> Running `strace` on an empty C program will generate several system calls. Therefore, when using `strace` on your Part 1 code, it should produce five more system calls than the empty program.


### Part 2:  Timer Kernel Module
**Details**:
- In Unix-like operating systems, the time is often represented as the number of seconds since the Unix Epoch (January 1st, 1970). The task requires creating a kernel module named `my_timer` that utilizes the function `ktime_get_real_ts64()` to retrieve the time value, which includes seconds and nanoseconds since the Epoch.
 1. Develop a kernel module called my_timer that calls the `ktime_get_real_ts64()` function to obtain the current time. This module should store the time value.
 2. When the `my_timer` module is loaded using `insmod`, it should create a proc entry named `/proc/timer`.
 3. When the `my_timer` module is unloaded using `rmmod`, the `/proc/timer` entry should be removed.
 4. On each read operation of `/proc/timer`, utilize the proc interface to print both the current time and the elapsed time since the last call (if valid).

- To insert a kernel module:
  ```$ sudo insmod my_timer.ko```
- To remove a kernel module:
  ```$ sudo rmmod my_timer.ko```
- To check for your kernel module:
  ```$ lsmod | grep my_timer```
- Example Usage:
  ```
  $ cat /proc/timer
  current time: 1518647111.760933999
  
  $ sleep 1
  
  $ cat /proc/timer
  current time: 1518647112.768429998
  elapsed time: 1.007495999
  
  $ sleep 3
  
  $ cat /proc/timer
  current time: 1518647115.774925999
  elapsed time: 3.006496001
  
  $ sleep 5
  
  $ cat /proc/timer
  current time: 1518647120.780421999
  elapsed time: 5.005496000
  ```


### Part 3: Elevator Module
**Details**:
- This part requires implementing a scheduling algorithm for an office elevator. You will create a kernel module `elevator` to implement this.
  - The office elevator can hold a maximum of 5 passengers and cannot exceed a maximum weight of 7 lbs
  - Each worker is randomly chosen as a part-time `lawyer, boss, and visitor` with equal likelihood.
  - Workers select their starting floor and destination floor.
  - Workers board the elevator in first-in, first-out (`FIFO`) order.
  - Each type of passenger (`part-time, lawyer, boss, and visitor`) has an assigned weight:
    - `1 lb` for part-time workers
    - `1.5 lbs` for lawyers
    - `2 lbs` for bosses
    - `0.5 lbs` for visitors
  - Workers on floors wait indefinitely to be serviced.
  - Once a worker boards the elevator, they can only disembark.
  - The elevator must wait for `2.0 seconds` when moving between floors and `1.0 seconds` when loading/unloading passengers.
  - The building has `floor 1` as the minimum (lobby) and `floor 5` as the maximum.
  - Workers can arrive at any time, and each floor must handle an arbitrary number.

### Part 3a: Adding System Calls
**Details**:
- Download the latest stable [linux-kernel-6.7.x](https://www.kernel.org/) and follow the provided slides in compiling your kernel.
- You should move the kernel to your `/usr/src/` directory and create a soft link to it as so:
  ```
  $ sudo ln -s /usr/src/[kernel_version] ~/[kernel_version]
  $ cd ~/[kernel_version]
  ```
- This will make it easier to modify from elsewhere instead of having to edit it in a restricted area.
- Modify the kernel by adding three system calls to control the elevator and create passengers. Assign the following numbers to the system calls:
  - 548 for `start_elevator()`
  - 549 for `issue_request()`
  - 550 for `stop_elevator()`
- The respective function prototypes are as followed:
  
```int start_elevator(void)```
  - The `start_elevator()` system call activates the elevator for service. From this point forward, the elevator exists and will begin to service requests. It returns 1 if the elevator is already active, 0 for a successful start, and `-ERRORNUM` if initialization fails or `-ENOMEM` if it couldn't allocate memory. The elevator is initialized with the following values:
    ```
      State: IDLE
      Current floor: 1
      Current load: 0 passengers
    ```

```int issue_request(int start_floor, int destination_floor, int type)```
  - The `issue_request()` system call creates a request for a passenger, specifying the start floor, destination floor, and type of passenger (0 for part-timers, 1 for lawyers, 2 for bosses, 3 for visitors). It returns 1 if the request is invalid (e.g., out of range or invalid type) and 0 otherwise.

```int stop_elevator(void)```
  - The `stop_elevator()` system call deactivates the elevator. It stops processing new requests (passengers waiting on floors), but it must offload all current passengers before complete deactivation. Only when the elevator is empty can it be deactivated (`state = OFFLINE`). The system call returns 1 if the elevator is already in the process of deactivating and 0 otherwise.

- You will need to make these files to add the system calls:
  - `[kernel_version]/syscalls/syscalls.c`
  - `[kernel_version]/syscalls/Makefile`
- You will need to modify the following files to add the system calls:
  - `[kernel_version]/arch/x86/syscalls/syscall_64.tbl`
  - `[kernel_version]/include/linux/syscalls.h`
  - `[kernel_version]/Makefile`


### Part 3b:  Kernel Compilation
**Details**:
- You will need to disable certain certificates when adding system calls, follow the slides.
- Compile the kernel with the new system calls. The kernel should be compiled with the following options:
  ```
  $ make menuconfig
  $ make -j$(nproc)
  $ sudo make modules_install
  $ sudo make install
  $ sudo reboot
  ```
- Check that you installed your kernel by typing this into the terminal:
  ```$ uname -r```
  [!WARNING]
  > Note: This is a long process! Make sure to do this part early!


### Part 3c: Test System Calls
**Details**:
- You should test if you successfully added the system called to your installed kernel with the provided tests in your starter file in the directory `part3/tests/`
- Run the following commands:
  ```
  $ make
  $ sudo insmod syscalls.ko
  $ ./test_syscalls
  ```
- You should get a message that tells you if you have the system calls installed or not.


### Part 3d: Implement Elevator
**Details**:
- Implement the elevator kernel module. The module should be named "elevator" and should be loaded using insmod. The module should be unloaded using rmmod.
- Recall that these are the details:
  - Use linked lists to handle the number of passengers per floor/elevator.
  - Use a kthread to control the elevator movement.
  - Use a mutex to control shared data access between floor and elevators.
  - Use kmalloc to allocate dynamic memory for passengers.


### Part 3e: Write to Proc File
**Details**:
- The module must provide a proc entry named `/proc/elevator`. The following information should be printed (labeled appropriately):
  - The elevator's movement state:
    `OFFLINE`: when the module is installed but the elevator isn't running (initial state)
    `IDLE`: elevator is stopped on a floor because there are no more passengers to service
    `LOADING`: elevator is stopped on a floor to load and unload passengers
    `UP`: elevator is moving from a lower floor to a higher floor
    `DOWN`: elevator is moving from a higher floor to a lower floor
  - The current floor the elevator is on
  - The elevator's current load (weight)
  - A list of passengers in the elevator
  - The total number of passengers waiting
  - The total number of passengers serviced

- For each floor of the building, the following should be printed:
    - An indicator of whether or not the elevator is on the floor.
    - The count of waiting passengers.
    - For each waiting passenger, 2 characters indicating the passenger type and destination floor.
- Example Proc File:
  ```
  Elevator state: LOADING
  Current floor: 4
  Current load: 4.5 lbs
  Elevator status: P5 B2 P4 V1
  
  [ ] Floor 6: 1 V3
  [ ] Floor 5: 0
  [*] Floor 4: 2 L1 B2
  [ ] Floor 3: 2 L4 P5
  [ ] Floor 2: 0
  [ ] Floor 1: 0
  
  Number of passengers: 4
  Number of passengers waiting: 5
  Number of passengers serviced: 61
  ```
  > `P` is for part-timers, `L` is for lawyers, `B` is for bosses, `V` is for visitors.


### Part 3f: Test Elevator
**Details**:
- Interact with two provided user-space applications that enable communication with the kernel module:
  - `producer.c`: creates passengers and issues requests to the elevator
      ```$ ./producer [number_of_passengers]```
  - `consumer.c`: calls the `start_elevator()` or the `stop_elevator()` system call.
    - If the flag is `--start`, the program starts the elevator.
    - If the flag is `--stop`, the program stops the elevator.
- You can use the following command to see your elevator in action:
  ```$ watch -n [snds] cat [proc_file]```
- The `producer.c` and `consumer.c` programs will be provided to you.


> [!NOTE]
> Please note that these assignments are subject to discussion and adjustment based on the team's
agreement and individual expertise. Regular communication and collaboration among team
members are encouraged to ensure the successful completion of the project**

## File Listing
```
root/
├── part1/
|    ├── empty.c
|    ├── empty.trace
|    ├── part1.c
|    ├── part1.trace
|    └── Makefile
├── part2/
|    ├── src/
|    |    └── my_timer.c
|    └── Makefile
├── part3/
|    ├── src/
|    |    └── producer-consumer
|    |    |    |    ├── consumer.c
|    |    |    |    ├── producer.c
|    |    |    |    ├── wrappers.h
|    |    |    |    ├── README.md
|    |    |    |    └── Makefile
|    |    ├── elevator.c
|    |    └── Makefile
|    ├── Makefile
|    └── syscalls.c
└── README.md
```
- For part 2 and part 3, the Makefile should produce a kernel module, `my_timer.ko` and `elevator.ko` respectively.


## How to Compile & Execute

### Requirements
- **Compiler**: GCC 12.x
- **Linux**: 6.7.x

### Compilation
To compile the parts utilizing modules:
```bash
make
```

### Execution
```bash
sudo insmod [MODULE_NAME].ko
```
This will allow the use of the modules

**For Part 2**
```bash
cat /proc/timer
```

**For Part 3**
```bash
./producer [NUM_PASSENGERS]
```
```bash
./consumer --start
```
```bash
watch -n1 cat /proc/elevator
```
```bash
./consumer --stop
```

> [!CAUTION]
> If you decide to recreate this on your PC, it's best done on a virtual machine, to avoid bricking your device. This requires you to set up Ubuntu 23.04.
