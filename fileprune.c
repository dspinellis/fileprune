/*
 *
 * prune - Prune a set of files, removing older copies
 *
 * (C) Copyright 2002 Diomidis Spinellis
 * 
 * Permission to use, copy, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: \\dds\\src\\sysutil\\fileprune\\RCS\\fileprune.c,v 1.2 2002/12/18 15:07:05 dds Exp $
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define PI 3.141592653589793
#define sqr(x) ((x) * (x))


/* Program options and their arguments */
static int opt_print_del = 0;	/* Do not delete files, just print them */
static int opt_print_keep = 0;	/* Do not delete files, print the retained ones */
static int opt_print_sched = 0;	/* Just print the schedule */
static int opt_count = 0;	/* Keep count files */
unsigned long count;
static int opt_size = 0;	/* Keep size files */
unsigned long size;
static int opt_age = 0;		/* Keep files aged <days */
unsigned long days;
static int opt_exp = 0;		/* Use exponential distribution */
static double exponent = 2.0;
static int opt_gauss = 0;	/* Use Gaussian distribution */
/* Standard deviation for the normal function */
static double sd = 180;
static int opt_fib = 0;		/* Use Fibonacci distribution */
static char opt_timespec = 'm'; /* Time to use */
static int opt_forceprune = 0;	/* Force pruning even if size/count are ok */
static int opt_keepfiles = 0;	/* Keep files even if size/count are not ok */

/* 
 * The normal (Gaussian) distribution function 
 * To see in in GNU plot type:
 * plot [-700:700] 1.0 / (180 * sqrt(2 * 3.141592)) * exp(-(x*x) / 2 / (180*1
80))      
 */
static double
normal(double x)
{
	return 1.0 / (sd * sqrt(2 * PI)) * exp(-sqr(x) / 2 / sqr(sd));
}

static char *argv0;

static void
usage(void)
{
	fprintf(stderr, 
		"usage: %s [-n|-p|-N] [-c count|-s size[k|m|g|t]|-a age[w|m|y]] [-e exp|-g sd|-f] [-t a|m|c] [-FK] file ...\n"
		"-n\t\tDo not delete files; print file names to delete\n"
		"-N\t\tDo not delete files; print file names to retain\n"
		"-p\t\tPrint the specified schedule for count elements\n"
		"-c count\tKeep count files\n"
		"-s size\t\tKeep files of size bytes (can multiply with k, m, g, t)\n"
		"-a age\t\tKeep files up to the specified age\n"
		"\t\t(age is in days, can postfix with w(eeks), m(months) y(years))\n"
		"-e exp\t\tUse an exponential distribution\n"
		"-g sd\t\tUse a Gaussian distribution with given standard deviation\n"
		"-f\t\tUse a Fibonacci distribution\n"
		"-t a|m|c\tFor age use access, modification (default), creation time\n"
		"-F\t\tForce pruning even if size/count have not been exceeded\n"
		"-K\t\tKeep scheduled files even if size/count have been exceeded\n"
		, argv0
	);
	exit(1);
}
static void
error_msg(const char *errmsg)
{
	fprintf(stderr, "%s: %s", argv0, errmsg);
	exit(2);
}

static void
error_pmsg(const char *operation, const char *fname)
{
	fprintf(stderr, "%s: %s(%s): %s", argv0, operation, fname, strerror(errno));
	exit(3);
}


extern char	*optarg;	/* Global argument pointer. */
extern int	optind;		/* Global argv index. */

int getopt(int argc, char *argv[], char *optstring);

/* Forward declarations */
static void stat_files(int argc, char *argv[]);
static void create_schedule(void);
static void print_schedule(void);
static void execute_schedule(void);

main(int argc, char *argv[])
{
	int c;
	char *endptr;

	argv0 = argv[0];
	while ((c = getopt(argc, argv, "nNpc:s:a:e:g:ft:FK")) != EOF)
		switch (c) {
		case 'n':
			opt_print_del = 1;
			break;
		case 'N':
			opt_print_keep = 1;
			break;
		case 'p':
			opt_print_sched = 1;
			break;
		case 'c':
			if (!optarg)
				usage();
			opt_count = 1;
			count = strtoul(optarg, &endptr, 10);
			if (!*optarg || *endptr || count == 0)
				error_msg("Invalid count argument");
			break;
		case 's':
			if (!optarg)
				usage();
			opt_size = 1;
			size = strtoul(optarg, &endptr, 10);
			if (!*optarg || size == 0)
				error_msg("Invalid size argument");
			switch (*endptr) {
			case 't': case 'T': size *= 1024; /* FALLTHROUGH */
			case 'g': case 'G': size *= 1024; /* FALLTHROUGH */
			case 'm': case 'M': size *= 1024; /* FALLTHROUGH */
			case 'k': case 'K': size *= 1024; break;
			case 0: break;
			default: error_msg("Invalid size multiplier");
			}
			break;
		case 'a':
			if (!optarg)
				usage();
			opt_age = 1;
			days = strtoul(optarg, &endptr, 10);
			if (!*optarg || days == 0)
				error_msg("Invalid days argument");
			switch (*endptr) {
			case 'w': case 'W': 
				days *= 7;
				break;
			case 'm': case 'M': 
				days = (int)(days * 30.4375);
				break;
			case 'y': case 'Y': 
				days = (int)(days * 365.25);
				break;
			case 0: 
				break;
			default: 
				error_msg("Invalid date multiplier");
			}
			break;
		case 'e':
			opt_exp = 1;
			if (!optarg)
				break;
			exponent = strtod(optarg, &endptr);
			if (!*optarg || *endptr || exponent <= 0.0)
				error_msg("Invalid exponent argument");
			break;
		case 'g':
			opt_gauss = 1;
			if (!optarg)
				break;
			sd = strtod(optarg, &endptr);
			if (!*optarg || *endptr || exponent < 1.0)
				error_msg("Invalid standard deviation argument");
			break;
		case 'f':
			opt_fib = 1;
			break;
		case 't':
			if (!optarg || !*optarg || !strchr("amc", *optarg))
				error_msg("Invalid time specification");
			opt_timespec = *optarg;
			break;
		case 'K':
			opt_keepfiles = 1;
			break;
		case 'F':
			opt_forceprune = 1;
			break;
		case '?':
			usage();
		}
		if ((opt_print_keep + opt_print_del + opt_print_sched > 1) ||
		    (opt_count + opt_size + opt_age > 1) ||
		    (opt_exp + opt_gauss + opt_fib > 1) ||
		    (opt_print_sched && !opt_count) ||
		    (argv[optind] == NULL && !opt_print_sched))
			usage();
	stat_files(argc - optind, argv + optind);
	create_schedule();
	if (opt_print_sched)
		print_schedule();
	else
		execute_schedule();
	return (0);
}


/*
 * Perform an nth order trapezium integration of f from a to b
 */
static double
trapezium(double (*f)(double), double a, double b, int order)
{
	static double resval;

	if (order == 1)
		resval = (b - a) * (f(a) + f(b)) / 2.0;
	else {
		int i, npoints;
		double x, distance, sum = 0;

		npoints = 1 << (order - 1);
		distance = (b - a) / npoints;
		//printf("np=%d dist=%g\n", npoints, distance);
		x = a + distance / 2.0;
		for (i = 0; i < npoints; i++) {
			//printf("x = %g f = %g\n", x, f(x));
			sum += f(x);
			x += distance;
		}
		resval = (resval + (b - a) * sum / npoints) / 2.0;
	}
	return resval;
}

#define PRECISION 1e-4

/*
 * Integrate f from a to b
 */
static double
integrate(double (*f)(double), double a, double b)
{
	double r, prev;
	int i;

	//printf("Integrate %g %g=", a, b);
	for (i = 1; i < 40; i++) {
		r = trapezium(f, a, b, i);
		//printf("r=%g\n", r);
		if (i > 5 && fabs(r - prev) <= PRECISION * fabs(prev)) {
			//printf("%g\n", r);
			return r;
		}
		prev = r;
	}
	/* Numerical instability */
	assert(0);
}

/* Area under the curve is 1

exp(2)	fib+1	gauss
1	1
2	2
4	3
8	5
16	8
32	13
64	21
128	34
256	55

integral(0, inf) = 0.5
*/

static void *
xmalloc(size_t size)
{
	void *p = malloc(size);
	if (!p)
		error_msg("Out of memory");
	return (p);
}

static char *
xstrdup(const char *str)
{
	char *r = xmalloc(strlen(str) + 1);
	strcpy(r, str);
	return (r);
}


/* Information on the files we are to process */
static struct s_finfo {
	char *name;		/* File name */
	off_t size;		/* Its size in bytes */
	time_t time;		/* Age time (as specified) */
	int todelete;		/* True if delete candidate */
	int deleted;		/* True if deleted */
} *finfo;

static unsigned long nfiles = 0;
static off_t totsize = 0;

/* The pruning schedule and its depth */
static int *schedule;
static int depth;

/*
 * Stat all files setting finfo, nfiles, totsize
 */
static void
stat_files(int argc, char *argv[])
{
	struct stat sb;
	int i;

	nfiles = argc;
	finfo = (struct s_finfo *)xmalloc(argc * sizeof(struct s_finfo));
	for (i = 0; i < argc; i++) {
		if (stat(argv[i], &sb) < 0)
			error_pmsg("stat", argv[i]);
		switch (opt_timespec) {
		case 'a':
			finfo[i].time = sb.st_atime;
			break;
		case 'm':
			finfo[i].time = sb.st_mtime;
			break;
		case 'c':
			finfo[i].time = sb.st_ctime;
			break;
		default:
			assert(0);
		}
		finfo[i].size = sb.st_size;
		totsize += sb.st_size;
		finfo[i].name = xstrdup(argv[i]);
		finfo[i].deleted = finfo[i].todelete = 0;
	}
}

static void
create_schedule(void)
{
	int i;

	if (opt_fib || opt_exp)
		depth = max(count, nfiles);
	else if (opt_gauss) {
		if (opt_size)
			depth = size / (totsize / nfiles);
		else if (opt_age)
			depth = (int)(integrate(normal, 0, days) / integrate(normal, 0, 1));
		else
			depth = max(count, nfiles);
	} else
		depth = nfiles;

	schedule = (int *)xmalloc(sizeof(int) * depth);

	if (opt_fib) {
		schedule[0] = schedule[1] = 1;
		for (i = 2; i < depth; i++)
			schedule[i] = schedule[i - 1] + schedule[i - 2];
	} else if (opt_exp) {
		int v, order;

		schedule[0] = 1;
		for (order = i = 1; i < depth; i++) {
			v = (int)pow(exponent, order++);
			/* Ensure at least a one day increment */
			schedule[i] = max(v, schedule[i - 1] + 1);
		}
	} else if (opt_gauss) {
		/* The total area under the +ve half is 0.5 */
		double area = 0.5 / (depth + 0.5);
		int start = 0;
		int n = 0;
		int current = 1;
		int diff;

		while (n < depth) {
			/* Find how many days area needed to cover area */
			if (integrate(normal, start, current) > area ||
				/* Avoid torturing yourself at the functions's end */
			    (n == depth - 1 && current - start > 2 * diff)) {
				diff = current - start;
				schedule[n++] = start + 1;
				start = current;
				current += diff;
			} else
				current++;
		}
	} else
		for (i = 0; i < depth; i++)
			schedule[i] = i;
}

static void
print_schedule(void)
{
	int i;

	for (i = 0; i < depth; i++)
		printf("%d\n", schedule[i]);
}

/* qsort comparisson function */
static int
bytime(const struct s_finfo *a, const struct s_finfo *b)
{
	return (b->time - a->time);
}

/*
 * Try to prunefile a file - as specified
 */
static void
prunefile(struct s_finfo *f)
{
	if (opt_print_keep || opt_print_del)
		f->deleted = 1;
	else
		if (unlink(f->name) < 0)
			error_pmsg("unlink", f->name);
}

/*
 * delete from oldest to newest until size/count is reached
 * OR delete all files older than age and all candidates
*/
static void
execute_schedule(void)
{
	int fi, si;	/* File and schedule index */
	time_t now;

	qsort(finfo, nfiles, sizeof(struct s_finfo), bytime);
	/* Mark delete candidates */
	time(&now);
	for (fi = nfiles - 1, si = depth - 1; fi >= 0; ) {
		int age = (int)(difftime(now, finfo[fi].time) / 60 / 60 / 24) + 1;
		if (si == -1 || age > schedule[si]) {
			/* File older than our interval: dump it, next file */
			finfo[fi].todelete = 1;
			fi--;
		} else if (age <= schedule[si] && (si == 0 || age > schedule[si - 1])) {
			/* File within our interval: keep it, next interval */
			fi--;
			si--;
		} else
			/* File newer than our interval: next interval */
			si--;
	}
	if (opt_size) {
		off_t currsize = totsize;
		/* Delete candidates */
		for (fi = nfiles - 1; (opt_forceprune || totsize > size) && fi >= 0; fi--) {
			if (finfo[fi].todelete) {
				prunefile(&finfo[fi]);
				totsize -= finfo[fi].size;
			}
		}
		/* Delete non-candidate old files */
		for (fi = nfiles - 1; !opt_keepfiles && totsize > size && fi >= 0; fi--) {
			if (!finfo[fi].todelete) {
				prunefile(&finfo[fi]);
				totsize -= finfo[fi].size;
			}
		}
	} else if (opt_count) {
		int currcount = nfiles;
		/* Delete candidates */
		for (fi = nfiles - 1; (opt_forceprune || currcount > count) && fi >= 0; fi--) {
			if (finfo[fi].todelete) {
				prunefile(&finfo[fi]);
				currcount--;
			}
		}
		/* Delete non-candidate old files */
		for (fi = nfiles - 1; !opt_keepfiles && currcount > count && fi >= 0; fi--) {
			if (!finfo[fi].todelete) {
				prunefile(&finfo[fi]);
				currcount--;
			}
		}
	} else {
		/* Delete candidates */
		for (fi = nfiles - 1; fi >= 0; fi--)
			if (finfo[fi].todelete)
				prunefile(&finfo[fi]);
	}
	if (opt_age) {
		/* Delete all old files */
		time_t limit = now - days * 60 * 60 * 24;
		for (fi = nfiles - 1; fi >= 0; fi--)
			if (!finfo[fi].todelete && finfo[fi].time < limit)
				prunefile(&finfo[fi]);
	}
	if (opt_print_keep || opt_print_del)
		for (fi = 0; fi < nfiles; fi++)
			if ((opt_print_del && finfo[fi].deleted) ||
			    (opt_print_keep && !finfo[fi].deleted))
				printf("%s\n", finfo[fi].name);
}
