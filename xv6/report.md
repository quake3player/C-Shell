# Scheduler Comparison Report

## Overview

Implemented two scheduling policies in xv6-riscv:

- Round Robin (default)
- First Come First Serve (FCFS) selected via `make qemu SCHEDULER=FCFS` (macro `SCHED_FCFS`).

A test program `testsched` creates 4 CPU-bound child processes simultaneously. Each child performs a long compute loop then prints total elapsed ticks (`uptime()` delta). Ticks are ~0.1s timer interrupts.

## Expectations

- Round Robin: All runnable children time-slice; turnaround times cluster closely (similar total ticks) and completion order can vary.
- FCFS (non-preemptive except on blocking): First process runs to completion; later processes wait, producing higher variance and larger maxima.

## Empirical Results

### Round Robin Runs

```text
Run 1:
child pid=5 total_ticks=8
child pid=4 total_ticks=8
child pid=6 total_ticks=9
child pid=7 total_ticks=8

Run 2:
child pid=9  total_ticks=7
child pid=10 total_ticks=8
child pid=12 total_ticks=8
child pid=11 total_ticks=8

Run 3:
child pid=19 total_ticks=8
child pid=20 total_ticks=8
child pid=21 total_ticks=9
child pid=22 total_ticks=8
```

Characteristics:

- Tight cluster (7–9 ticks)
- No systematic growth with pid/creation order
- Different completion orders across runs (interleaving due to time slicing)

### FCFS Runs

```text
Run 1:
child pid=4 total_ticks=4
child pid=7 total_ticks=3
child pid=6 total_ticks=7
child pid=5 total_ticks=7

Run 2 (UART garble partially reconstructed):
child pid=11 total_ticks=6
child pid=12 total_ticks=6
child pid=9  total_ticks=15
(one line partly corrupted)

Run 3:
child pid=15 total_ticks=6
child pid=16 total_ticks=6
child pid=17 total_ticks=6
child pid=18 total_ticks=13
```

Characteristics:

- Increased spread; some processes show notably higher turnaround (13–15 ticks)
- Later processes sometimes incur long waits behind earlier CPU-bound work
- Occasional UART output interleaving/garbling (console driver buffering) does not change variance pattern

## Interpretation

Round Robin distributes CPU time evenly; all CPU-bound children progress concurrently, producing similar completion times (fairness, low variance).

FCFS runs one process to completion before later arrivals (unless the running process blocks), inflating turnaround for processes that arrive later. This creates a heavier tail in turnaround distribution and higher maximum values.

## Key Indicators of Difference

| Aspect | Round Robin | FCFS |
|--------|-------------|------|
| Turnaround range | Narrow (7–9) | Wider (3–15) |
| Late process penalty | Minimal | Significant (13–15 ticks) |
| Completion ordering | Interleaved | More serialized / arrival-biased |
| Fair overlap | Yes | No (serialized CPU use) |

## Conclusion

Measurements confirm behavioral divergence: Round Robin enforces fairness via periodic preemption; FCFS increases waiting-time variance and creates longer tail latencies.

## Possible Extensions

- Track per-process `first_run_tick` and `finish_tick` to separate wait vs run time
- Add CPU tick accounting per process
- Implement a Completely Fair Scheduler (CFS) using virtual runtime for proportional fairness

### Scheduler Outputs

#### Round-Robin (Default)

```text
hart 2 starting
hart 1 starting
init: starting sh
$ schedulertest
child 4 pid=8 elapsed=6
child 3 pid=7 elapsed=8
child 5 pid=9 elapsed=8
child 6 pid=10 elapsed=8
child 7 pid=11 elapsed=8
child 0 pid=4 elapsed=50
child 1 pid=5 elapsed=50
child 2 pid=6 elapsed=50
schedulertest total elapsed=50 ticks
```

#### FCFS

```text
init: starting sh
$ schedulertest
child 4 pid=8 elapsed=5
child 5 pid=9 elapsed=5
child 3 pid=7 elapsed=9
child 7 pid=11 elapsed=5
child 6 pid=10 elapsed=9
child 2 pid=6 elapsed=50
child 0 pid=4 elapsed=50
child 1 pid=5 elapsed=50
schedulertest total elapsed=50 ticks
```

#### CFS

```text
hart 1 starting
hart 2 starting
init: starting sh
$ schedulertest
[Scheduler Tick]
PID: 4 | vRuntime: 0
PID: 5 | vRuntime: 0
PID: 6 | vRuntime: 0
PID: 7 | vRuntime: 0
PID: 8 | vRuntime: 0
PID: 9 | vRuntime: 0
PID: 10 | vRuntime: 0
PID: 11 | vRuntime: 0
--> Scheduling PID 4 (lowest vRuntime)
[Scheduler Tick]
PID: 5 | vRuntime: 0
PID: 6 | vRuntime: 0
PID: 7 | vRuntime: 0
PID: 8 | vRuntime: 0
PID: 9 | vRuntime: 0
PID: 10 | vRuntime: 0
PID: 11 | vRuntime: 0
--> Scheduling PID 5 (lowest vRuntime)
[Scheduler Tick]
PID: 6 | vRuntime: 0
PID: 7 | vRuntime: 0
PID: 8 | vRuntime: 0
PID: 9 | vRuntime: 0
PID: 10 | vRuntime: 0
PID: 11 | vRuntime: 0
--> Scheduling PID 6 (lowest vRuntime)
[Scheduler Tick]
PID: 7 | vRuntime: 0
PID: 8 | vRuntime: 0
PID: 9 | vRuntime: 0
PID: 10 | vRuntime: 0
PID: 11 | vRuntime: 0
--> Scheduling PID 7 (lowest vRuntime)
[Scheduler Tick]
PID: 8 | vRuntime: 0
PID: 9 | vRuntime: 0
PID: 10 | vRuntime: 0
PID: 11 | vRuntime: 0
--> Scheduling PID 8 (lowest vRuntime)
[Scheduler Tick]
PID: 9 | vRuntime: 0
PID: 10 | vRuntime: 0
PID: 11 | vRuntime: 0
--> Scheduling PID 9 (lowest vRuntime)
child 5 pid=9 elapsed=[Scheduler Tick]
PID: 10 | vRuntime: 0
PID: 11 | vRuntime: 0
PID: 4 | vRuntime: 0
PID: 5 | vRuntime: 0
PID: 6 | vRuntime: 0
--> Scheduling PID 10 (lowest vRuntime)
child 3 pid=7 elapsed=5
[Scheduler Tick]
PID: 11 | vRuntime: 0
PID: 4 | vRuntime: 0
PID: 5 | vRuntime: 0
PID: 6 | vRuntime: 0
PID: 3 | vRuntime: 0
PID: 9 | vRuntime: 5
--> Scheduling PID 11 (lowest vRuntime)
child 4 pid=8 elapsed=9
[Scheduler Tick]
PID: 4 | vRuntime: 0
PID: 5 | vRuntime: 0
PID: 6 | vRuntime: 0
PID: 3 | vRuntime: 0
PID: 9 | vRuntime: 5
--> Scheduling PID 4 (lowest vRuntime)
[Scheduler Tick]
PID: 5 | vRuntime: 0
PID: 6 | vRuntime: 0
PID: 3 | vRuntime: 0
PID: 9 | vRuntime: 5
--> Scheduling PID 5 (lowest vRuntime)
[Scheduler Tick]
PID: 6 | vRuntime: 0
PID: 3 | vRuntime: 0
PID: 9 | vRuntime: 5
```