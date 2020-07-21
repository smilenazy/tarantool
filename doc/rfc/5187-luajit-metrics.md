# LuaJIT metrics

* **Status**: In progress
* **Start date**: 17-07-2020
* **Authors**: Sergey Kaplun @Buristan skaplun@tarantool.org,
               Igor Munkin @igormunkin imun@tarantool.org,
               Sergey Ostanevich @sergos sergos@tarantool.org
* **Issues**: [#5187](https://github.com/tarantool/tarantool/issues/5187)

## Summary

LuaJIT metrics provide extra information about the Lua state. They consists of
GC metrics (overall amount of objects and memory usage), JIT stats (both
related to the compiled traces and the engine itself), string hash hits/misses.

## Background and motivation

One can be curious about their application performance. We are going to provide
various metrics about the several platform subsystems behaviour. GC pressure
produced by user code can weight down all application performance. Irrelevant
traces compiled by the JIT engine can just burn CPU time with no benefits as a
result. String hash collisions can lead to DoS caused by a single request. All
these metrics should be well monitored by users wanting to improve the
performance of their application.

## Detailed design

For C API we introduce additional extension header <lmisclib.h> that provides
interfaces for new LuaJIT C API extensions. The first interface in this header
will be the following:

```
/* API for obtaining various metrics from the platform. */

LUAM_API struct luam_Metrics *luaM_metrics(lua_State *L,
					   struct luam_Metrics *dst);
```

This function fills the structure pointed by `dst` with the corresponding
metrics related to Lua state anchored to the given coroutine `L`. The result of
the function is a pointer to the filled structure (the same `dst` points to).

The `struct luam_Metrics` has the following definition:

```
struct luam_Metrics {
	/*
	 * Strings amount founded in string hash
	 * instead of allocation of new one.
	 */
	size_t strhash_hit;
	/* Strings amount allocated and put into string hash. */
	size_t strhash_miss;

	size_t strnum;   /* Amount of allocated string objects. */
	size_t tabnum;   /* Amount of allocated table objects. */
	size_t udatanum; /* Amount of allocated udata objects. */
	size_t cdatanum; /* Amount of allocated cdata objects. */

	/* Memory currently allocated. */
	size_t gc_total;
	/* Total amount of freed memory. */
	size_t gc_freed;
	/* Total amount of allocated memory. */
	size_t gc_allocated;

	/* Count of incremental GC steps per state. */
	size_t gc_steps_pause;
	size_t gc_steps_propagate;
	size_t gc_steps_atomic;
	size_t gc_steps_sweepstring;
	size_t gc_steps_sweep;
	size_t gc_steps_finalize;

	/*
	 * Overall number of snap restores (and number of stopped
	 * trace executions) for given jit_State.
	 */
	size_t jit_snap_restore;
	/* Overall number of abort traces for given jit_State. */
	size_t jit_trace_abort;
	/* Total size of all allocated machine code areas. */
	size_t jit_mcode_size;
	/* Amount of JIT traces. */
	unsigned int jit_trace_num;
};
```

All metrics are collected throughout the platform uptime. But some of them
(namely `strhash_hit`, `strhash_miss`, `gc_freed`, `gc_allocated`,
`gc_steps_pause`, `gc_steps_propagate`, `gc_steps_atomic`,
`gc_steps_sweepstring`, `gc_steps_sweep`, `gc_steps_finalize`,
`jit_snap_restore` and `jit_trace_abort`) increase monotonic and can overflow.
They make sense only with comparing with their value from a previous
`luaM_metrics()` call.

There is also a complement introduced for Lua space -- `misc.getmetrics()`.
This function is just a wrapper for `luaM_metrics()` returning a Lua table with
the similar metrics. Its usage is quite simple:
```
$ ./src/tarantool
Tarantool 2.5.0-267-gbf047ad44
type 'help' for interactive help
tarantool> misc.getmetrics()
---
- tabnum: 1812
  gc_total: 1369927
  strnum: 5767
  jit_trace_num: 0
  cdatanum: 89
  jit_mcode_size: 0
  udatanum: 17
  jit_snap_restore: 0
  gc_freed: 2239391
  strhash_hit: 53759
  gc_steps_finalize: 0
  gc_allocated: 3609318
  gc_steps_atomic: 6
  gc_steps_sweep: 296
  gc_steps_sweepstring: 17920
  jit_trace_abort: 0
  strhash_miss: 6874
  gc_steps_propagate: 10106
  gc_steps_pause: 7
...
```
