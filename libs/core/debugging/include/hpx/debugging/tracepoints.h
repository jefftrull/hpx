// Set up LTTNG tracepoints for certain parallel algorithms

// this is a Linux-only facility AFAICT
#ifdef __linux__

#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER HPX

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE <hpx/debugging/tracepoints.h>

#if !defined(_HPX_TRACEPOINTS_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _HPX_TRACEPOINTS_H

#include <lttng/tracepoint.h>

// the execution of a "chunk" of work by a single thread
// these are for stages 1 and 3 and store duration, priority, and their data range
TRACEPOINT_EVENT(
    HPX,
    chunk_start,
    TP_ARGS(
        std::size_t, start_ofs,
        std::size_t, stop_ofs,
        int, stage,
        int, prio
    ),
    TP_FIELDS(
        ctf_integer(std::size_t, start_ofs, start_ofs)
        ctf_integer(std::size_t, stop_ofs, stop_ofs)
        ctf_integer(int, stage, stage)
        ctf_integer(int, prio, prio)
    )
)

TRACEPOINT_EVENT(
    HPX,
    chunk_stop,
    TP_ARGS(
        std::size_t, start_ofs,
        std::size_t, stop_ofs,
        int, stage,
        int, prio
    ),
    TP_FIELDS(
        ctf_integer(std::size_t, start_ofs, start_ofs)
        ctf_integer(std::size_t, stop_ofs, stop_ofs)
        ctf_integer(int, stage, stage)
        ctf_integer(int, prio, prio)
    )
)

// finally a single event for stage 2, which generally does just one simple op
// so we omit the stage number and data range
TRACEPOINT_EVENT(
    HPX,
    stage2,
    TP_ARGS(
        std::size_t, loc,
        int, prio
    ),
    TP_FIELDS(
        ctf_integer(std::size_t, loc, loc)
        ctf_integer(int, prio, prio)
    )
)


TRACEPOINT_EVENT(
    HPX,
    tasks_created,
    TP_ARGS(),
    TP_FIELDS()
)

// scheduling info
TRACEPOINT_EVENT(
    HPX,
    thread_create_normalprio,
    TP_ARGS(
        std::size_t, num
    ),
    TP_FIELDS(
        ctf_integer(std::size_t, num, num)
    )
)

TRACEPOINT_EVENT(
    HPX,
    thread_create_highprio,
    TP_ARGS(
        std::size_t, num
    ),
    TP_FIELDS(
        ctf_integer(std::size_t, num, num)
    )
)

#endif // !defined(_HPX_TRACEPOINTS_H) || ...

#include <lttng/tracepoint-event.h>

#endif // __linux__
