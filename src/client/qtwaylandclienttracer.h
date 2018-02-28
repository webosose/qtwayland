/*
 * qtwaylandclienttracer.h
 *
 * Tracepoint API for using LTTng UST tracing in qtwayland client.
 *
 * Copyright (c) 2016 LG Electronics, Inc.
 */

#ifndef qtwaylandclienttracer_h
#define qtwaylandclienttracer_h

#ifdef HAS_LTTNG

#include "pmtrace_qtwaylandclient_provider.h"

/* PMTRACE_QTWLCLI_LOG is for free form tracing. Provide a string
   which uniquely identifies your trace point. */
#define PMTRACE_QTWLCLI(label) \
    tracepoint(qtwayland_client, message, label)

/* PMTRACE_QTWLCLI_COORDINATE is for printing a touch coordinate. Provide a prefix
 * string in "prefix" followed by the "x" and "y" integer coordinates and a
 * "suffix" string.
 */
#define PMTRACE_QTWLCLI_COORDINATE(item, x, y) \
    tracepoint(qtwayland_client, touchcoordinate, item, x, y)

/* PMTRACE_QTWLCLI_BEFORE / AFTER is for tracing a time duration
 * which is not contained within a scope (curly braces) or function,
 * or in C code where there is no mechanism to automatically detect
 * exiting a scope or function.
 */
#define PMTRACE_QTWLCLI_BEFORE(label) \
    tracepoint(qtwayland_client, before, label)
#define PMTRACE_QTWLCLI_AFTER(label) \
    tracepoint(qtwayland_client, after, label)

/* PMTRACE_QTWLCLI_SCOPE* is for tracing a the duration of a scope.  In
 * C++ code use PMTRACE_SCOPE only, in C code use the
 * ENTRY/EXIT macros and be careful to catch all exit cases.
 */
#define PMTRACE_QTWLCLI_SCOPE_ENTRY(label) \
    tracepoint(qtwayland_client, scope_entry, label)
#define PMTRACE_QTWLCLI_SCOPE_EXIT(label) \
    tracepoint(qtwayland_client, scope_exit, label)
#define PMTRACE_QTWLCLI_SCOPE(label) \
    PmTraceQtwlcliScope traceScope(label)

/* PMTRACE_QTWLCLI_FUNCTION* is for tracing a the duration of a scope.
 * In C++ code use PMTRACE_QTWLCLI_FUNCTION only, in C code use the
 * ENTRY/EXIT macros and be careful to catch all exit cases.
 */
#define PMTRACE_QTWLCLI_FUNCTION_ENTRY(label) \
    tracepoint(qtwayland_client, function_entry, label)
#define PMTRACE_QTWLCLI_FUNCTION_EXIT(label) \
    tracepoint(qtwayland_client, function_exit, label)
#define PMTRACE_QTWLCLI_FUNCTION \
    PmTraceQtwlcliFunction traceFunction(const_cast<char*>(Q_FUNC_INFO))

class PmTraceQtwlcliScope {
public:
    PmTraceQtwlcliScope(char* label)
        : scopeLabel(label)
    {
        PMTRACE_QTWLCLI_SCOPE_ENTRY(scopeLabel);
    }

    ~PmTraceQtwlcliScope()
    {
        PMTRACE_QTWLCLI_SCOPE_EXIT(scopeLabel);
    }

private:
    char* scopeLabel;

    // Prevent heap allocation
    void operator delete(void*);
    void* operator new(size_t);
    PmTraceQtwlcliScope(const PmTraceQtwlcliScope&);
    PmTraceQtwlcliScope& operator=(const PmTraceQtwlcliScope&);
};

class PmTraceQtwlcliFunction {
public:
    PmTraceQtwlcliFunction(char* label)
        : fnLabel(label)
    {
        PMTRACE_QTWLCLI_FUNCTION_ENTRY(fnLabel);
    }

    ~PmTraceQtwlcliFunction()
    {
        PMTRACE_QTWLCLI_FUNCTION_EXIT(fnLabel);
    }

private:
    char* fnLabel;

    // Prevent heap allocation
    void operator delete(void*);
    void* operator new(size_t);
    PmTraceQtwlcliFunction(const PmTraceQtwlcliFunction&);
    PmTraceQtwlcliFunction& operator=(const PmTraceQtwlcliFunction&);
};
#else // HAS_LTNG

#define PMTRACE_QTWLCLI(label)
#define PMTRACE_QTWLCLI_COORDINATE(item, x, y)
#define PMTRACE_QTWLCLI_BEFORE(label)
#define PMTRACE_QTWLCLI_AFTER(label)
#define PMTRACE_QTWLCLI_SCOPE_ENTRY(label)
#define PMTRACE_QTWLCLI_SCOPE_EXIT(label)
#define PMTRACE_QTWLCLI_SCOPE(label)
#define PMTRACE_QTWLCLI_FUNCTION_ENTRY(label)
#define PMTRACE_QTWLCLI_FUNCTION_EXIT(label)
#define PMTRACE_QTWLCLI_FUNCTION

#endif // HAS_LTTNG

#endif // qtwaylandclienttracer_h
