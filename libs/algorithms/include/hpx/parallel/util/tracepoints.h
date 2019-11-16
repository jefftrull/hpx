// Set up LTTNG tracepoints for certain parallel algorithms

// this is a Linux-only facility AFAICT
#ifdef __linux__

#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER HPX

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE <hpx/parallel/util/tracepoints.h>

#if !defined(_HPX_TRACEPOINTS_H) || defined(TRACEPOINT_HEADER_MULTI_READ)

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
        int, prio
    ),
    TP_FIELDS(
        ctf_integer(int, prio, prio)
    )
)

#ifndef _HPX_TRACEPOINTS_H

namespace hpx { namespace parallel { namespace util {
template<typename F>
void logchunk(int stage, std::size_t start, std::size_t stop, int prio, F const & f)
{
#ifdef __linux__
    tracepoint(HPX, chunk_start, start, stop, stage, prio);
    f();
    tracepoint(HPX, chunk_stop, start, stop, stage, prio);
#else
    f();
#endif
}

template<typename F>
void logstage2(int prio, F const & f)
{
    f();

#ifdef __linux__
    // stage 2 is a single op so should be very small
    // makes more sense as a point event
    tracepoint(HPX, stage2, prio);
#endif
}

}}}

#define _HPX_TRACEPOINTS_H
#endif // _HPX_TRACEPOINTS_H

#endif // !defined(_HPX_TRACEPOINTS_H) || ...

#include <lttng/tracepoint-event.h>

#endif // __linux__
