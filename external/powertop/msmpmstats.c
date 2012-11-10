/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of Code Aurora nor
 *      the names of its contributors may be used to endorse or promote
 *      products derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "powertop.h"

char msm_pm_stat_lines[12][200];

void msm_pm_stats(void)
{
	FILE* fp = NULL;
	char buf[256], temp[256];
	char *p;
	int count, time_s, time_ns;
	int i, n = 0;
	static char sleep_types[10][50] = {
				"idle-request",
				"idle-spin",
				"idle-wfi",
				"idle-sleep",
				"idle-failed-sleep",
				"idle-power-collapse",
				"idle-failed-power-collapse",
				"suspend",
				"failed-suspend",
				"not-idle"
				};

	fp = fopen("/proc/msm_pm_stats", "r");
	if (!fp) {
		printf("No msm_pm_stats available in procfs. \
			Enable CONFIG_MSM_IDLE_STATS in the kernel.\n");
		return;
	}

	sprintf(msm_pm_stat_lines[n], "MSM PM idle stats:\n");

	while (!feof(fp)) {
		fgets(buf, 256, fp);
		for (i = n; i < 10; i++) {
			p = buf;
			if (strstr(p, sleep_types[i])) {
				fgets(buf, 256, fp);
				sscanf(buf, "%s%d", temp, &count);
				fgets(buf, 256, fp);
				sscanf(buf, "%s%d.%d", temp,
						&time_s,
						&time_ns);
				sprintf(msm_pm_stat_lines[++n],
						"%s (count = %d) : %d.%ds\n",
						sleep_types[i],
						count,
						time_s,
						time_ns);
				break;
			}
		}
	}

	fclose(fp);
}
