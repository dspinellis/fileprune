.\" $Id: \\dds\\src\\sysutil\\fileprune\\RCS\\dds-prune.ms,v 1.2 2002/12/23 16:25:07 dds Exp $
.\" login@usenix.org

.TL
Organized Pruning of File Sets
.AU
Diomidis D. Spinellis
.AI
<dds@aueb.gr>

.PP
In a number of backup scenarios backup files simply accrue in a directory
that should be periodically cleaned up.
Typical examples include backup data from 
databases,
PDAs, 
network clients, and
routers.
The data contents of the above are often not directly backed-up onto tapes but,
to disk-based files using a \fIcron(8)\fP job.
The continous increases in disk capacities in many cases allow us to keep
multiple sets of these backup files in a given directory as a substitute for a
regular tape backup.
These files are typically kept in a managable size by periodically retaining
all old files.
Well-organized tape-based backups offer however an additional advantage:
through a carefully staged tape retention schedule users can ofen retrieve
files much older than the number of retained tapes.
A tape backup schedule may for example involve daily incremental backups,
weekly full backups retained for two months, and one monthly tape retained
for two years.
In the above situation if I discover today that sometime in the previous
six months I deleted a file I had created a year ago, I can go to the
retained monthly backups  following the file's creation and retrieve it
from there.
.PP
Backups to files are not often organized in this manner.
Two approaches I have seen
involve either naming each file with a periodically repeated
date element, such as the day of the week or month, so that newer
files will overwrite older ones,
or tagging each file with a unique identifier,
such as the complete date,
and having a separate script remove files older than a given date.
Both approaches however, lack the property of selectively
retaining a subset of older files.
More elaborate schemes can of course be constructed by carefully
syncronizing and staging separate \fIcron(8)\fP jobs,
but I have never seen them applied in practice.
The problem of selectively retaining old files gets especially difficult
when the backups are created at irregular intervals e.g. each time
you synchronize your PDA or remember to backup your cellular phone directory.
.PP
On the other hand, a file-based backup scheme offers us the ability
to examine all the retained files and selectively prune those we decide
are not worth keeping.
The key concept for deciding which files to keep is the retention schedule.
In tape-based schemes this simply revolves around weeks, months, and years.
If we have a tool for managing the file pruning we can be more creative
in selecting a retention schedule, and hopefully use one that will,
violating Murphy's Law, offer us an increased probability to recover that
old file we discovered missing.
.SH
Retention Schedules
.PP
When I first decided to work on this problem, I thought about an
\fIexponential\fP retention schedule.
I would like to keep yesterday's backup, a backup two days ago,
then backups aged 4, 8, 16, 32, and 64 days.  
With 10 files I could cover a period lasting more than a year.
The above schedule uses 2 as the schedule's base;
one could select any smaller
number to increase the number of retained files, or a larger one to
decrease them.
The idea behind this schedule is that recent backups are more valuable
than older ones.
Creeping featurism made me think of different possible schedules.
One other possibility is a Fibonacci schedule.
Here the retention sequence starts with 1, 1, and each subsequent 
term is the sum of the two previous ones: 3, 5, 8, 13, 21 34, 55.
At that point, I had to wonder which of the two schedules was better for 
securing my valuable data.
.PP
It turns out that none of the two is.
If we were to sample data recovery requests in a large data center
we would probably find that the age of the requested files would
follow the uniquitous bell-shaped \fInormal\fP or \fIGaussian\fP distribution.
The exact shape of the bell is determined by the \fIstandard deviation\fP
of the requested file ages;
this expresses the variation (in the same unit unit as we measure the file
ages) between the ages of different requested files.
Since recovery requests
from all users (appart from point-haired managers)
always refer to the past,
the shape is actually one half of a bell curve.
You can see the normal curve for a standard deviation of 200 in Figure 1.
.PSPIC normal.eps
The formula defining the normal curve is actually quite complex
.EQ
f(x) = 1 over { sqrt{2 pi } sigma } e sup {-x sup 2 over {2 sigma  sup 2}}
.EN
but once it is coded in a program, its application can be a breeze.
The curve represents the probability that a file of a given age
will be requested.
You can see that, as we are intuitevely expecting, as files age they are
less likely to be needed.
In order to distribute our archive files in a way that reflects this
diminishing probability distribution we need to define our retention interval
schedule so that the interval's length is proportional to the probability
of requiring a file within that interval.
This is represented by the area under the curve for the given interval;
for the mathematically inclined, the area for an interval from
\fIa\fP to \fIb\fP is given by the integral
.EQ
int from a to b f(x) dx
.EN
What we thus need to do is to divide the whole area under the curve into
a number of equally-sized parts, as many as the files we can afford to retain,
and then calculate the respective intervals.
.PP
.PSPIC distr.eps
Unfortunately, there is no formula that can give us the numbers we are looking
for, however, we can numerically integrate the normal function adjusting the
interval as we move back into time.
On modern machines, this computationally intensive work will typically take
less than a second.
You can see how we would spread 30 files in a period of around 2000 days
using an exponential distribution with a base of 1.3 and a normal distribution
with a standard deviation of 1000 in Figure 2.
For comparisson purposes I have also included how a Fibonacci distribution
and an exponential distribution with a base of 2
would appear in the above scheme; only 18 files would fit in the
Fibonacci distribution and 12 in the base-2 exponential.
.SH
The Prune Tool
.PP
Putting code where my mouth is, I wrote a C program to implement the
file pruning strategies described above.
It is available for download in source form through a BSD-style license from
http://www.spinellis.gr/sw/unix/prune.
\fIPrune\fP 
will delete files from the specified set targetting a given distribution
of the files within time,
while also supporting size, number, and age constraints.
Its main purpose is to keep a set of daily-created backup files
in managable size,
while still providing reasonable access to older versions.
Specifying a size, file number, or age constraint will
simply remove files starting from the oldest, until the
constraint is met.
The distribution specification (exponential, Gaussian (normal), or Fibonacci)
provides finer control of the files to delete,
allowing the retention of recent copies and the increasingly
agressive pruning of the older files.
The retention schedule specifies the age intervals for which files
will be retained.
As an example, an exponential retention schedule for 10 files
with a base of 2 will be:
.IP
1 2 4 8 16 32 64 128 256 512 1024
.PP
The above schedule specifies that for the interval of 65 to 128
days there should be (at least) one retained file (unless constraints
or other options override this setting).
Retention schedules are always calculated and evaluated in integer days.
By default \fIprune\fP will keep the oldest file within each day interval
allowing files to gradually migrate from one interval to the next
as time goes by.
It may also keep additional files, if the complete file set satisfies
the specified constraint.
The algorithm used for prunning does not assume that the files are
uniformally distributed;
\fIprune\fP will successfully prune files stored at irregular intervals.
.PP
\fIPrune\fP is invoked through the following syntax:
.br
\fBprune\fP 
[\fB\-n\fP|\fB\-N\fP|\fB\-p\fP]
[\fB\-c\fP \fIcount\fP|\fB\-s\fP \fIsize\fP[\fBk\fP|\fBm\fP|\fBg\fP|\fBt\fP]|\fB\-a\fP \fIage\fP[\fBw\fP|\fBm\fP|\fBy\fP]]
[\fB\-e\fP \fIbase\fP|\fB\-g\fP \fIstandard deviation\fP|\fB\-f\fP]
[\fB\-t\fP \fBa\fP|\fBm\fP|\fBc\fP]
[\fB\-FK\fP]
\fIfile\fR ...
.PP
The numerous options reflect the tool's flexibility.
You can specify the distribution to use (exponential, normal (Gaussian),
or Fibonacci) using the 
\fB\-e\fP, \fB\-g\fP, and \fB\-f\fP options as well
as the constraints for the number (count), size, or age of the
files to retain using the
\fB\-c\fP, \fB\-s\fP, and \fB\-a\fP options.
By default the constraints are used to specify the upper limit
of the size of number of files that will be retained.
If more files can be accommodated (because e.g. some intervals are
empty), or the specified size limit has not been reached, \fIprune\fP
will retain those files, deleting old files until the constraint is
satisfied.
The \fB-F\fP flag can be used to override this behaviour.
On the other hand, if a constraint is violated \fIprune\fP
may not retain any files in given interval;
the \fB-K\fP flag can be used to always keep at least one file
in each interval.
Finally, the \fB-t\fP flag allows you to specify whether \fIprune\fP
will use the creation, access, or modification time of the specified
files for determining their age.
.PP
The following examples illustrate some possible uses for \fIprune\fP:
.DS
.ft C
ssh remotehost tar cf - /datafiles >backup/`date +'%Y%m%d'`
prune -e 2 backup/*
.DE
Backup \fIremotehost\fP, storing the result in file file
named with today's timestamp (e.g. 20021219).
Prune the files in the backup directory
so that each retained file's age will be double that of its
immediately younger neighbour.
.DS
.ft C
prune -g 365 -c 30 *
.DE
Keep at most 30 files with their ages following a
Gaussian (normal) distribution with a standard deviation of one year.
.DS
.ft C
prune -e 2 -s 5G *
.DE
Prune the specified files following an 
exponential schedule so that no more than
5GB are occupied.
More than one file may be left in an interval,
if the size constraint is met.
Alternatively, some old intervals may be emptied in order
to satisfy the size constraint.
.DS
.ft C
prune -F -e 2 -s 5G *
.DE
As above, but leave no more than one file in each scheduled interval.
.DS
.ft C
prune -K -e 2 -s 5G *
.DE
As in the first example of the %g-constrained series,
but leave exactly one file in each interval,
even if this will violate the size constraint.
.DS
.ft C
prune -a 1m -f
.DE
Delete all files older than one month use;
use a Fibonacci distribution for pruning the remaining ones.
.SH
Conclusions
.PP
Increasing disk capacities and network bandwidth allow us to implement
disk-based backup mechanisms.
An important aspect of a disk-based backup system is the employed retention
schedule.
The prune tool allows you to rationally specify and automatically manage
the retention schedule to suit your needs.
An exponential schedule with an integer base or a Fibonacci-based schedule
can be easilly understood by unsophisticated users, while a schedule with
a normal distribution and an appropriately set standard deviation
is more likely to reflect your true file retention requirements.
