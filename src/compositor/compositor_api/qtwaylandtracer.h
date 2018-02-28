/*
 * qtwaylandtracer.h
 *
 * Tracepoint API for using LTTng UST tracing in qtwayland.
 *
 * Copyright (c) 2016 LG Electronics, Inc.
 */

#ifndef qtwaylandtracer_h
#define qtwaylandtracer_h

#ifdef HAS_LTTNG

#include "pmtrace_qtwayland_provider.h"

/* PMTRACE_QTWL_LOG is for free form tracing. Provide a string
   which uniquely identifies your trace point. */
#define PMTRACE_QTWL(label) \
    tracepoint(qtwayland, message, label)

/* PMTRACE_QTWL_COORDINATE is for printing a touch coordinate. Provide a prefix
 * string in "prefix" followed by the "x" and "y" integer coordinates and a
 * "suffix" string.
 */
#define PMTRACE_QTWL_COORDINATE(item, x, y) \
    tracepoint(qtwayland, touchcoordinate, item, x, y)

/* PMTRACE_QTWL_BEFORE / AFTER is for tracing a time duration
 * which is not contained within a scope (curly braces) or function,
 * or in C code where there is no mechanism to automatically detect
 * exiting a scope or function.
 */
#define PMTRACE_QTWL_BEFORE(label) \
    tracepoint(qtwayland, before, label)
#define PMTRACE_QTWL_AFTER(label) \
    tracepoint(qtwayland, after, label)

/* PMTRACE_QTWL_SCOPE* is for tracing a the duration of a scope.  In
 * C++ code use PMTRACE_SCOPE only, in C code use the
 * ENTRY/EXIT macros and be careful to catch all exit cases.
 */
#define PMTRACE_QTWL_SCOPE_ENTRY(label) \
    tracepoint(qtwayland, scope_entry, label)
#define PMTRACE_QTWL_SCOPE_EXIT(label) \
    tracepoint(qtwayland, scope_exit, label)
#define PMTRACE_QTWL_SCOPE(label) \
    PmTraceQtwlScope traceScope(label)

/* PMTRACE_QTWL_FUNCTION* is for tracing a the duration of a scope.
 * In C++ code use PMTRACE_QTWL_FUNCTION only, in C code use the
 * ENTRY/EXIT macros and be careful to catch all exit cases.
 */
#define PMTRACE_QTWL_FUNCTION_ENTRY(label) \
    tracepoint(qtwayland, function_entry, label)
#define PMTRACE_QTWL_FUNCTION_EXIT(label) \
    tracepoint(qtwayland, function_exit, label)
#define PMTRACE_QTWL_FUNCTION \
    PmTraceQtwlFunction traceFunction(const_cast<char*>(Q_FUNC_INFO))

class PmTraceQtwlScope {
public:
    PmTraceQtwlScope(char* label)
        : scopeLabel(label)
    {
        PMTRACE_QTWL_SCOPE_ENTRY(scopeLabel);
    }

    ~PmTraceQtwlScope()
    {
        PMTRACE_QTWL_SCOPE_EXIT(scopeLabel);
    }

private:
    char* scopeLabel;

    // Prevent heap allocation
    void operator delete(void*);
    void* operator new(size_t);
    PmTraceQtwlScope(const PmTraceQtwlScope&);
    PmTraceQtwlScope& operator=(const PmTraceQtwlScope&);
};

class PmTraceQtwlFunction {
public:
    PmTraceQtwlFunction(char* label)
        : fnLabel(label)
    {
        PMTRACE_QTWL_FUNCTION_ENTRY(fnLabel);
    }

    ~PmTraceQtwlFunction()
    {
        PMTRACE_QTWL_FUNCTION_EXIT(fnLabel);
    }

private:
    char* fnLabel;

    // Prevent heap allocation
    void operator delete(void*);
    void* operator new(size_t);
    PmTraceQtwlFunction(const PmTraceQtwlFunction&);
    PmTraceQtwlFunction& operator=(const PmTraceQtwlFunction&);
};
#else // HAS_LTNG

#define PMTRACE_QTWL(label)
#define PMTRACE_QTWL_COORDINATE(item, x, y)
#define PMTRACE_QTWL_BEFORE(label)
#define PMTRACE_QTWL_AFTER(label)
#define PMTRACE_QTWL_SCOPE_ENTRY(label)
#define PMTRACE_QTWL_SCOPE_EXIT(label)
#define PMTRACE_QTWL_SCOPE(label)
#define PMTRACE_QTWL_FUNCTION_ENTRY(label)
#define PMTRACE_QTWL_FUNCTION_EXIT(label)
#define PMTRACE_QTWL_FUNCTION

#endif // HAS_LTTNG

#endif // qtwaylandtracer_h
