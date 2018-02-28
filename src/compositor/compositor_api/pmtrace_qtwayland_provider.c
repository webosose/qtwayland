/*
 * pmtrace_qtwayland_provider.c
 *
 * Tracepoint provider file for LTTng UST tracing in qtwayland.
 *
 * For more information on see:
 *    http://lttng.org/files/doc/man-pages/man3/lttng-ust.3.html
 *
 * Copyright (c) 2016 LG Electronics, Inc.
 */

/*
 * These #defines alter the behavior of pmtrace_qtwayland_provider.h to define the tracing
 * primitives rather than just declaring them.
 */
#define TRACEPOINT_CREATE_PROBES
#define TRACEPOINT_DEFINE
/*
 * The header containing our TRACEPOINT_EVENTs.
 */
#include "pmtrace_qtwayland_provider.h"

#include <lttng/tracepoint-event.h>
