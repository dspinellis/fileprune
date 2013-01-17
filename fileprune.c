/*
 *
 * fileprune - Prune a set of files, removing older copies
 *
 * (C) Copyright 2002-2013 Diomidis Spinellis
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

#ifdef unix
#include <unistd.h>	/* unlink, getopt */
#else
int unlink(const char *path);
int getopt(int argc, char *argv[], char *optstring);
/* For math libraries that do not support it */
double erf(double);
#endif

#define PI 3.14159265358979323844
#define SQRT2 1.41421356237309504880

#define sqr(x) ((x) * (x))

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

/* Program options and their arguments */
static int opt_print_del = 0;	/* Do not delete files, just print them */
static int opt_print_keep = 0;	/* Do not delete files, print the retained ones */
static int opt_print_sched = 0;	/* Just print the schedule */
static int opt_count = 0;	/* Keep count files */
static long count;
static int opt_size = 0;	/* Keep size files */
static off_t size;
static int opt_age = 0;		/* Keep files aged <days */
static long days;
static int opt_exp = 0;		/* Use exponential distribution */
static double exponent = 2.0;
static int opt_gauss = 0;	/* Use Gaussian distribution */
/* Standard deviation for the normal function */
static double sd = 180;
static int opt_fib = 0;		/* Use Fibonacci distribution */
static char opt_timespec = 'm'; /* Time to use */
static int opt_timespec_set = 0; /* True if time type was specified */
static int opt_forceprune = 0;	/* Force pruning even if size/count are ok */
static int opt_keepfiles = 0;	/* Keep files even if size/count are not ok */
static int opt_use_date = 0;	/* Use date list rather than actual files */

/* Information on the files we are to process */
static struct s_finfo {
	char *name;		/* File name */
	off_t size;		/* Its size in bytes */
	time_t time;		/* Age time (as specified) */
	int todelete;		/* True if delete candidate */
	int deleted;		/* True if deleted */
} *finfo;

static long nfiles = 0;

static char *argv0;

static void
usage(void)
{
	fprintf(stderr,
		"usage: %s [-n|-p|-N] [-c count|-s size[k|m|g|t]|-a age[w|m|y]]\n"
		"\t[-e exp|-g sd|-f] [-t a|m|c] [-FK] file ...\n"
		"or: %s -d -n|-N [-c count|-a age[w|m|y]] [-e exp|-g sd|-f] [-FK] date ...\n"
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
		"-d\t\tUse a list of ISO dates, rather than actual files\n"
		, argv0, argv0
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


#if defined(_MSC_VER)
/* Microsoft Visual C/C++ lacks these, so we declare them here. */
extern char	*optarg;	/* Global argument pointer. */
extern int	optind;		/* Global argv index. */
#endif


/* Forward declarations */
static void stat_files(int argc, char *argv[]);
static void parse_dates(int argc, char *argv[]);
static void create_schedule(void);
static void print_schedule(void);
static void execute_schedule(void);
static void * xmalloc(size_t size);

int
main(int argc, char *argv[])
{
	int c;
	char *endptr;

	argv0 = argv[0];
	while ((c = getopt(argc, argv, "a:c:de:Ffg:KNnps:t:")) != EOF)
		switch (c) {
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
		case 'c':
			if (!optarg)
				usage();
			opt_count = 1;
			count = strtoul(optarg, &endptr, 10);
			if (!*optarg || *endptr || count == 0)
				error_msg("Invalid count argument");
			break;
		case 'd':
			opt_use_date = 1;
			break;
		case 'e':
			opt_exp = 1;
			if (!optarg)
				break;
			exponent = strtod(optarg, &endptr);
			if (!*optarg || *endptr || exponent <= 0.0)
				error_msg("Invalid exponent argument");
			break;
		case 'F':
			opt_forceprune = 1;
			break;
		case 'f':
			opt_fib = 1;
			break;
		case 'g':
			opt_gauss = 1;
			if (!optarg)
				break;
			sd = strtod(optarg, &endptr);
			if (!*optarg || *endptr || exponent < 1.0)
				error_msg("Invalid standard deviation argument");
			break;
		case 'K':
			opt_keepfiles = 1;
			break;
		case 'N':
			opt_print_keep = 1;
			break;
		case 'n':
			opt_print_del = 1;
			break;
		case 'p':
			opt_print_sched = 1;
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
		case 't':
			if (!optarg || !*optarg || !strchr("amc", *optarg))
				error_msg("Invalid time specification");
			opt_timespec = *optarg;
			opt_timespec_set = 1;
			break;
		case '?':
			usage();
		}

	if (opt_print_keep + opt_print_del + opt_print_sched > 1) {
		fprintf(stderr, "Cannot specify more than one output option\n");
		usage();
	}
	if (opt_count + opt_size + opt_age > 1) {
		fprintf(stderr, "Cannot specify more than one schedule limit option\n");
		usage();
	}
	if (opt_exp + opt_gauss + opt_fib > 1) {
		fprintf(stderr, "Cannot specify more than one schedule type option\n");
		usage();
	}
	if (opt_print_sched && !opt_count) {
		fprintf(stderr, "The schedule print option requires a count specification\n");
		usage();
	}
	if (argv[optind] == NULL && !opt_print_sched) {
		fprintf(stderr, "Required file or date arguments are missing\n");
		usage();
	}
	if (opt_use_date && (opt_timespec_set || opt_size ||
			       !(opt_print_keep || opt_print_del))) {
		fprintf(stderr, "The -d option requires -N or -n and cannot be used with -t and -s\n");
		usage();
	}

	/* Create finfo array */
	nfiles = argc - optind;
	finfo = (struct s_finfo *)xmalloc(nfiles * sizeof(struct s_finfo));
	if (opt_use_date)
		parse_dates(argc - optind, argv + optind);
	else
		stat_files(argc - optind, argv + optind);

	create_schedule();

	if (opt_print_sched)
		print_schedule();
	else
		execute_schedule();
	return (0);
}

/*
 * The Gaussian function cumulative distribution function
 * for mean == 0
 * See http://mathworld.wolfram.com/GaussianDistribution.html
 * The erf "error function" is available under most Unix math libraries
 */
static double
D(double x)
{
	return fabs(1. + erf(x / sd / SQRT2)) / 2.0;
}

/* Checked malloc */
static void *
xmalloc(size_t size)
{
	void *p = malloc(size);
	if (!p)
		error_msg("Out of memory");
	return (p);
}

/* Checked strdup */
static char *
xstrdup(const char *str)
{
	char *r = xmalloc(strlen(str) + 1);
	strcpy(r, str);
	return (r);
}


static off_t totsize = 0;

/* The pruning schedule and its depth */
static int *schedule;
static int depth;

/*
 * Stat all files setting finfo elements and totsize
 */
static void
stat_files(int argc, char *argv[])
{
	struct stat sb;
	int i;

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

/*
 * Parse a list of dates setting finfo elements
 */
static void
parse_dates(int argc, char *argv[])
{
	int i;
	struct tm t;

	for (i = 0; i < argc; i++) {
		memset(&t, 0, sizeof(t));
		if (sscanf(argv[i], "%d-%d-%d %d:%d:%d",
		    &t.tm_year,
		    &t.tm_mon,
		    &t.tm_mday,
		    &t.tm_hour,
		    &t.tm_min,
		    &t.tm_sec) < 3) {
			fprintf(stderr, "%s: Unable to parse date [%s] as YYYY-MM-DD [hh:[mm:[ss]]]", argv0, argv[i]);
			exit(4);
		}
		t.tm_year -= 1900;
		t.tm_mon--;
		if ((finfo[i].time = mktime(&t)) == (time_t)-1) {
			fprintf(stderr, "%s: Invalid time specification: %s", argv0, argv[i]);
			exit(4);
		}
		finfo[i].name = xstrdup(argv[i]);
		finfo[i].deleted = finfo[i].todelete = 0;
	}
}

/*
 * Create the pruning schedule in the schedule array.
 * This contains the day numbers of each interval that
 * should have a file retained.
 */
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
			/*
			 * Divide the total area for days by the one-day area:
			 * {int from 0 to days normal} over
			 * {int from 0 to 1 normal}
			 * where int from 0 to x == D(x) - 0.5
			 */
			depth = (int)((D(days) - .5) / (D(1) - .5));
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
			if (D(current) - D(start) > area ||
			    /* Avoid torture at the functions's end */
			    (n == depth - 1 && current - start > 2 * diff) ||
			    current - start > 10 * diff ||
			    current > 365000) {
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

/* Print (rather than execute) the calculated schedule */
static void
print_schedule(void)
{
	int i;

	for (i = 0; i < depth; i++)
		printf("%d\n", schedule[i]);
}

/* qsort comparison function */
static int
bytime(const void *a, const void *b)
{
	return (((const struct s_finfo *)b)->time - ((const struct s_finfo *)a)->time);
}

/*
 * Try to prune a file - as specified
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
 * Given the theoretical pruning schedule and the actual files
 * perform the pruning operation.
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
		for (fi = nfiles - 1; fi >= 0; fi--) {
			fprintf(stderr, "name[%s]=%d days=%ld now=%ld limit=%ld\n", finfo[fi].name, (int)finfo[fi].time, days, (int)now,  (int)limit);
			if (!finfo[fi].todelete && finfo[fi].time < limit)
				prunefile(&finfo[fi]);
		}
	}
	if (opt_print_keep || opt_print_del)
		for (fi = 0; fi < nfiles; fi++)
			if ((opt_print_del && finfo[fi].deleted) ||
			    (opt_print_keep && !finfo[fi].deleted))
				printf("%s\n", finfo[fi].name);
}
