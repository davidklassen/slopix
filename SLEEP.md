# Sleep Channels

## Current Implementation

`ksleep()` in `proc.c` is a busy-wait loop:
```c
void ksleep(unsigned long ticks) {
    unsigned long target = timer_get_ticks() + ticks;
    enable_irq();
    while (timer_get_ticks() < target) {
        yield();
    }
}
```

Process stays RUNNABLE, keeps getting scheduled just to check the clock and yield. `sys_wait()` has the same pattern.

## Problem

- Sleeping process gets scheduled ~100 times per second doing nothing
- Context switch overhead: TLB flushes, page table switches
- CPU never truly idles

## Sleep Channels Solution

Add SLEEPING state and channel pointer to proc:
```c
enum proc_state { UNUSED, RUNNABLE, RUNNING, SLEEPING, ZOMBIE };

struct proc {
    // ...
    void *chan;  // sleep channel (0 if not sleeping)
};
```

Core primitives:
```c
void sleep(void *chan) {
    current->chan = chan;
    current->state = SLEEPING;
    sched();
    current->chan = 0;
}

void wakeup(void *chan) {
    for (int i = 0; i < NPROC; i++) {
        struct proc *p = &procs[i];
        if (p->state == SLEEPING && p->chan == chan)
            p->state = RUNNABLE;
    }
}
```

Scheduler skips SLEEPING processes entirely.

## Time-Based Sleep

For `ksleep(ticks)`, need to wake process after delay. Options:

1. **Check on every timer tick** (simple)
   - Add `wakeup_tick` field to proc
   - Timer handler checks all sleeping procs, wakes those past their time
   - O(n) per tick, but n is small (NPROC=8)

2. **Sorted wake list** (efficient)
   - Maintain list sorted by wakeup time
   - Only check head of list
   - More complex, premature optimization for now

## Blocking I/O

Sleep channels enable efficient blocking read:
```c
// In sys_read(), if no data available:
sleep(&uart_rx_chan);

// In UART RX interrupt handler:
wakeup(&uart_rx_chan);
```

## Timing

Current timer uses ARM Generic Timer (`CNTFRQ_EL0`), which is:
- Independent of CPU frequency
- Standard approach for ARM timekeeping
- 10ms tick granularity (100Hz)

For finer precision later:
- Increase tick rate (more overhead)
- Read `CNTVCT_EL0` directly for sub-tick precision
- Use absolute timer comparisons

## POSIX Compatibility

Sleep channels are internal - syscall API unchanged:
- `sleep(ms)` same signature, just blocks efficiently
- `read()` can block until data (expected POSIX behavior)
- `wait()` blocks until child exits

Current `sleep(ms)` is non-standard (POSIX uses seconds/nanoseconds), but easily wrapped in libc.

## Limitations

Sleep channels alone don't solve "wait on multiple events" (ticker can't quit while sleeping). That needs:
- `select()`/`poll()` - wait on multiple fds
- Signals (SIGINT to interrupt sleep)
- `read()` with timeout

## Implementation Order

1. Add SLEEPING state and `chan` field to proc
2. Implement `sleep(chan)` and `wakeup(chan)` primitives
3. Add `wakeup_tick` field for time-based sleep
4. Timer handler wakes time-sleeping procs
5. Convert `sys_wait()` to use sleep channel
6. Later: blocking `read()` with UART interrupt wakeup
