# AI-Based Adaptive CPU Scheduler

> An Interactive, Educational OS Workload Simulator

![C++17](https://img.shields.io/badge/C++-17-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)
![Zero Dependencies](https://img.shields.io/badge/Dependencies-None-brightgreen.svg)

## Overview
Traditional methods of learning CPU scheduling rely on manual mathematical tracing, which becomes unmanageable as process volume increases or when preemptive logic introduces frequent context switches. 

This project is a robust, single-file C++ simulator designed to bridge basic operating-system concepts with a practical visual application. It visualizes process execution, calculates theoretical metrics, and introduces an **Adaptive AI Heuristic** to bridge theoretical algorithms with dynamic real-world workloads.

[📄 **Read the Full Technical Report (PDF)**](<./AI-Based Adaptive CPU Scheduler_Report.pdf>)

## Features
* **Interactive Educational Mode:** Load processes manually, from a file, or use built-in sample scenarios.
* **Visual Gantt Charts:** Terminal-based visualization of CPU state across time, including idle cycles.
* **Comparative Metrics:** Automatically calculates Average Wait Time, Turnaround Time, Response Time, CPU Utilization, Throughput, and Context Switches.
* **Starvation Protection:** Implements Priority Aging to ensure low-priority processes are eventually executed.
* **Zero Dependencies:** Pure C++17 implementation using standard library data structures and Windows ANSI escape codes.

## Supported Algorithms
1. **First Come First Served (FCFS)** - Non-preemptive
2. **Shortest Job First (SJF)** - Non-preemptive
3. **Shortest Remaining Time First (SRTF)** - Preemptive
4. **Round Robin (RR)** - Preemptive (configurable time quantum)
5. **Priority Scheduling (with Aging)** - Preemptive 
6. **Adaptive AI Scheduler** - Meta-scheduler

### How the Adaptive AI Works
Instead of forcing a user to guess the best approach, the AI meta-scheduler statistically evaluates the workload at runtime. It calculates:
* Coefficient of variation of burst times
* Standard deviation of priorities
* Density of arrivals
* Total process count

Based on this statistical profile, it deterministically selects the optimal algorithm, preventing convoy effects on high-variance workloads and avoiding starvation on dense workloads.

## Getting Started

### Prerequisites
* A C++17 compatible compiler (e.g., GCC, Clang, or MSVC)
* Windows OS (for standard ANSI console rendering)

### Build and Run
Clone the repository and compile the single source file:

```bash
git clone https://github.com/M-S-H-Git/ai-cpu-scheduler.git
cd ai-cpu-scheduler

# Compile using g++
g++ -std=c++17 main.cpp -o scheduler.exe

# Run the simulator
./scheduler.exe
