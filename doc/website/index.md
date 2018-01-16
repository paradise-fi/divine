DIVINE is a modern, explicit-state **model checker**. Based on the **LLVM**
toolchain, it can **verify programs** written in multiple real-world
programming languages, including **C** and **C++**.  The verification core is
built on a foundation of **high­-per­for­mance algorithms** and data
structures, scaling all the way from a laptop to a high-end cluster.  Learn
more in the [manual](manual.html). Our plans for upcoming releases are outlined
in the [roadmap] [rmap].

# DIVINE 4.1 Released

[2018-01-15] Version 4.1 of the DIVINE model checker has been released. Since
version 4.0 was released a year ago, a large number of improvements has made
its way into the tool -- however, there are only minor differences between the
last 4.0 point release and the first version in the new 4.1 series. The changes
that happened between the 4.0 release in January 2017 and 4.1 are summarised in
our [release notes] [rnotes].

Likewise, we plan to implement many improvements over the lifetime of the 4.1
series. The main points are described in our [roadmap] [rmap], including a
much-improved **symbolic verification** mode, a new **compiler** binary that
can be used as a drop-in replacement for `gcc` or `clang` when building complex
projects, and a **`valgrind`-like** mode for executing programs in a
semi-native environment.

Previously: [more news] [news].

# Use in Publications

When you refer to DIVINE in an academic paper, we would appreciate if you could
use the following reference (our currently most up-to-date tool paper):

    @InProceedings{BBK+17,
      author = {Zuzana Baranová and Jiří Barnat and Katarína Kejstová
                and Tadeáš Kučera and Henrich Lauko and Jan Mrázek
                and Petr Ročkai and Vladimír Štill},
      title = {Model Checking of {C} and {C}++ with {DIVINE} 4},
      booktitle = {Automated Technology for Verification and Analysis (ATVA 2017)},
      pages     = {201-207},
      volume = {10482},
      series = {LNCS},
      year = {2017},
      publisher = {Springer}}

# Contacting Us

If you have comments or questions about DIVINE, please send an email to divine
at fi.muni.cz.

[rmap]: roadmap.html
[rnotes]: whatsnew.html
[dl]: download.html
[news]: previously.html
