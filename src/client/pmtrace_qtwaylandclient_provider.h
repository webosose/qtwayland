/*
 * pmtrace_qtwaylandclient_provider.h
 *
 * Tracepoint provider file for LTTng UST tracing in qtwayland client.
 *
 * For more information on see:
 *    http://lttng.org/files/doc/man-pages/man3/lttng-ust.3.html
 *
 * The application level API to these tracepoints is in qtwaylandclienttracer.h
 *
 * Copyright (c) 2016 LG Electronics, Inc.
 */

#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER qtwayland_client

#undef TRACEPOINT_INCLUDE_FILE
#define TRACEPOINT_INCLUDE_FILE ./pmtrace_qtwaylandclient_provider.h

#ifdef __cplusplus
extern "C"{
#endif /*__cplusplus */


#if !defined(_PMTRACE_QTWAYLANDCLIENT_PROVIDER_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _PMTRACE_QTWAYLANDCLIENT_PROVIDER_H

#include <lttng/tracepoint.h>

/* "message" tracepoint should be used for single event trace points */
TRACEPOINT_EVENT(
    qtwayland_client,
    message,
    TP_ARGS(char*, text),
    TP_FIELDS(ctf_string(scope, text)))
/* "touchcoordinate" tracepoint is for printing touch event trace points */
TRACEPOINT_EVENT(
    qtwayland_client,
    touchcoordinate,
    TP_ARGS(char *, item, float, x, float, y),
    TP_FIELDS(ctf_string(item, item)
              ctf_float(float, xcoord, x)
              ctf_float(float, ycoord, y)))
/* "item" tracepoint should be used for name/value pairs */
TRACEPOINT_EVENT(
    qtwayland_client,
    item,
    TP_ARGS(char*, text1, char*, text2),
    TP_FIELDS(ctf_string(name, text1) ctf_string(value, text2)))
/* "position" tracepoint records a message and two integer parameters */
TRACEPOINT_EVENT(
    qtwayland_client,
    position,
    TP_ARGS(char*, text, int, x, int, y),
    TP_FIELDS(ctf_string(scope, text)
              ctf_integer(int, xPos, x)
              ctf_integer(int, yPos, y)))
/* "before"/"after" tracepoint should be used for measuring the
   duration of something that doesn't correspond with a function call or scope */
TRACEPOINT_EVENT(
    qtwayland_client,
    before,
    TP_ARGS(char*, text),
    TP_FIELDS(ctf_string(scope, text)))
TRACEPOINT_EVENT(
    qtwayland_client,
    after,
    TP_ARGS(char*, text),
    TP_FIELDS(ctf_string(scope, text)))
/* "scope_entry"/"scope_exit" tracepoints should be used only by
   PmtraceTraceScope class to measure the duration of a scope within
   a function in C++ code. In C code these may be used directly for
   the same purpose, just make sure you trace any early exit from the
   scope such as break statements or gotos.  */
TRACEPOINT_EVENT(
    qtwayland_client,
    scope_entry,
    TP_ARGS(char*, text),
    TP_FIELDS(ctf_string(scope, text)))
TRACEPOINT_EVENT(
    qtwayland_client,
    scope_exit,
    TP_ARGS(char*, text),
    TP_FIELDS(ctf_string(scope, text)))
/* "function_entry"/"function_exit" tracepoints should be used only by
   PmtraceTraceFunction class to measure the duration of a function
   in C++ code. In C code it may be used directly for the same
   purpose, just make sure you capture any early exit from the
   function such as return statements. */
TRACEPOINT_EVENT(
    qtwayland_client,
    function_entry,
    TP_ARGS(char*, text),
    TP_FIELDS(ctf_string(scope, text)))
TRACEPOINT_EVENT(
    qtwayland_client,
    function_exit,
    TP_ARGS(char*, text),
    TP_FIELDS(ctf_string(scope, text)))

#endif /* _PMTRACE_QTWAYLANDCLIENT_PROVIDER_H */

#ifdef __cplusplus
}
#endif /*__cplusplus */

