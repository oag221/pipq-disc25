# SprayList_Repo_Clone

This folder is a clone of the SprayList repository, cloned from
<https://github.com/jkopinsky/SprayList>.  It has been edited, as discussed
below.

## Documentation Changes

This README is not the original README.  See OldREADME.md for the original
documentation.

## Dependency Changes

The original `spraylist` required `gsl`.  In our containerized environment, this
means it will be necessary to run `apt install libgsl-dev`.

The original `spraylist` had optional NUMA support.  It is now mandatory.  This
means it will be necessary to run `apt install libnuma-dev`.

## Code Changes

We regularized the `Makefile`, so that it doesn't build everything every time it
is run.  This involved things like adding the `-MMD` flag.

We also renamed the files to have the `.cc` extension, and then used `g++`
instead of `gcc`.

We also turned on `-Werror`, and then edited the source files to fix all
warnings.  This also involved fixing C vs. C++ issues, like the use of `new` as
a variable name.

The original repository used the file `test.c` to produce a binary called
`spray`.  We now call that binary `test`.

Several `#if` guards were removed, for features that are always on, or always
off.

All headers are now in the `include` folder.  The `random.h` headers have been
merged.

`inline` functions were used to avoid excess function calls for skip list
wrapper functions.

The `ssalloc` infrastructure has been removed.  Linden does not use `ssalloc`,
so it is an unfair comparison.  Now everything uses `malloc`.  `jemalloc`
appears to resolve most of the performance issues that `ssalloc` hid.  

```bash
apt install libjemalloc-dev; LD_PRELOAD=libjemalloc.so ...
```

## Pending Changes

There are many platform-related constants that need to be reevaluated.  There is
currently a 128-thread maximum, a hard coding to 2 NUMA zones, a hard coding of
the threads per core and cores per CPU, etc.

The code in `measurements.cc` needs to be reviewed.  The timing seems to be tied
to a CPU Frequency, instead of using the OS.

The dependency on `libatomic_ops` should be removed.

Much of the code predates C++11, and isn't using `std::atomic`.  Some variables
are using the `VOLATILE` macro, which is defined as blank.  That's almost
certainly not correct.

The `sssp` benchmark uses a crude mechanism for determining when to terminate.
A proper termination detection barrier should be used instead.

The current priority queues are all using a custom allocator (`ssalloc`), which
allocates out of a shared slab.  We will probably want to use `jemalloc`
instead.

We should build a separate executable for each data structure, instead of
building one monolithic benchmark executable.

And, of course, we should integrate SkipVectorPQ into it.
