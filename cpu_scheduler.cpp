// ============================================================================
//  AI-BASED ADAPTIVE CPU SCHEDULER
//  Operating Systems Scheduling Simulator
//
//  This program simulates how an operating system decides which program
//  (process) to run on the CPU and in what order. It implements 6 scheduling
//  algorithms and compares their performance side by side.
//
//  Algorithms: FCFS, SJF, SRTF, Round Robin, Priority (with Aging), Adaptive AI
//
//  Designed for users with zero technical experience — every step is guided.
// ============================================================================

#include <iostream>
#include <vector>
#include <algorithm>
#include <queue>
#include <deque>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cmath>
#include <string>
#include <numeric>
#include <limits>
#include <functional>
#include <climits>
#include <cstdlib>
#include <map>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

using namespace std;

// ============================================================================
// SECTION 1: ANSI COLOR CODES & CONSTANTS
// ============================================================================

namespace Color {
    const string RESET   = "\033[0m";
    const string BOLD    = "\033[1m";
    const string DIM     = "\033[2m";
    const string ITALIC  = "\033[3m";
    const string ULINE   = "\033[4m";

    const string RED     = "\033[91m";
    const string GREEN   = "\033[92m";
    const string YELLOW  = "\033[93m";
    const string BLUE    = "\033[94m";
    const string MAGENTA = "\033[95m";
    const string CYAN    = "\033[96m";
    const string WHITE   = "\033[97m";

    const string BG_RED     = "\033[41m";
    const string BG_GREEN   = "\033[42m";
    const string BG_YELLOW  = "\033[43m";
    const string BG_BLUE    = "\033[44m";
    const string BG_MAGENTA = "\033[45m";
    const string BG_CYAN    = "\033[46m";

    // Process colors for Gantt charts (cycle through these)
    const string PROC_COLORS[] = {
        "\033[48;5;39m",   // blue
        "\033[48;5;208m",  // orange
        "\033[48;5;46m",   // green
        "\033[48;5;201m",  // pink
        "\033[48;5;226m",  // yellow
        "\033[48;5;51m",   // cyan
        "\033[48;5;196m",  // red
        "\033[48;5;141m",  // purple
        "\033[48;5;118m",  // lime
        "\033[48;5;214m",  // gold
    };
    const int NUM_PROC_COLORS = 10;

    string procColor(int pid) {
        return PROC_COLORS[pid % NUM_PROC_COLORS];
    }
}

const int MAX_PROCESSES   = 20;
const int AGING_FACTOR    = 3;   // Priority improves by 1 every 3 time units of waiting
const int DEFAULT_QUANTUM = 3;

// ============================================================================
// SECTION 2: DATA STRUCTURES
// ============================================================================

enum class ProcessState { NEW, READY, RUNNING, WAITING, TERMINATED };

string stateToString(ProcessState s) {
    switch (s) {
        case ProcessState::NEW:        return "NEW";
        case ProcessState::READY:      return "READY";
        case ProcessState::RUNNING:    return "RUNNING";
        case ProcessState::WAITING:    return "WAITING";
        case ProcessState::TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

struct Process {
    int pid;
    int arrival_time;
    int burst_time;
    int priority;        // lower number = higher priority

    // -- Computed during simulation (reset per algorithm run) --
    int remaining_time;
    int waiting_time;
    int turnaround_time;
    int completion_time;
    int response_time;
    bool started;
    ProcessState state;

    // For aging in priority scheduling
    int effective_priority;
    int time_waiting_in_ready;

    Process() : pid(0), arrival_time(0), burst_time(0), priority(0),
                remaining_time(0), waiting_time(0), turnaround_time(0),
                completion_time(0), response_time(-1), started(false),
                state(ProcessState::NEW), effective_priority(0),
                time_waiting_in_ready(0) {}

    void reset() {
        remaining_time = burst_time;
        waiting_time = 0;
        turnaround_time = 0;
        completion_time = 0;
        response_time = -1;
        started = false;
        state = ProcessState::NEW;
        effective_priority = priority;
        time_waiting_in_ready = 0;
    }
};

struct GanttEntry {
    int pid;        // -1 for idle
    int start_time;
    int end_time;
};

struct SchedulingResult {
    string algorithm_name;
    vector<GanttEntry> gantt_chart;
    vector<Process> processes;   // processes with computed metrics
    double avg_waiting_time;
    double avg_turnaround_time;
    double avg_response_time;
    double cpu_utilization;
    double throughput;
    int context_switches;
    int total_time;
    int idle_time;
};

struct AlgorithmScore {
    string name;
    double score;
    vector<string> reasons;
};

// ============================================================================
// SECTION 3: UTILITY & UI HELPER FUNCTIONS
// ============================================================================

void enableAnsiColors() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
        SetConsoleMode(hOut, dwMode);
    }
    SetConsoleOutputCP(65001); // UTF-8
#endif
}

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void pauseScreen() {
    cout << "\n" << Color::DIM << "  Press Enter to continue..." << Color::RESET;
    if (cin.eof()) return;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void printSeparator(char c = '=', int width = 78) {
    cout << Color::CYAN;
    for (int i = 0; i < width; i++) cout << c;
    cout << Color::RESET << "\n";
}

void printDoubleSeparator(int width = 78) {
    cout << Color::CYAN << Color::BOLD;
    for (int i = 0; i < width; i++) cout << "=";
    cout << Color::RESET << "\n";
}

void printHeader(const string& title, int width = 78) {
    cout << "\n";
    printDoubleSeparator(width);
    int pad = (width - (int)title.length()) / 2;
    cout << Color::CYAN << Color::BOLD;
    for (int i = 0; i < pad; i++) cout << " ";
    cout << title << Color::RESET << "\n";
    printDoubleSeparator(width);
}

void printSubHeader(const string& title) {
    cout << "\n  " << Color::YELLOW << Color::BOLD << ">> " << title
         << " <<" << Color::RESET << "\n";
    cout << "  ";
    printSeparator('-', 74);
}

void printSuccess(const string& msg) {
    cout << "  " << Color::GREEN << Color::BOLD << "[OK] " << Color::RESET
         << Color::GREEN << msg << Color::RESET << "\n";
}

void printError(const string& msg) {
    cout << "  " << Color::RED << Color::BOLD << "[ERROR] " << Color::RESET
         << Color::RED << msg << Color::RESET << "\n";
}

void printWarning(const string& msg) {
    cout << "  " << Color::YELLOW << Color::BOLD << "[!] " << Color::RESET
         << Color::YELLOW << msg << Color::RESET << "\n";
}

void printInfo(const string& msg) {
    cout << "  " << Color::CYAN << "[i] " << Color::RESET << msg << "\n";
}

void printBullet(const string& msg) {
    cout << "      " << Color::DIM << "* " << Color::RESET << msg << "\n";
}

void printBanner() {
    clearScreen();
    cout << Color::CYAN << Color::BOLD;
    cout << "\n";
    cout << "  ================================================================\n";
    cout << "                                                                                                                                                              \n";
    cout << "            AI-BASED ADAPTIVE CPU SCHEDULER SIMULATOR                                                                       \n";
    cout << "            Operating Systems Scheduling Demonstration                                                                              \n";
    cout << "                                                                                                                                                                 \n";
    cout << "  ================================================================\n";
    cout << Color::RESET;
    cout << "\n";
    cout << Color::WHITE;
    cout << "  Welcome! This program simulates how an operating system decides\n";
    cout << "  which program (process) to run on the CPU and in what order.\n\n";
    cout << Color::GREEN;
    cout << "  Algorithms implemented:\n";
    cout << "    [1] FCFS   - First Come, First Served\n";
    cout << "    [2] SJF    - Shortest Job First\n";
    cout << "    [3] SRTF   - Shortest Remaining Time First\n";
    cout << "    [4] RR     - Round Robin\n";
    cout << "    [5] PRIO   - Priority Scheduling (with Aging)\n";
    cout << "    [6] AI     - Adaptive AI Scheduler (picks the best!)\n";
    cout << Color::RESET;
    cout << "\n  " << Color::DIM << "Don't worry if you're new to this";
    cout << " -- we'll guide you at every step!" << Color::RESET << "\n\n";
}

int getValidInt(const string& prompt, int minVal, int maxVal) {
    int value;
    while (true) {
        cout << prompt;
        if (cin.eof()) {
            cout << "\n";
            printInfo("End of input reached. Exiting.");
            exit(0);
        }
        if (cin >> value) {
            if (value >= minVal && value <= maxVal) {
                return value;
            }
            printError("Please enter a number between " + to_string(minVal)
                       + " and " + to_string(maxVal) + ".");
        } else {
            if (cin.eof()) {
                cout << "\n";
                printInfo("End of input reached. Exiting.");
                exit(0);
            }
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            printError("That's not a valid number. Please try again.");
        }
    }
}

// ============================================================================
// SECTION 4: PROCESS INPUT FUNCTIONS
// ============================================================================

void displayProcessTable(const vector<Process>& processes) {
    cout << "\n";
    cout << "  " << Color::BOLD << Color::WHITE
         << "+-----+---------+-------+----------+" << Color::RESET << "\n";
    cout << "  " << Color::BOLD << Color::WHITE
         << "| PID | Arrival | Burst | Priority |" << Color::RESET << "\n";
    cout << "  " << Color::BOLD << Color::WHITE
         << "+-----+---------+-------+----------+" << Color::RESET << "\n";
    for (const auto& p : processes) {
        cout << "  " << Color::WHITE << "| " << Color::CYAN << setw(3) << p.pid
             << Color::WHITE << " |  " << setw(5) << p.arrival_time
             << "  |  " << setw(3) << p.burst_time
             << "  |   " << setw(4) << p.priority
             << "   |" << Color::RESET << "\n";
    }
    cout << "  " << Color::BOLD << Color::WHITE
         << "+-----+---------+-------+----------+" << Color::RESET << "\n";
}

vector<Process> inputManual() {
    printSubHeader("MANUAL PROCESS ENTRY");
    cout << "\n";
    printInfo("You will now enter the details of each process (program).");
    printInfo("For each process, you need to provide:\n");
    printBullet("PID        : A unique ID number for the process (e.g., 1, 2, 3)");
    printBullet("Arrival    : When the process arrives (time unit, 0 = start)");
    printBullet("Burst      : How long the process needs the CPU (time units)");
    printBullet("Priority   : Importance level (1 = highest, larger = lower)");
    cout << "\n";

    int n = getValidInt("  How many processes? (1-" + to_string(MAX_PROCESSES)
                        + "): ", 1, MAX_PROCESSES);
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    vector<Process> processes;
    for (int i = 0; i < n; i++) {
        Process p;
        cout << "\n  " << Color::MAGENTA << Color::BOLD << "--- Process "
             << (i + 1) << " of " << n << " ---" << Color::RESET << "\n";

        p.pid = getValidInt("    Enter PID (unique number, 1-999): ", 1, 999);

        // Check for duplicate PID
        bool duplicate = false;
        for (const auto& existing : processes) {
            if (existing.pid == p.pid) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            printError("PID " + to_string(p.pid)
                       + " already exists! Using next available.");
            int maxPid = 0;
            for (const auto& existing : processes) {
                maxPid = max(maxPid, existing.pid);
            }
            p.pid = maxPid + 1;
            printInfo("Assigned PID: " + to_string(p.pid));
        }

        p.arrival_time = getValidInt("    Arrival time (0-1000): ", 0, 1000);
        p.burst_time   = getValidInt("    Burst time  (1-500) : ", 1, 500);
        p.priority     = getValidInt("    Priority    (1-100)  : ", 1, 100);

        p.remaining_time = p.burst_time;
        processes.push_back(p);
        printSuccess("Process P" + to_string(p.pid) + " added successfully!");
    }

    cout << "\n";
    printSuccess("All " + to_string(n) + " processes entered!");
    displayProcessTable(processes);
    return processes;
}

vector<Process> inputFromFile(const string& filename) {
    printSubHeader("LOADING FROM FILE");
    vector<Process> processes;

    ifstream file(filename);
    if (!file.is_open()) {
        printError("Cannot open file: " + filename);
        printInfo("Make sure the file exists in the same folder as this program.");
        printInfo("Expected format (one process per line):");
        cout << "         PID  ArrivalTime  BurstTime  Priority\n";
        cout << "         1    0            5          2\n";
        cout << "         2    1            3          1\n";
        return processes;
    }

    string line;
    int lineNum = 0;
    while (getline(file, line)) {
        lineNum++;
        // Skip empty lines and comments (lines starting with #)
        if (line.empty() || line[0] == '#') continue;

        istringstream iss(line);
        Process p;
        if (iss >> p.pid >> p.arrival_time >> p.burst_time >> p.priority) {
            if (p.burst_time <= 0) {
                printWarning("Line " + to_string(lineNum)
                             + ": Burst time must be > 0. Skipping.");
                continue;
            }
            if (p.arrival_time < 0) {
                printWarning("Line " + to_string(lineNum)
                             + ": Arrival time must be >= 0. Skipping.");
                continue;
            }
            p.remaining_time = p.burst_time;
            processes.push_back(p);
        } else {
            printWarning("Line " + to_string(lineNum)
                         + ": Could not read data. Skipping.");
        }

        if ((int)processes.size() >= MAX_PROCESSES) {
            printWarning("Maximum " + to_string(MAX_PROCESSES)
                         + " processes reached. Ignoring remaining lines.");
            break;
        }
    }
    file.close();

    if (processes.empty()) {
        printError("No valid processes found in file.");
    } else {
        printSuccess("Loaded " + to_string(processes.size())
                     + " processes from " + filename);
        displayProcessTable(processes);
    }
    return processes;
}

vector<Process> loadSampleData(int scenario) {
    vector<Process> processes;

    switch (scenario) {
        case 1: { // Mixed workload (good all-around test)
            printInfo("Loading Sample 1: Mixed Workload (8 processes)");
            int data[][4] = {
                {1, 0, 6, 3}, {2, 1, 4, 1}, {3, 2, 8, 4},
                {4, 3, 2, 2}, {5, 4, 5, 5}, {6, 6, 3, 1},
                {7, 7, 7, 3}, {8, 8, 1, 2}
            };
            for (auto& d : data) {
                Process p;
                p.pid = d[0]; p.arrival_time = d[1];
                p.burst_time = d[2]; p.priority = d[3];
                p.remaining_time = p.burst_time;
                processes.push_back(p);
            }
            break;
        }
        case 2: { // All arrive at time 0
            printInfo("Loading Sample 2: Simultaneous Arrival (5 processes)");
            int data[][4] = {
                {1, 0, 10, 3}, {2, 0, 1, 1}, {3, 0, 2, 4},
                {4, 0, 5, 2}, {5, 0, 3, 5}
            };
            for (auto& d : data) {
                Process p;
                p.pid = d[0]; p.arrival_time = d[1];
                p.burst_time = d[2]; p.priority = d[3];
                p.remaining_time = p.burst_time;
                processes.push_back(p);
            }
            break;
        }
        case 3: { // Priority-heavy scenario (starvation risk)
            printInfo("Loading Sample 3: Priority-Heavy / Starvation Risk (6 processes)");
            int data[][4] = {
                {1, 0, 12, 5}, {2, 2, 4, 1}, {3, 3, 6, 1},
                {4, 5, 3, 2}, {5, 7, 8, 1}, {6, 9, 2, 3}
            };
            for (auto& d : data) {
                Process p;
                p.pid = d[0]; p.arrival_time = d[1];
                p.burst_time = d[2]; p.priority = d[3];
                p.remaining_time = p.burst_time;
                processes.push_back(p);
            }
            break;
        }
        case 4: { // Short burst uniform
            printInfo("Loading Sample 4: Short & Similar Bursts (6 processes)");
            int data[][4] = {
                {1, 0, 3, 2}, {2, 1, 2, 3}, {3, 2, 3, 1},
                {4, 3, 2, 4}, {5, 4, 3, 2}, {6, 5, 2, 3}
            };
            for (auto& d : data) {
                Process p;
                p.pid = d[0]; p.arrival_time = d[1];
                p.burst_time = d[2]; p.priority = d[3];
                p.remaining_time = p.burst_time;
                processes.push_back(p);
            }
            break;
        }
        default: // Same as scenario 1
            return loadSampleData(1);
    }

    displayProcessTable(processes);
    return processes;
}

void generateSampleFile(const string& filename) {
    ofstream file(filename);
    if (!file.is_open()) {
        printError("Could not create sample file: " + filename);
        return;
    }
    file << "# Sample Process Data for CPU Scheduler\n";
    file << "# Format: PID  ArrivalTime  BurstTime  Priority\n";
    file << "# (Priority: 1 = highest priority, larger = lower priority)\n";
    file << "#\n";
    file << "1  0  6  3\n";
    file << "2  1  4  1\n";
    file << "3  2  8  4\n";
    file << "4  3  2  2\n";
    file << "5  4  5  5\n";
    file << "6  6  3  1\n";
    file << "7  7  7  3\n";
    file << "8  8  1  2\n";
    file.close();
    printSuccess("Sample file created: " + filename);
}

// ============================================================================
// SECTION 5: METRIC COMPUTATION HELPERS
// ============================================================================

// Compress per-time-unit trace into Gantt blocks
vector<GanttEntry> compressGantt(const vector<int>& trace) {
    vector<GanttEntry> gantt;
    if (trace.empty()) return gantt;

    GanttEntry current;
    current.pid = trace[0];
    current.start_time = 0;
    current.end_time = 1;

    for (int i = 1; i < (int)trace.size(); i++) {
        if (trace[i] == current.pid) {
            current.end_time = i + 1;
        } else {
            gantt.push_back(current);
            current.pid = trace[i];
            current.start_time = i;
            current.end_time = i + 1;
        }
    }
    gantt.push_back(current);
    return gantt;
}

int countContextSwitches(const vector<GanttEntry>& gantt) {
    int switches = 0;
    for (int i = 1; i < (int)gantt.size(); i++) {
        if (gantt[i].pid != gantt[i - 1].pid) {
            switches++;
        }
    }
    return switches;
}

SchedulingResult computeResult(const string& name,
                               const vector<int>& trace,
                               vector<Process>& procs) {
    SchedulingResult result;
    result.algorithm_name = name;
    result.gantt_chart = compressGantt(trace);
    result.processes = procs;
    result.total_time = trace.empty() ? 0 : (int)trace.size();
    result.context_switches = countContextSwitches(result.gantt_chart);

    double totalWT = 0, totalTAT = 0, totalRT = 0;
    int n = (int)procs.size();
    result.idle_time = 0;

    for (int t : trace) {
        if (t == -1) result.idle_time++;
    }

    for (auto& p : procs) {
        totalWT  += p.waiting_time;
        totalTAT += p.turnaround_time;
        totalRT  += p.response_time;
    }

    result.avg_waiting_time    = (n > 0) ? totalWT / n : 0;
    result.avg_turnaround_time = (n > 0) ? totalTAT / n : 0;
    result.avg_response_time   = (n > 0) ? totalRT / n : 0;
    result.cpu_utilization = (result.total_time > 0)
        ? ((result.total_time - result.idle_time) * 100.0 / result.total_time)
        : 0;
    result.throughput = (result.total_time > 0)
        ? (double)n / result.total_time
        : 0;

    // Copy computed process data back
    result.processes = procs;
    return result;
}

// ============================================================================
// SECTION 6: SCHEDULING ALGORITHMS
// ============================================================================

// --- 6A: FCFS (First Come, First Served) ---
SchedulingResult runFCFS(vector<Process> procs) {
    sort(procs.begin(), procs.end(), [](const Process& a, const Process& b) {
        return (a.arrival_time == b.arrival_time) ? (a.pid < b.pid)
                                                  : (a.arrival_time < b.arrival_time);
    });
    for (auto& p : procs) p.reset();

    vector<int> trace;
    int currentTime = 0;

    for (auto& p : procs) {
        // If CPU is idle, advance time
        if (currentTime < p.arrival_time) {
            for (int t = currentTime; t < p.arrival_time; t++)
                trace.push_back(-1); // idle
            currentTime = p.arrival_time;
        }

        p.response_time = currentTime - p.arrival_time;
        p.waiting_time = currentTime - p.arrival_time;

        for (int t = 0; t < p.burst_time; t++) {
            trace.push_back(p.pid);
        }
        currentTime += p.burst_time;
        p.completion_time = currentTime;
        p.turnaround_time = p.completion_time - p.arrival_time;
        p.state = ProcessState::TERMINATED;
    }

    return computeResult("FCFS", trace, procs);
}

// --- 6B: SJF (Shortest Job First, Non-Preemptive) ---
SchedulingResult runSJF(vector<Process> procs) {
    for (auto& p : procs) p.reset();
    int n = (int)procs.size();
    vector<bool> completed(n, false);
    vector<int> trace;
    int currentTime = 0;
    int done = 0;

    while (done < n) {
        // Find process with shortest burst among arrived, non-completed
        int best = -1;
        int bestBurst = INT_MAX;
        for (int i = 0; i < n; i++) {
            if (!completed[i] && procs[i].arrival_time <= currentTime) {
                if (procs[i].burst_time < bestBurst ||
                    (procs[i].burst_time == bestBurst && best >= 0 &&
                     procs[i].arrival_time < procs[best].arrival_time)) {
                    bestBurst = procs[i].burst_time;
                    best = i;
                }
            }
        }

        if (best == -1) {
            trace.push_back(-1); // idle
            currentTime++;
            continue;
        }

        Process& p = procs[best];
        p.response_time = currentTime - p.arrival_time;
        p.waiting_time = currentTime - p.arrival_time;

        for (int t = 0; t < p.burst_time; t++) {
            trace.push_back(p.pid);
        }
        currentTime += p.burst_time;
        p.completion_time = currentTime;
        p.turnaround_time = p.completion_time - p.arrival_time;
        p.state = ProcessState::TERMINATED;
        completed[best] = true;
        done++;
    }

    return computeResult("SJF", trace, procs);
}

// --- 6C: SRTF (Shortest Remaining Time First, Preemptive SJF) ---
SchedulingResult runSRTF(vector<Process> procs) {
    for (auto& p : procs) p.reset();
    int n = (int)procs.size();
    vector<int> trace;
    int currentTime = 0;
    int done = 0;

    while (done < n) {
        // Find process with shortest remaining time among arrived
        int best = -1;
        int bestRemain = INT_MAX;
        for (int i = 0; i < n; i++) {
            if (procs[i].remaining_time > 0 &&
                procs[i].arrival_time <= currentTime) {
                if (procs[i].remaining_time < bestRemain ||
                    (procs[i].remaining_time == bestRemain && best >= 0 &&
                     procs[i].arrival_time < procs[best].arrival_time)) {
                    bestRemain = procs[i].remaining_time;
                    best = i;
                }
            }
        }

        if (best == -1) {
            trace.push_back(-1);
            currentTime++;
            continue;
        }

        Process& p = procs[best];
        if (!p.started) {
            p.response_time = currentTime - p.arrival_time;
            p.started = true;
        }

        trace.push_back(p.pid);
        p.remaining_time--;
        currentTime++;

        if (p.remaining_time == 0) {
            p.completion_time = currentTime;
            p.turnaround_time = p.completion_time - p.arrival_time;
            p.waiting_time = p.turnaround_time - p.burst_time;
            p.state = ProcessState::TERMINATED;
            done++;
        }
    }

    return computeResult("SRTF", trace, procs);
}

// --- 6D: Round Robin ---
SchedulingResult runRoundRobin(vector<Process> procs, int quantum) {
    // Sort by arrival time for initial ordering
    sort(procs.begin(), procs.end(), [](const Process& a, const Process& b) {
        return (a.arrival_time == b.arrival_time) ? (a.pid < b.pid)
                                                  : (a.arrival_time < b.arrival_time);
    });
    for (auto& p : procs) p.reset();

    // Map pid to index for quick lookup
    map<int, int> pidToIdx;
    for (int i = 0; i < (int)procs.size(); i++) {
        pidToIdx[procs[i].pid] = i;
    }

    int n = (int)procs.size();
    vector<int> trace;
    deque<int> readyQueue; // stores indices into procs
    vector<bool> inQueue(n, false);
    int currentTime = 0;
    int done = 0;
    int nextArrival = 0;

    // Add processes arriving at time 0
    for (int i = 0; i < n; i++) {
        if (procs[i].arrival_time <= 0) {
            readyQueue.push_back(i);
            inQueue[i] = true;
        }
    }
    // Track next arrival index
    nextArrival = 0;
    while (nextArrival < n && procs[nextArrival].arrival_time <= 0)
        nextArrival++;

    while (done < n) {
        if (readyQueue.empty()) {
            // Advance to next arrival
            if (nextArrival < n) {
                int nextTime = procs[nextArrival].arrival_time;
                while (currentTime < nextTime) {
                    trace.push_back(-1);
                    currentTime++;
                }
                while (nextArrival < n &&
                       procs[nextArrival].arrival_time <= currentTime) {
                    if (!inQueue[nextArrival] &&
                        procs[nextArrival].remaining_time > 0) {
                        readyQueue.push_back(nextArrival);
                        inQueue[nextArrival] = true;
                    }
                    nextArrival++;
                }
            }
            continue;
        }

        int idx = readyQueue.front();
        readyQueue.pop_front();
        inQueue[idx] = false;

        Process& p = procs[idx];
        if (!p.started) {
            p.response_time = currentTime - p.arrival_time;
            p.started = true;
        }

        int execTime = min(quantum, p.remaining_time);
        for (int t = 0; t < execTime; t++) {
            trace.push_back(p.pid);
        }
        currentTime += execTime;
        p.remaining_time -= execTime;

        // Add newly arrived processes (arrived during this quantum)
        while (nextArrival < n &&
               procs[nextArrival].arrival_time <= currentTime) {
            if (!inQueue[nextArrival] &&
                procs[nextArrival].remaining_time > 0) {
                readyQueue.push_back(nextArrival);
                inQueue[nextArrival] = true;
            }
            nextArrival++;
        }

        if (p.remaining_time > 0) {
            readyQueue.push_back(idx);
            inQueue[idx] = true;
        } else {
            p.completion_time = currentTime;
            p.turnaround_time = p.completion_time - p.arrival_time;
            p.waiting_time = p.turnaround_time - p.burst_time;
            p.state = ProcessState::TERMINATED;
            done++;
        }
    }

    return computeResult("RR (q=" + to_string(quantum) + ")", trace, procs);
}

// --- 6E: Priority Scheduling with Aging (Preemptive) ---
SchedulingResult runPriority(vector<Process> procs) {
    for (auto& p : procs) p.reset();
    int n = (int)procs.size();
    vector<int> trace;
    int currentTime = 0;
    int done = 0;

    while (done < n) {
        // Apply aging to all waiting (arrived, not completed) processes
        for (int i = 0; i < n; i++) {
            if (procs[i].remaining_time > 0 &&
                procs[i].arrival_time <= currentTime &&
                procs[i].time_waiting_in_ready > 0) {
                int ageBonus = procs[i].time_waiting_in_ready / AGING_FACTOR;
                procs[i].effective_priority = max(1,
                    procs[i].priority - ageBonus);
            }
        }

        // Find highest priority (lowest number) among arrived processes
        int best = -1;
        int bestPrio = INT_MAX;
        for (int i = 0; i < n; i++) {
            if (procs[i].remaining_time > 0 &&
                procs[i].arrival_time <= currentTime) {
                if (procs[i].effective_priority < bestPrio ||
                    (procs[i].effective_priority == bestPrio && best >= 0 &&
                     procs[i].arrival_time < procs[best].arrival_time)) {
                    bestPrio = procs[i].effective_priority;
                    best = i;
                }
            }
        }

        if (best == -1) {
            trace.push_back(-1);
            currentTime++;
            continue;
        }

        Process& p = procs[best];
        if (!p.started) {
            p.response_time = currentTime - p.arrival_time;
            p.started = true;
        }

        trace.push_back(p.pid);
        p.remaining_time--;
        currentTime++;

        // Update waiting time for all OTHER arrived, non-completed processes
        for (int i = 0; i < n; i++) {
            if (i != best && procs[i].remaining_time > 0 &&
                procs[i].arrival_time < currentTime) {
                procs[i].time_waiting_in_ready++;
            }
        }
        // Reset waiting counter for the running process
        p.time_waiting_in_ready = 0;

        if (p.remaining_time == 0) {
            p.completion_time = currentTime;
            p.turnaround_time = p.completion_time - p.arrival_time;
            p.waiting_time = p.turnaround_time - p.burst_time;
            p.state = ProcessState::TERMINATED;
            done++;
        }
    }

    return computeResult("Priority", trace, procs);
}

// ============================================================================
// SECTION 7: ADAPTIVE AI SCHEDULER
// ============================================================================

double calcMean(const vector<double>& v) {
    if (v.empty()) return 0;
    double sum = 0;
    for (double x : v) sum += x;
    return sum / v.size();
}

double calcStdDev(const vector<double>& v) {
    if (v.size() <= 1) return 0;
    double m = calcMean(v);
    double sumSq = 0;
    for (double x : v) sumSq += (x - m) * (x - m);
    return sqrt(sumSq / v.size());
}

SchedulingResult runAdaptive(vector<Process> procs, int quantum) {
    int n = (int)procs.size();

    // ---- Step 1: Analyze Workload Characteristics ----
    vector<double> bursts, priorities, arrivals;
    for (const auto& p : procs) {
        bursts.push_back((double)p.burst_time);
        priorities.push_back((double)p.priority);
        arrivals.push_back((double)p.arrival_time);
    }

    double meanBurst   = calcMean(bursts);
    double stdBurst    = calcStdDev(bursts);
    double cvBurst     = (meanBurst > 0) ? stdBurst / meanBurst : 0; // coefficient of variation
    double stdPriority = calcStdDev(priorities);
    double minArrival  = *min_element(arrivals.begin(), arrivals.end());
    double maxArrival  = *max_element(arrivals.begin(), arrivals.end());
    double arrivalSpread = maxArrival - minArrival;

    // ---- Step 2: Score Each Algorithm ----
    AlgorithmScore scores[5];
    scores[0] = {"FCFS",     0, {}};
    scores[1] = {"SJF",      0, {}};
    scores[2] = {"SRTF",     0, {}};
    scores[3] = {"Round Robin", 0, {}};
    scores[4] = {"Priority", 0, {}};

    // Factor 1: Burst time variation
    if (cvBurst < 0.25) {
        scores[0].score += 30; scores[0].reasons.push_back("Burst times are very similar (CV=" + to_string(cvBurst).substr(0,4) + ") -> FCFS is fair");
        scores[3].score += 25; scores[3].reasons.push_back("Similar burst times work well with time-slicing");
    } else if (cvBurst < 0.6) {
        scores[1].score += 25; scores[1].reasons.push_back("Moderate burst variation (CV=" + to_string(cvBurst).substr(0,4) + ") -> SJF reduces wait");
        scores[2].score += 30; scores[2].reasons.push_back("Moderate burst variation benefits preemptive shortest-job");
        scores[3].score += 15; scores[3].reasons.push_back("RR provides fairness with moderate variation");
    } else {
        scores[1].score += 30; scores[1].reasons.push_back("High burst variation (CV=" + to_string(cvBurst).substr(0,4) + ") -> SJF strongly preferred");
        scores[2].score += 40; scores[2].reasons.push_back("High burst variation (CV=" + to_string(cvBurst).substr(0,4) + ") -> SRTF minimizes wait time");
    }

    // Factor 2: Priority diversity
    if (stdPriority > 1.5) {
        scores[4].score += 35;
        scores[4].reasons.push_back("Diverse priorities (StdDev=" + to_string(stdPriority).substr(0,4) + ") -> Priority scheduling is meaningful");
    } else if (stdPriority > 0.5) {
        scores[4].score += 15;
        scores[4].reasons.push_back("Moderate priority diversity (StdDev=" + to_string(stdPriority).substr(0,4) + ")");
    } else {
        scores[0].score += 5;
        scores[0].reasons.push_back("Priorities are nearly identical -> priority scheduling not needed");
    }

    // Factor 3: Process count
    if (n <= 3) {
        scores[0].score += 15; scores[0].reasons.push_back("Few processes (" + to_string(n) + ") -> FCFS is simple and effective");
        scores[1].score += 15; scores[1].reasons.push_back("Few processes (" + to_string(n) + ") -> SJF overhead is minimal");
    } else if (n <= 7) {
        scores[2].score += 10; scores[2].reasons.push_back("Medium process count (" + to_string(n) + ")");
        scores[3].score += 15; scores[3].reasons.push_back("Medium process count (" + to_string(n) + ") -> RR balances fairness");
    } else {
        scores[3].score += 25; scores[3].reasons.push_back("Many processes (" + to_string(n) + ") -> RR ensures no starvation");
        scores[4].score += 10; scores[4].reasons.push_back("Many processes benefit from priority ordering");
    }

    // Factor 4: Arrival time spread
    if (arrivalSpread == 0) {
        scores[1].score += 15; scores[1].reasons.push_back("All processes arrive together -> SJF optimal ordering");
        scores[0].score += 10; scores[0].reasons.push_back("Simultaneous arrival -> FCFS is straightforward");
    } else if (arrivalSpread > meanBurst) {
        scores[2].score += 15; scores[2].reasons.push_back("Wide arrival spread -> SRTF adapts to new arrivals");
        scores[3].score += 10; scores[3].reasons.push_back("Spread arrivals -> RR shares CPU fairly");
    }

    // Factor 5: Average burst time
    if (meanBurst <= 3) {
        scores[3].score += 10; scores[3].reasons.push_back("Short average burst (" + to_string(meanBurst).substr(0,4) + ") -> RR with small quantum is efficient");
    } else if (meanBurst > 8) {
        scores[0].score += 5; scores[0].reasons.push_back("Long average burst -> minimizing context switches helps");
    }

    // ---- Step 3: Select the Winner ----
    int bestIdx = 0;
    for (int i = 1; i < 5; i++) {
        if (scores[i].score > scores[bestIdx].score) {
            bestIdx = i;
        }
    }

    // ---- Step 4: Display AI Analysis ----
    printSubHeader("AI WORKLOAD ANALYSIS");
    cout << "\n";
    printInfo("Analyzing workload characteristics...\n");
    cout << "    " << Color::WHITE << "Number of processes  : " << Color::CYAN << n << Color::RESET << "\n";
    cout << "    " << Color::WHITE << "Mean burst time      : " << Color::CYAN << fixed << setprecision(2) << meanBurst << Color::RESET << "\n";
    cout << "    " << Color::WHITE << "Burst time Std Dev   : " << Color::CYAN << stdBurst << Color::RESET << "\n";
    cout << "    " << Color::WHITE << "Burst time CV        : " << Color::CYAN << cvBurst << Color::RESET << "\n";
    cout << "    " << Color::WHITE << "Priority Std Dev     : " << Color::CYAN << stdPriority << Color::RESET << "\n";
    cout << "    " << Color::WHITE << "Arrival time spread  : " << Color::CYAN << arrivalSpread << Color::RESET << "\n";

    cout << "\n";
    printInfo("Algorithm Scores:");
    cout << "\n";

    for (int i = 0; i < 5; i++) {
        string bar = "";
        int barLen = (int)(scores[i].score / 2);
        for (int b = 0; b < barLen; b++) bar += "#";

        string color = (i == bestIdx) ? Color::GREEN + Color::BOLD : Color::WHITE;
        string marker = (i == bestIdx) ? " <-- WINNER" : "";

        cout << "    " << color << setw(14) << left << scores[i].name
             << " [" << setw(3) << right << (int)scores[i].score << " pts] "
             << setw(30) << left << bar << marker << Color::RESET << "\n";
    }

    cout << "\n";
    printInfo("Reasoning for selection: " + Color::GREEN + Color::BOLD
              + scores[bestIdx].name + Color::RESET);
    for (const auto& reason : scores[bestIdx].reasons) {
        printBullet(reason);
    }

    // ---- Step 5: Run the Selected Algorithm ----
    SchedulingResult result;
    string selectedName = scores[bestIdx].name;

    if (selectedName == "FCFS") {
        result = runFCFS(procs);
    } else if (selectedName == "SJF") {
        result = runSJF(procs);
    } else if (selectedName == "SRTF") {
        result = runSRTF(procs);
    } else if (selectedName == "Round Robin") {
        result = runRoundRobin(procs, quantum);
    } else if (selectedName == "Priority") {
        result = runPriority(procs);
    } else {
        result = runSRTF(procs); // fallback
    }

    result.algorithm_name = "AI (" + selectedName + ")";
    return result;
}

// ============================================================================
// SECTION 8: OUTPUT & VISUALIZATION
// ============================================================================

void printGanttChart(const SchedulingResult& result) {
    cout << "\n  " << Color::BOLD << Color::WHITE << "Gantt Chart: "
         << Color::CYAN << result.algorithm_name << Color::RESET << "\n\n";

    const auto& gantt = result.gantt_chart;
    if (gantt.empty()) {
        printWarning("No Gantt chart data.");
        return;
    }

    // Determine widths for each block
    vector<int> widths;
    for (const auto& g : gantt) {
        int duration = g.end_time - g.start_time;
        string label = (g.pid == -1) ? "IDLE" : ("P" + to_string(g.pid));
        int minWidth = max((int)label.length() + 2, duration);
        minWidth = max(minWidth, 4);
        widths.push_back(minWidth);
    }

    // Limit line width, wrap if needed
    const int MAX_LINE_WIDTH = 72;
    int lineStart = 0;

    while (lineStart < (int)gantt.size()) {
        int lineEnd = lineStart;
        int lineWidth = 1; // for the leading |
        while (lineEnd < (int)gantt.size()) {
            if (lineWidth + widths[lineEnd] + 1 > MAX_LINE_WIDTH && lineEnd > lineStart)
                break;
            lineWidth += widths[lineEnd] + 1;
            lineEnd++;
        }

        // Top border
        cout << "    +";
        for (int i = lineStart; i < lineEnd; i++) {
            for (int w = 0; w < widths[i]; w++) cout << "-";
            cout << "+";
        }
        cout << "\n";

        // Process labels
        cout << "    |";
        for (int i = lineStart; i < lineEnd; i++) {
            string label = (gantt[i].pid == -1) ? "IDLE"
                : ("P" + to_string(gantt[i].pid));
            int pad = widths[i] - (int)label.length();
            int padLeft = pad / 2;
            int padRight = pad - padLeft;

            if (gantt[i].pid == -1) {
                cout << Color::DIM;
            } else {
                cout << Color::procColor(gantt[i].pid) << Color::BOLD
                     << Color::WHITE;
            }
            for (int p = 0; p < padLeft; p++) cout << " ";
            cout << label;
            for (int p = 0; p < padRight; p++) cout << " ";
            cout << Color::RESET << "|";
        }
        cout << "\n";

        // Bottom border
        cout << "    +";
        for (int i = lineStart; i < lineEnd; i++) {
            for (int w = 0; w < widths[i]; w++) cout << "-";
            cout << "+";
        }
        cout << "\n";

        // Time labels
        cout << "    " << gantt[lineStart].start_time;
        for (int i = lineStart; i < lineEnd; i++) {
            string timeStr = to_string(gantt[i].end_time);
            int spaces = widths[i] + 1 - (int)timeStr.length();
            for (int s = 0; s < spaces; s++) cout << " ";
            cout << timeStr;
        }
        cout << "\n";

        lineStart = lineEnd;
    }
}

void printProcessResults(const SchedulingResult& result) {
    cout << "\n  " << Color::BOLD << Color::WHITE << "Per-Process Results: "
         << Color::CYAN << result.algorithm_name << Color::RESET << "\n\n";

    cout << "  " << Color::BOLD
         << "+-----+---------+-------+----------+------------+------------+---------+----------+"
         << Color::RESET << "\n";
    cout << "  " << Color::BOLD
         << "| PID | Arrival | Burst | Priority | Completion | Turnaround | Waiting | Response |"
         << Color::RESET << "\n";
    cout << "  " << Color::BOLD
         << "+-----+---------+-------+----------+------------+------------+---------+----------+"
         << Color::RESET << "\n";

    // Sort processes by PID for consistent display
    vector<Process> sorted = result.processes;
    sort(sorted.begin(), sorted.end(), [](const Process& a, const Process& b) {
        return a.pid < b.pid;
    });

    for (const auto& p : sorted) {
        cout << "  |" << Color::CYAN << setw(4) << p.pid << Color::RESET
             << " |  " << setw(5) << p.arrival_time
             << "  |  " << setw(3) << p.burst_time
             << "  |   " << setw(4) << p.priority << "   "
             << "|    " << setw(5) << p.completion_time << "   "
             << "|    " << setw(5) << p.turnaround_time << "   "
             << "|  " << setw(4) << p.waiting_time << "   "
             << "|   " << setw(4) << p.response_time << "   |" << "\n";
    }

    cout << "  " << Color::BOLD
         << "+-----+---------+-------+----------+------------+------------+---------+----------+"
         << Color::RESET << "\n";

    cout << "  " << Color::GREEN << "  Avg:"
         << setw(43) << " " << "  "
         << Color::BOLD << fixed << setprecision(2)
         << setw(8) << result.avg_turnaround_time << "  "
         << setw(7) << result.avg_waiting_time << "  "
         << setw(8) << result.avg_response_time
         << Color::RESET << "\n";
}

void printComparativeTable(const vector<SchedulingResult>& results) {
    printHeader("COMPARATIVE METRICS TABLE");
    cout << "\n";

    int numAlgs = (int)results.size();
    if (numAlgs == 0) return;

    // Find best values for highlighting
    double bestWT = 1e9, bestTAT = 1e9, bestRT = 1e9, bestCPU = -1;
    double bestTP = -1;
    int bestCS = INT_MAX;

    for (const auto& r : results) {
        if (r.avg_waiting_time < bestWT) bestWT = r.avg_waiting_time;
        if (r.avg_turnaround_time < bestTAT) bestTAT = r.avg_turnaround_time;
        if (r.avg_response_time < bestRT) bestRT = r.avg_response_time;
        if (r.cpu_utilization > bestCPU) bestCPU = r.cpu_utilization;
        if (r.throughput > bestTP) bestTP = r.throughput;
        if (r.context_switches < bestCS) bestCS = r.context_switches;
    }

    // Column width
    const int nameW = 18;
    const int colW = 12;

    // Print header row
    cout << "  " << Color::BOLD << setw(nameW) << left << "Metric" << Color::RESET;
    for (const auto& r : results) {
        string shortName = r.algorithm_name;
        if (shortName == "AI (Round Robin)") shortName = "AI (RR)";
        if (shortName.length() > (size_t)colW) shortName = shortName.substr(0, colW);
        cout << Color::BOLD << Color::CYAN << setw(colW) << right
             << shortName << Color::RESET << " ";
    }
    cout << "\n";

    // Separator
    cout << "  ";
    for (int i = 0; i < nameW + numAlgs * (colW + 1); i++) cout << "-";
    cout << "\n";

    // Avg Waiting Time
    cout << "  " << setw(nameW) << left << "Avg Wait Time";
    for (const auto& r : results) {
        string marker = (fabs(r.avg_waiting_time - bestWT) < 0.001) ? Color::GREEN + Color::BOLD : Color::WHITE;
        string star = (fabs(r.avg_waiting_time - bestWT) < 0.001) ? "*" : " ";
        cout << marker << setw(colW - 1) << right << fixed << setprecision(2)
             << r.avg_waiting_time << star << Color::RESET;
    }
    cout << "\n";

    // Avg Turnaround Time
    cout << "  " << setw(nameW) << left << "Avg Turnaround";
    for (const auto& r : results) {
        string marker = (fabs(r.avg_turnaround_time - bestTAT) < 0.001) ? Color::GREEN + Color::BOLD : Color::WHITE;
        string star = (fabs(r.avg_turnaround_time - bestTAT) < 0.001) ? "*" : " ";
        cout << marker << setw(colW - 1) << right << fixed << setprecision(2)
             << r.avg_turnaround_time << star << Color::RESET;
    }
    cout << "\n";

    // Avg Response Time
    cout << "  " << setw(nameW) << left << "Avg Response";
    for (const auto& r : results) {
        string marker = (fabs(r.avg_response_time - bestRT) < 0.001) ? Color::GREEN + Color::BOLD : Color::WHITE;
        string star = (fabs(r.avg_response_time - bestRT) < 0.001) ? "*" : " ";
        cout << marker << setw(colW - 1) << right << fixed << setprecision(2)
             << r.avg_response_time << star << Color::RESET;
    }
    cout << "\n";

    // CPU Utilization
    cout << "  " << setw(nameW) << left << "CPU Utilization %";
    for (const auto& r : results) {
        string marker = (fabs(r.cpu_utilization - bestCPU) < 0.001) ? Color::GREEN + Color::BOLD : Color::WHITE;
        string star = (fabs(r.cpu_utilization - bestCPU) < 0.001) ? "*" : " ";
        cout << marker << setw(colW - 1) << right << fixed << setprecision(1)
             << r.cpu_utilization << star << Color::RESET;
    }
    cout << "\n";

    // Throughput
    cout << "  " << setw(nameW) << left << "Throughput";
    for (const auto& r : results) {
        string marker = (fabs(r.throughput - bestTP) < 0.001) ? Color::GREEN + Color::BOLD : Color::WHITE;
        string star = (fabs(r.throughput - bestTP) < 0.001) ? "*" : " ";
        cout << marker << setw(colW - 1) << right << fixed << setprecision(4)
             << r.throughput << star << Color::RESET;
    }
    cout << "\n";

    // Context Switches
    cout << "  " << setw(nameW) << left << "Context Switches";
    for (const auto& r : results) {
        string marker = (r.context_switches == bestCS) ? Color::GREEN + Color::BOLD : Color::WHITE;
        string star = (r.context_switches == bestCS) ? "*" : " ";
        cout << marker << setw(colW - 1) << right
             << r.context_switches << star << Color::RESET;
    }
    cout << "\n";

    // Total Time
    cout << "  " << setw(nameW) << left << "Total Time";
    for (const auto& r : results) {
        cout << Color::WHITE << setw(colW - 1) << right
             << r.total_time << " " << Color::RESET;
    }
    cout << "\n";

    // Separator
    cout << "  ";
    for (int i = 0; i < nameW + numAlgs * (colW + 1); i++) cout << "-";
    cout << "\n";

    cout << "\n  " << Color::GREEN << Color::BOLD << "* = Best value in that metric"
         << Color::RESET << "\n";

    // ---- Find Overall Best Algorithm ----
    // Score: best in each metric gets 1 point, ties share
    map<int, int> algPoints;
    for (int i = 0; i < numAlgs; i++) algPoints[i] = 0;

    // Count wins
    for (const auto& r : results) {
        int idx = &r - &results[0];
        if (fabs(r.avg_waiting_time - bestWT) < 0.001) algPoints[idx] += 2;
        if (fabs(r.avg_turnaround_time - bestTAT) < 0.001) algPoints[idx] += 2;
        if (fabs(r.avg_response_time - bestRT) < 0.001) algPoints[idx] += 2;
        if (r.context_switches == bestCS) algPoints[idx] += 1;
    }

    int overallBest = 0;
    for (int i = 1; i < numAlgs; i++) {
        if (algPoints[i] > algPoints[overallBest]) overallBest = i;
    }

    cout << "\n";
    cout << "  " << Color::BOLD << Color::MAGENTA
         << "================================================================" << "\n";
    cout << "    OVERALL WINNER:  " << Color::GREEN << Color::BOLD
         << results[overallBest].algorithm_name << Color::RESET << "\n";
    cout << "  " << Color::BOLD << Color::MAGENTA
         << "================================================================"
         << Color::RESET << "\n";

    cout << "\n  " << Color::WHITE << "This algorithm scored the best overall across"
         << " all metrics." << Color::RESET << "\n";
    cout << "  " << Color::DIM << "Waiting time and turnaround time are weighted"
         << " highest in the scoring." << Color::RESET << "\n";
}

// ============================================================================
// SECTION 9: EDUCATIONAL MODE
// ============================================================================

void educationalMode() {
    while (true) {
        clearScreen();
        printHeader("LEARN ABOUT CPU SCHEDULING");
        cout << "\n";
        cout << "  " << Color::WHITE << "Select a topic to learn about:\n\n";
        cout << "    " << Color::CYAN << "[1] " << Color::WHITE << "What is a Process?\n";
        cout << "    " << Color::CYAN << "[2] " << Color::WHITE << "Process States (Lifecycle)\n";
        cout << "    " << Color::CYAN << "[3] " << Color::WHITE << "The Ready Queue\n";
        cout << "    " << Color::CYAN << "[4] " << Color::WHITE << "Context Switching\n";
        cout << "    " << Color::CYAN << "[5] " << Color::WHITE << "Scheduling Metrics Explained\n";
        cout << "    " << Color::CYAN << "[6] " << Color::WHITE << "Starvation & Aging\n";
        cout << "    " << Color::CYAN << "[7] " << Color::WHITE << "Preemptive vs Non-Preemptive\n";
        cout << "    " << Color::CYAN << "[8] " << Color::WHITE << "Algorithm Deep Dive (all 6)\n";
        cout << "    " << Color::CYAN << "[0] " << Color::WHITE << "Back to Main Menu\n";
        cout << Color::RESET << "\n";

        int choice = getValidInt("  Your choice: ", 0, 8);
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "\n";

        switch (choice) {
            case 0: return;

            case 1:
                printSubHeader("WHAT IS A PROCESS?");
                cout << R"(
  A "process" is simply a program that is currently running (or waiting to
  run) on your computer. When you open a web browser, that's a process.
  When you play music, that's another process.

  Your computer's CPU (the brain of the computer) can only run ONE process
  at a time (per core). So when you have multiple programs open, the
  operating system must decide which one gets to use the CPU, and for
  how long. This is called "CPU Scheduling".

  Each process has these properties:
    * PID (Process ID)   - A unique number to identify it
    * Arrival Time       - When the process shows up and wants the CPU
    * Burst Time         - How much total CPU time it needs to finish
    * Priority           - How important it is (lower number = more important)
)" << "\n";
                break;

            case 2:
                printSubHeader("PROCESS STATES (LIFECYCLE)");
                cout << R"(
  A process goes through several states during its life:

    +-------+      +-------+      +---------+      +------------+
    |  NEW  | ---> | READY | ---> | RUNNING | ---> | TERMINATED |
    +-------+      +-------+      +---------+      +------------+
                       ^              |
                       |              |  (preempted or
                       +--------------+   needs I/O)
                                      |
                                      v
                                 +---------+
                                 | WAITING |
                                 +---------+

  * NEW        : The process has just been created
  * READY      : The process is waiting in line for the CPU
  * RUNNING    : The process is currently using the CPU
  * WAITING    : The process is waiting for something (like disk I/O)
  * TERMINATED : The process has finished all its work
)" << "\n";
                break;

            case 3:
                printSubHeader("THE READY QUEUE");
                cout << R"(
  The "Ready Queue" is like a waiting line at a bank. All the processes
  that are ready to use the CPU wait in this queue.

       Ready Queue (waiting for CPU):
       +----+    +----+    +----+    +----+
       | P3 | -> | P1 | -> | P5 | -> | P2 | ---> [CPU]
       +----+    +----+    +----+    +----+

  The CPU scheduler decides which process from the queue gets to run
  next. Different scheduling algorithms use different rules to decide
  the order.

  Think of it like this:
    * FCFS   = First person in line gets served first
    * SJF    = Person with the quickest task goes first
    * RR     = Everyone gets 2 minutes, then goes to the back of the line
    * Priority = VIP customers go first
)" << "\n";
                break;

            case 4:
                printSubHeader("CONTEXT SWITCHING");
                cout << R"(
  A "context switch" happens when the CPU stops running one process and
  starts running a different one. The operating system must:

    1. Save the current process's progress (like bookmarking a page)
    2. Load the new process's saved progress
    3. Start running the new process

  This takes time! In our simulation, we count the number of context
  switches because more switches = more overhead = less efficient.

  Example:
    Time 0-3: Running P1  |
    Time 3:   CONTEXT SWITCH (save P1, load P2)  <-- switch #1
    Time 3-5: Running P2  |
    Time 5:   CONTEXT SWITCH (save P2, load P3)  <-- switch #2
    Time 5-9: Running P3  |

  Non-preemptive algorithms (FCFS, SJF) have fewer context switches
  because processes run until they finish. Preemptive algorithms (SRTF,
  RR, Priority) may have more switches but offer better responsiveness.
)" << "\n";
                break;

            case 5:
                printSubHeader("SCHEDULING METRICS EXPLAINED");
                cout << R"(
  We use several measurements to compare scheduling algorithms:

  1. WAITING TIME (lower is better)
     How long a process waits in the ready queue before it finishes.
     Formula: Turnaround Time - Burst Time

  2. TURNAROUND TIME (lower is better)
     Total time from when a process arrives to when it finishes.
     Formula: Completion Time - Arrival Time

  3. RESPONSE TIME (lower is better)
     Time from arrival to when the process FIRST gets the CPU.
     Important for interactive systems (you want quick feedback!).

  4. CPU UTILIZATION (higher is better)
     Percentage of time the CPU is actually doing work (not idle).
     Formula: (Total Time - Idle Time) / Total Time x 100

  5. THROUGHPUT (higher is better)
     Number of processes completed per time unit.
     Formula: Number of Processes / Total Time

  6. CONTEXT SWITCHES (lower is better)
     Number of times the CPU switched between processes.
     More switches = more wasted time on bookkeeping.
)" << "\n";
                break;

            case 6:
                printSubHeader("STARVATION & AGING");
                cout << R"(
  STARVATION is a problem where a low-priority process NEVER gets to
  run because higher-priority processes keep arriving and cutting in line.

  Example of starvation:
    * Process P1 has priority 10 (very low) and arrives at time 0
    * Every few seconds, a new priority-1 process arrives
    * P1 NEVER gets the CPU because there's always someone more important!
    * P1 "starves" -- it waits forever.

  AGING is the solution to starvation. It works like this:
    * The longer a process waits, the higher its priority gradually becomes
    * Eventually, even a low-priority process becomes high-priority
    * This guarantees every process will eventually get the CPU

  In this program, we increase priority by 1 for every )"
              << AGING_FACTOR << R"( time units
  a process waits in the ready queue. So a priority-10 process that
  has waited )" << (AGING_FACTOR * 5) << R"( time units would become priority 5 (higher!).
)" << "\n";
                break;

            case 7:
                printSubHeader("PREEMPTIVE vs NON-PREEMPTIVE SCHEDULING");
                cout << R"(
  NON-PREEMPTIVE (= "Don't interrupt me!")
    Once a process starts running, it runs until it finishes.
    No one can take the CPU away from it mid-execution.

    Examples: FCFS, SJF
    Pros:  Fewer context switches, simpler
    Cons:  A long process can make everyone else wait ("convoy effect")

  PREEMPTIVE (= "You've had enough, next person's turn!")
    The OS can STOP a running process at any time and give the
    CPU to another process. The stopped process goes back to the
    ready queue and will continue later.

    Examples: SRTF, Round Robin, Priority
    Pros:  Better response time, fairer
    Cons:  More context switches, more complex

  Analogy:
    Non-preemptive = A meeting where the speaker finishes their
                     entire presentation before the next person speaks
    Preemptive     = A meeting with a timer -- each speaker gets
                     5 minutes, then the next person goes
)" << "\n";
                break;

            case 8:
                printSubHeader("ALGORITHM DEEP DIVE");
                cout << R"(
  1. FCFS (First Come, First Served)
     Rule: Whoever arrives first, runs first.
     Type: Non-preemptive
     Like: A queue at a grocery store -- first in line, first served.

  2. SJF (Shortest Job First)
     Rule: The process needing the LEAST CPU time runs first.
     Type: Non-preemptive
     Like: Express checkout -- customers with fewer items go first.

  3. SRTF (Shortest Remaining Time First)
     Rule: The process with the LEAST remaining time runs.
     Type: Preemptive (can interrupt!)
     Like: Express lane, but if someone with fewer items arrives,
           they cut in front of the current person.

  4. Round Robin (RR)
     Rule: Each process gets a fixed time slice ("quantum").
           After the quantum, it goes to the back of the line.
     Type: Preemptive
     Like: A merry-go-round -- everyone gets an equal turn.

  5. Priority Scheduling (with Aging)
     Rule: Highest priority process runs first. Aging prevents
           low-priority processes from waiting forever.
     Type: Preemptive
     Like: VIP access -- but regular customers slowly earn VIP
           status the longer they wait.

  6. Adaptive AI Scheduler
     Rule: Analyzes the workload (how many processes, how different
           they are, etc.) and AUTOMATICALLY picks the best algorithm.
     Like: A smart manager who looks at the crowd and decides
           which queue system would work best today.
)" << "\n";
                break;
        }

        pauseScreen();
    }
}

// ============================================================================
// SECTION 10: MAIN FUNCTION
// ============================================================================

int main() {
    enableAnsiColors();

    vector<Process> processes;
    int timeQuantum = DEFAULT_QUANTUM;
    bool processesLoaded = false;

    while (true) {
        printBanner();

        cout << "  " << Color::BOLD << Color::WHITE
             << "MAIN MENU" << Color::RESET << "\n";
        printSeparator('-', 50);
        cout << "\n";
        cout << "    " << Color::CYAN << "[1] " << Color::WHITE
             << "Enter Processes Manually (keyboard)\n";
        cout << "    " << Color::CYAN << "[2] " << Color::WHITE
             << "Load Processes from File\n";
        cout << "    " << Color::CYAN << "[3] " << Color::WHITE
             << "Use Sample Data " << Color::GREEN
             << "(recommended for beginners)" << Color::RESET << "\n";
        cout << "    " << Color::CYAN << "[4] " << Color::WHITE
             << "Learn About CPU Scheduling (Educational Mode)\n";
        cout << "    " << Color::CYAN << "[5] " << Color::WHITE
             << "Exit Program\n";
        cout << Color::RESET << "\n";

        int choice = getValidInt("  Your choice (1-5): ", 1, 5);
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        switch (choice) {
            case 1: {
                processes = inputManual();
                processesLoaded = !processes.empty();
                break;
            }

            case 2: {
                printSubHeader("FILE INPUT");
                printInfo("Enter the filename (or press Enter for 'sample_processes.txt'):");
                cout << "\n    Filename: ";
                string filename;
                getline(cin, filename);
                if (filename.empty()) filename = "sample_processes.txt";

                // Try to open; if not found, offer to create sample
                ifstream testFile(filename);
                if (!testFile.is_open()) {
                    printWarning("File '" + filename + "' not found.");
                    printInfo("Would you like to create a sample file?");
                    cout << "    " << Color::CYAN << "[1] Yes, create sample file"
                         << Color::RESET << "\n";
                    cout << "    " << Color::CYAN << "[2] No, go back"
                         << Color::RESET << "\n";
                    int sub = getValidInt("    Choice: ", 1, 2);
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    if (sub == 1) {
                        generateSampleFile(filename);
                    } else {
                        break;
                    }
                } else {
                    testFile.close();
                }

                processes = inputFromFile(filename);
                processesLoaded = !processes.empty();
                break;
            }

            case 3: {
                printSubHeader("SAMPLE DATA SELECTION");
                cout << "\n  " << Color::WHITE << "Choose a scenario:\n\n";
                cout << "    " << Color::CYAN << "[1] " << Color::WHITE
                     << "Mixed Workload (8 processes, varied bursts & priorities)\n";
                cout << "    " << Color::CYAN << "[2] " << Color::WHITE
                     << "Simultaneous Arrival (5 processes, all arrive at time 0)\n";
                cout << "    " << Color::CYAN << "[3] " << Color::WHITE
                     << "Starvation Risk (6 processes, extreme priority gaps)\n";
                cout << "    " << Color::CYAN << "[4] " << Color::WHITE
                     << "Short & Similar (6 processes, small uniform bursts)\n";
                cout << Color::RESET << "\n";

                int scenario = getValidInt("  Choose scenario (1-4): ", 1, 4);
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                processes = loadSampleData(scenario);
                processesLoaded = true;
                break;
            }

            case 4:
                educationalMode();
                continue; // Don't proceed to scheduling

            case 5:
                cout << "\n";
                printSuccess("Thank you for using the CPU Scheduler Simulator!");
                cout << "  " << Color::DIM << "Goodbye!" << Color::RESET << "\n\n";
                return 0;
        }

        if (!processesLoaded || processes.empty()) {
            printError("No processes loaded. Please try again.");
            pauseScreen();
            continue;
        }

        // ---- Ask for Round Robin Time Quantum ----
        cout << "\n";
        printSubHeader("ROUND ROBIN TIME QUANTUM");
        printInfo("Round Robin gives each process a fixed time slice.");
        printInfo("A smaller quantum = fairer but more switches.");
        printInfo("A larger quantum  = fewer switches but less responsive.");
        cout << "\n";
        timeQuantum = getValidInt("  Enter time quantum (1-20, recommended 2-4): ",
                                  1, 20);
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        // ---- Run All Scheduling Algorithms ----
        printHeader("RUNNING ALL SCHEDULING ALGORITHMS");
        cout << "\n";
        printInfo("Running 6 scheduling algorithms on your data...\n");

        vector<SchedulingResult> results;

        // 1. FCFS
        cout << "  " << Color::YELLOW << "[1/6] " << Color::WHITE
             << "Running FCFS (First Come, First Served)..." << Color::RESET << "\n";
        results.push_back(runFCFS(processes));
        printSuccess("FCFS complete.");

        // 2. SJF
        cout << "  " << Color::YELLOW << "[2/6] " << Color::WHITE
             << "Running SJF (Shortest Job First)..." << Color::RESET << "\n";
        results.push_back(runSJF(processes));
        printSuccess("SJF complete.");

        // 3. SRTF
        cout << "  " << Color::YELLOW << "[3/6] " << Color::WHITE
             << "Running SRTF (Shortest Remaining Time First)..."
             << Color::RESET << "\n";
        results.push_back(runSRTF(processes));
        printSuccess("SRTF complete.");

        // 4. Round Robin
        cout << "  " << Color::YELLOW << "[4/6] " << Color::WHITE
             << "Running Round Robin (quantum=" << timeQuantum << ")..."
             << Color::RESET << "\n";
        results.push_back(runRoundRobin(processes, timeQuantum));
        printSuccess("Round Robin complete.");

        // 5. Priority
        cout << "  " << Color::YELLOW << "[5/6] " << Color::WHITE
             << "Running Priority Scheduling (with Aging)..."
             << Color::RESET << "\n";
        results.push_back(runPriority(processes));
        printSuccess("Priority complete.");

        // 6. Adaptive AI
        cout << "  " << Color::YELLOW << "[6/6] " << Color::WHITE
             << "Running Adaptive AI Scheduler..." << Color::RESET << "\n";
        results.push_back(runAdaptive(processes, timeQuantum));
        printSuccess("Adaptive AI complete.");

        pauseScreen();

        // ---- Display Results ----

        // Gantt Charts
        clearScreen();
        printHeader("GANTT CHARTS");
        printInfo("Each chart shows which process runs at each time unit.");
        printInfo("Colored blocks = process running, IDLE = CPU not busy.\n");

        for (const auto& r : results) {
            printGanttChart(r);
        }

        pauseScreen();

        // Per-Process Results
        clearScreen();
        printHeader("DETAILED PER-PROCESS RESULTS");
        printInfo("Showing metrics for each process under each algorithm.\n");

        for (const auto& r : results) {
            printProcessResults(r);
            cout << "\n";
        }

        pauseScreen();

        // Comparative Table
        clearScreen();
        printComparativeTable(results);

        cout << "\n";
        pauseScreen();

        // ---- Post-Results Menu ----
        clearScreen();
        printHeader("WHAT WOULD YOU LIKE TO DO NEXT?");
        cout << "\n";
        cout << "    " << Color::CYAN << "[1] " << Color::WHITE
             << "Run again with different data\n";
        cout << "    " << Color::CYAN << "[2] " << Color::WHITE
             << "Learn about CPU scheduling concepts\n";
        cout << "    " << Color::CYAN << "[3] " << Color::WHITE
             << "Exit program\n";
        cout << Color::RESET << "\n";

        int postChoice = getValidInt("  Your choice (1-3): ", 1, 3);
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        switch (postChoice) {
            case 1:
                processesLoaded = false;
                continue;
            case 2:
                educationalMode();
                continue;
            case 3:
                cout << "\n";
                printSuccess("Thank you for using the CPU Scheduler Simulator!");
                cout << "  " << Color::DIM << "Goodbye!" << Color::RESET << "\n\n";
                return 0;
        }
    }

    return 0;
}
