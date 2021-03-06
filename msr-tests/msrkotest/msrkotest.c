#include <time.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

char* g_PROGNAME = NULL;
char* g_PROGVERSION = "0.0.1";
unsigned int g_ITERATIONS = 500000000;
unsigned int g_MSR = 0x017A;
unsigned long long g_MSRDATA = 0x0LL;
unsigned int g_MYCPU = 0;
unsigned int g_MSRCPU = 0;

void Usage()
{
	fprintf(stderr, "Usage:\n %s ", g_PROGNAME);
	fprintf(stderr,
		" -msr # (default is 0x%04X)\n"
		"          -data # (default is 0x%llX)\n"
		"          -i # (default is %d)\n"
		"          -src # (Source CPU, default is %d)\n"
		"          -target # (Target CPU, default is %d)\n"
		"\n\n",
		g_MSR,
		g_MSRDATA,
		g_ITERATIONS,
		g_MYCPU,
		g_MSRCPU);
	_exit(-1);
}

void set_affinity(int usecpu)
{
	int mycpu;
	int cpucheck;
	size_t cpusetsz;
	cpu_set_t *cpuset;
	size_t size;

	if ((cpuset = CPU_ALLOC(128)) == NULL) {
		perror("CPU_ALLOC");
		_exit(1);
	}

	size = CPU_ALLOC_SIZE(128);

	CPU_ZERO_S(size, cpuset);
	CPU_SET_S(usecpu, size, cpuset);

	if (sched_setaffinity(0, size, cpuset) < 0) {
		perror("sched_setaffinity: SKIPPED");
		return;
	}

	if ((mycpu = sched_getcpu()) < 0) {
		perror("GetCPU Failed");
		_exit(2);
	}

	if (mycpu != usecpu) {
		fprintf(stderr, "Did not bind to processor %d\n", usecpu);
		_exit(2);
	}
}

void gettimes(
	struct timespec *start,
	struct timespec *end,
	double *ns_per_io,
	double *mghz)
{
	unsigned long long nsdelta = 1000000000 * (end->tv_sec - start->tv_sec);

	if (end->tv_nsec <= start->tv_nsec)
		nsdelta += ((1000000000 - end->tv_sec) + start->tv_nsec);
	else
		nsdelta += (end->tv_nsec - start->tv_nsec);

	*ns_per_io = (double)nsdelta / g_ITERATIONS;
	*mghz = (1000 / *ns_per_io);
}

int main(int ac, char** av)
{
	int argidx = 0;
	char filename[80];
	unsigned long long data;
	int fd;
	struct timespec st, et;
	unsigned long long nsdelta;
	double nspio, mhrz;

	/* Process Command line arguments */
	g_PROGNAME = av[argidx++];
	ac--;

	if (ac & 1)
		Usage();

	for ( ; ac >= 2; ac -= 2 ) {
		char *arg = av[argidx++];
		char *arg2 = av[argidx++];
		if (strcmp(arg, "-msr") == 0) {
			g_MSR = strtoul(arg2, NULL, 0);
		} else if (strcmp(arg, "-data") == 0) {
			g_MSRDATA = strtoull(arg2, NULL, 0);
		} else if (strcmp(arg, "-i") == 0) {
			g_ITERATIONS = strtoul(arg2, NULL, 0);
		} else if (strcmp(arg, "-src") == 0) {
			g_MYCPU = strtoul(arg2, NULL, 0);
		} else if (strcmp(arg, "-target") == 0) {
			g_MSRCPU = strtoul(arg2, NULL, 0);
		} else {
			fprintf(stderr, "Invalid argument %s\n", arg);
			Usage();
		}
	}

	set_affinity(g_MYCPU);
	sprintf(filename, "/dev/cpu/%d/msr", g_MSRCPU);

	if ((fd = open(filename, O_RDWR)) < 0) {
		perror(filename);
		_exit(-1);
	}

	if (g_MSRDATA == 0LL) {	/* Assume read */
		clock_gettime(CLOCK_MONOTONIC, &st);
		for (int i = 0; i < g_ITERATIONS; i++) {
			if (pread(fd, &data, sizeof(data), g_MSR) < 0) {
				perror("pread");
				_exit(-2);
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &et);
		gettimes(&st, &et, &nspio, &mhrz);
		printf("rdmsr(0x%04X) Source CPU %2d, Target CPU %2d, (%u times): "
			"%f ns per msrop, %f Mhz\n",
			g_MSR, g_MYCPU, g_MSRCPU,
			g_ITERATIONS, nspio, mhrz);
	}
	else {
		clock_gettime(CLOCK_MONOTONIC, &st);
		for (int i = 0; i < g_ITERATIONS; i++) {
			if (pwrite(fd, &g_MSRDATA, sizeof(g_MSRDATA), g_MSR) < 0) {
				perror("pwrite");
				_exit(-2);
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &et);
		gettimes(&st, &et, &nspio, &mhrz);

		printf("wrmsr(0x%04X) Source CPU %2d, Target CPU %2d, (%u times): "
			"%f ns per msrop, %f Mhz\n",
			g_MSR, g_MYCPU, g_MSRCPU,
			g_ITERATIONS, nspio, mhrz);
	}

	(void)close(fd);
	return 0;
}
