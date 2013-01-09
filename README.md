_Fileprune_
will delete files from the specified set targetting a given distribution
of the files within time as well as size, number, and age constraints.
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
with a base of 2 will be
```
1 2 4 8 16 32 64 128 256 512 1024
```
The above schedule specifies that for the interval of 65 to 128
days there should be (at least) one retained file (unless constraints
and options override this setting).
Retention schedules are always calculated and evaluated in integer days.
By default _fileprune_ will keep the oldest file within each day interval
allowing files to migrate from one interval to the next as time goes by.
It may also keep additional files, if the complete file set satisfies
the specified constraint.
The algorithm used for prunning does not assume that the files are
uniformally distributed;
_fileprune_ will successfully prune file collections stored at
irregular intervals.

# Project home
You can download the source and executables from the
[project's page](http://www.spinellis.gr/sw/unix/fileprune).
You can also read more in the article
[Organized pruning of file sets](http://www.spinellis.gr/pubs/trade/2003-login-prune/html/prune.html).  _;login:_, 28(3):39-42, June 2003.

# Building
* To build the program under Unix, Linux, Cygwin run ```make```
* To build the program under Microsoft C/C++ run ```nmake /f Makefile.mak```
