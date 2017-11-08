#undef TRACE_SYSTEM
#define TRACE_SYSTEM comip-hotplug

#if !defined(_TRACE_COMIP_HOTPLUG_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_COMIP_HOTPLUG_H

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(load,
	TP_PROTO(u32 cpu_id,
		u32 cpufreq,
		u32 nr_running,
		u64 load_delta,
		unsigned long time_delta),
	TP_ARGS(cpu_id, cpufreq, nr_running, load_delta, time_delta),

	TP_STRUCT__entry(
		__field(u32, cpu_id)
		__field(u32, cpufreq)
		__field(u32, nr_running)
		__field(u64, load_delta)
		__field(unsigned long, time_delta)
	),

	TP_fast_assign(
		__entry->cpu_id = (u32) cpu_id;
		__entry->cpufreq = (u32) cpufreq;
		__entry->nr_running = (u32) nr_running;
		__entry->load_delta = load_delta;
		__entry->time_delta = time_delta;
	),

	TP_printk("cpu_id=%u, cpufreq=%d, nr_running=%d, load_delta=%llu, time_delta=%lu",
		__entry->cpu_id,
		__entry->cpufreq,
		__entry->nr_running,
		__entry->load_delta,
		__entry->time_delta)
);

DEFINE_EVENT(load, comip_hotplug_load_raw,
	TP_PROTO(u32 cpu_id, u32 cpufreq, u32 nr_running,
		u64 load_delta, unsigned long time_delta),
	TP_ARGS(cpu_id, cpufreq, nr_running, load_delta, time_delta)
);

DECLARE_EVENT_CLASS(info,
	TP_PROTO(unsigned long load,
		unsigned long avg_load, unsigned long cur_freq,
		unsigned long nr_rq, unsigned long nr_online),
	TP_ARGS(load, avg_load, cur_freq, nr_rq, nr_online),

	TP_STRUCT__entry(
		__field(unsigned long, load)
		__field(unsigned long, avg_load)
		__field(unsigned long, cur_freq)
		__field(unsigned long, nr_rq)
		__field(unsigned long, nr_online)
	),

	TP_fast_assign(
		__entry->load = load;
		__entry->avg_load = avg_load;
		__entry->cur_freq = cur_freq;
		__entry->nr_rq = nr_rq;
		__entry->nr_online = nr_online;
	),

	TP_printk("nr_online_cpus=%lu :avg_load=%lu :threshold_up=%lu, threshold_down=%lu nr_rq_min=%lu",
		__entry->load, __entry->avg_load, __entry->cur_freq,
		__entry->nr_rq, __entry->nr_online)
);

DEFINE_EVENT(info, comip_hotplug_info,
	TP_PROTO(unsigned long load, unsigned long avg_load,
		unsigned long cur_freq, unsigned long nr_rq,
		unsigned long nr_online),
	TP_ARGS(load, avg_load, cur_freq, nr_rq, nr_online)
);

DECLARE_EVENT_CLASS(stat,
	TP_PROTO(unsigned long load,
		unsigned long avg_load, unsigned long cur_freq,
		unsigned long nr_rq),
	TP_ARGS(load, avg_load, cur_freq, nr_rq),

	TP_STRUCT__entry(
		__field(unsigned long, load)
		__field(unsigned long, avg_load)
		__field(unsigned long, cur_freq)
		__field(unsigned long, nr_rq)
	),

	TP_fast_assign(
		__entry->load = load;
		__entry->avg_load = avg_load;
		__entry->cur_freq = cur_freq;
		__entry->nr_rq = nr_rq;
	),

	TP_printk("state =%lu :nr_running=%lu :num_online_cpus=%lu, sample_rate=%lu",
		__entry->load, __entry->avg_load, __entry->cur_freq,
		__entry->nr_rq)
);

DEFINE_EVENT(stat, comip_hotplug_stat,
	TP_PROTO(unsigned long load, unsigned long avg_load,
		unsigned long cur_freq, unsigned long nr_rq),
	TP_ARGS(load, avg_load, cur_freq, nr_rq)
);

#endif /* _TRACE_COMIP_HOTPLUG_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
