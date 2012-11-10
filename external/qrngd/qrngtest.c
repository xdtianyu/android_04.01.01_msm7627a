/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/types.h>
#include <sys/time.h>
#include <sys/poll.h>

/* Local defines */
#ifndef min
	#define min(a,b) (((a)>(b))?(b):(a))
#endif

typedef unsigned char bool;

#define TRUE 1
#define FALSE 0

/* device to pull random data from */
#define RANDOM_DEVICE       "/dev/random"

/* Buffer for reading random data */
#define MAX_BUFFER 8192
static unsigned char databuf[MAX_BUFFER];

/* Random data check buffer */
static unsigned int rnd_ctr[256];

/* Return codes */
#define EXIT_NO_ERRROR                   0
#define EXIT_BAD_PARAMETER              -1
#define EXIT_COULD_NOT_OPEN_DEVICE      -2
#define EXIT_COULD_NOT_READ_DEVICE      -3
#define EXIT_TIMED_OUT_READING_DEVICE   -4
#define EXIT_RANDOM_TEST_FAILED         -5

/* Version number of this source */
#define APP_VERSION "1.00b"
#define APP_NAME    "qrngtest"

const char *program_version =
APP_NAME " " APP_VERSION "\n"
	"Copyright (c) 2011, Code Aurora Forum. All rights reserved.\n"
	"This is free software; see the source for copying conditions.  There is NO\n"
	"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n";

const char *program_usage =
	"Usage: " APP_NAME " [OPTION...]\n"
	"  -c                 run continuously\n"
	"  -d <iterations>    number of test iterations (default 100)\n"
	"  -q                 do not display run-time status\n"
	"  -r <device name>   random input device (default: /dev/random)\n"
	"  -h                 help (this page)\n";

/* User parameters */
struct user_options {
	bool		run_continuously;
	bool		run_quietly;
	int		test_itr;
	char            input_device_name[128];
};

static void title(void)
{
	printf("%s", program_version);
}

static void usage(void)
{
	printf("%s", program_usage);
}

/* Parse command line parameters */
static int get_user_options(struct user_options *user_ops, int argc, char **argv)
{
	int max_params = argc;
	int itr = 1;			/* skip program name */
	while (itr < max_params) {
		if (argv[itr][0] != '-')
			return -1;

		switch (argv[itr++][1]) {
			case 'c':
				user_ops->run_continuously = TRUE;
				break;

			case 'q':
				user_ops->run_quietly = TRUE;
				break;

			case 'd':
				if (itr < max_params) {
					user_ops->test_itr = atoi(argv[itr++]);
					if (!user_ops->test_itr)
						return -1;
					break;
				}
				else
					return -1;

			case 'r':
				if (itr < max_params) {
					if(strlen(argv[itr]) < sizeof(user_ops->input_device_name))
						strcpy(user_ops->input_device_name, argv[itr++]);
					else
						return -1;
					break;
				}
				else
					return -1;

			case 'h':
				return -1;

			default:
				fprintf(stderr, "ERROR: Bad option: '%s'\n", argv[itr-1]);
				return -1;
		}
	}
	return 0;
}


/* Read (non-blocking) data from the random device */
static int read_src(int fd, void *buf, size_t size)
{
	size_t offset = 0;
	char *chr = (char *) buf;
	ssize_t ret_size;
	struct pollfd fds[1];			/* used for polling file descriptor  */
	int ret;

	/* setup poll() data */
	memset(fds, 0 , sizeof(fds));
	fds[0].fd = fd;
	fds[0].events = POLLIN;

	/* must be a valid size */
	if (!size)
		return -1;

	/* read buffer of data */
	do {
		ret = poll(fds, 1, 2000);
		if (ret == 0)
			return -2;	/* no data to read */

		ret_size = read(fd, chr + offset, size);
		/* any read failure is bad */
		if (ret_size == -1)
			break;
		size -= ret_size;
		offset += ret_size;


	} while (size > 0);

	/* should have read in all of requested data */
	if (size > 0)
		return -1;
	return 0;
}

int main(int argc, char **argv)
{
	struct user_options user_ops;		/* holds user configuration data     */
	int exitval = EXIT_NO_ERRROR;
	int random_fd = 0;
	int ret;
	int done = FALSE;
	int i;
	int pass_itr;
	bool pass_test = TRUE;
	int iterations;

	/* setup user defaults */
	user_ops.run_continuously = FALSE;
	user_ops.run_quietly      = FALSE;
	user_ops.test_itr         = 100;
	strcpy(user_ops.input_device_name, RANDOM_DEVICE);

	/* display application header */
	title();

	/* get user preferences */
	ret = get_user_options(&user_ops, argc, argv);
	if (ret < 0) {
		usage();
		exitval = EXIT_BAD_PARAMETER;
		goto exit;
	}

	/* open random device */
	random_fd = open(user_ops.input_device_name, O_RDONLY);
	if (random_fd < 0) {
		printf("Can't open random device file %s\n", user_ops.input_device_name);
		exitval = EXIT_COULD_NOT_OPEN_DEVICE;
		goto exit;
	}

	/* display test */
	if (!user_ops.run_quietly)
		printf("Testing random numbers, one period displayed for every good %dKB read:\n",
		       MAX_BUFFER / 1024);

	/* run the test */
	iterations = user_ops.test_itr;
	do {
		pass_itr = TRUE;
		/* clear random number checking buffer */
		memset(rnd_ctr, 0, 256);

		/* read a buffer of random numbers */
		ret = read_src(random_fd, databuf, MAX_BUFFER);
		if (ret == -1) {
			printf("\nError reading data!\n");
			exitval = EXIT_COULD_NOT_READ_DEVICE;
			goto exit;
		}
		if (ret == -2) {
			printf("\nTimed out reading data!\n");
			exitval = EXIT_TIMED_OUT_READING_DEVICE;
			goto exit;
		}

		/* count random numbers */
		for (i = 0; i < MAX_BUFFER; ++i) {
			rnd_ctr[databuf[i]]++;
		}

		/* check random numbers to make sure they are not bogus */
		for (i = 0; i < 256; ++i) {
			if (rnd_ctr[i] == 0) {
				pass_itr = FALSE;
				pass_test = FALSE;
			}
		}

		/* display results */
		if (!user_ops.run_quietly) {
			if (pass_itr)
				fprintf(stderr, ".");
			else
				fprintf(stderr, "*");
		}

		/* check when we should stop */
		if (!user_ops.run_continuously) {
			if (--iterations == 0)
				done = TRUE;
		}
	} while (!done);

	if (pass_test)
		printf("\nTest passed!\n");
	else {
		printf("\nTest failed!\n");
		exitval = EXIT_RANDOM_TEST_FAILED;
	}

exit:
	close(random_fd);
	return exitval;

}
