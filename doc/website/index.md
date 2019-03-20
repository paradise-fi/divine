DIVINE is a modern, explicit-state **model checker**. Based on the **LLVM**
toolchain, it can **verify programs** written in multiple real-world
programming languages, including **C** and **C++**.  The verification core is
built on a foundation of **high­-per­for­mance algorithms** and data
structures, scaling all the way from a laptop to a high-end cluster.  Learn
more in the [manual](manual.html). Our plans for upcoming releases are outlined
in the [roadmap] [rmap].

# DIVINE 4.2 Released

[2019-01-15] A year since last major release, we are pleased to announce that
DIVINE version 4.2 has been released. Like all 4.x releases, the changes
included in 4.2 were integrated gradually. The 4.1.x series, culminating in the
release of 4.2 today has brought substantially improved POSIX compatibility, to
a level where packages like `gzip` or `coreutils` can be configured and
compiled using `divcc`, which creates binaries which can be verified in divine
and also executed in production. In this case, the executable binary code is
derived from the exact same bitcode that is processed by the model checker.

Other highlights include much faster bitcode loading, improved support for
relaxed memory models used by Intel `x86_64`-series processors, support for C11
threads, verification of synchronous systems specified using C with LTL
properties, much faster and more robust symbolic verification mode. Basic
support for detection of memory leaks has also been added, along with a large
number of smaller improvements and fixes detailed in the [release notes]
[rnotes].

Previously: [more news] [news].

# Use in Publications

When you refer to DIVINE in an academic paper, we would appreciate if you could
use the following reference (our currently most up-to-date tool paper):

    @InProceedings{BBK+17,
      author =    {Zuzana Baranová and Jiří Barnat and Katarína Kejstová and
                   Tadeáš Kučera and Henrich Lauko and Jan Mrázek and Petr Ročkai
                   and Vladimír Štill},
      title =     {Model Checking of {C} and {C}++ with {DIVINE} 4},
      booktitle = {Automated Technology for Verification and Analysis},
      pages =     {201-207},
      volume =    10482,
      series =    {LNCS},
      year =      2017,
      publisher = {Springer},
      doi =       {10.1007/978-3-319-68167-2_14}
    }

# Contacting Us

If you have comments or questions about DIVINE, please send an email to divine
at fi.muni.cz.

[rmap]: roadmap.html
[rnotes]: whatsnew.html
[dl]: download.html
[news]: previously.html
