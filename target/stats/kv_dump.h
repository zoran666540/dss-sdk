/**
 *   BSD LICENSE
 *
 *   Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <kv_stats.h>

typedef struct segment_cn_table_s {
	int 			nr_cnt;
	kv_stats_counters_table_t	 *cnt_table;
} segment_cnt_table_t;

segment_cnt_table_t segment_cnt_table[MAX_SEGMENT];

typedef struct counter_info_s {
	char			name[CNT_NAME_SZ];
	counter_type	type;
} counter_info_t;

typedef struct counter_data_s {
	union {
		unsigned long long	numbers;
		void 			*name;
	} value;
} counter_data_t;

typedef struct segment_info_s {
	int 			nr_cnt;
	counter_info_t	 *cnt_info;
} segment_info_t;

// head info include the counter descriptions for each segment.
typedef struct file_header_s {
	int version;
	int nr_seg;
	int reserved[2];
	segment_info_t *segs;
} file_header_t;

//each sampling include a meta info for nr_cpu and timestamp for this sampling
typedef struct sample_meta_s {
	int size;
	int	timestamp_sec;
	int	timestamp_usec;
	int nr_cpu;
} sample_meta_t;

//each task refers to one cpu stats instance, each task might take care of different number of counters
typedef struct task_meta_s {
	int		cpu_id;
	int		nr_seg;
	int		chunks_per_segment[MAX_SEGMENT];
} task_meta_t;

//the packed sampling counters for each task (cpu),
//concatenated segment by segment
//for each segment, counters/string values are chunk by chunk concatenated.
typedef struct cpu_data_s {
	counter_data_t *data;
} cpu_data_t;

