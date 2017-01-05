DIVINE is a modern, explicit-state **model checker**. Based on the **LLVM**
toolchain, it can **verify programs** written in multiple real-world
programming languages, including **C** and **C++**.  The verification core is
built on a foundation of **high­-per­for­mance algorithms** and data
structures, scaling all the way from a laptop to a high-end cluster.  Learn
more in the **[manual](manual.html)**. Our plans for upcoming releases are
outlined in the [roadmap](roadmap.html).

Second Beta of DIVINE 4 is Available
====================================

[2016-12-14] We are pleased to announce that the second beta version of the
upcoming DIVINE 4.0 is now [available for download] [1] as a source tarball.

[1]: download.html

DIVINE 4 Coming This Autumn
===========================

[2016-09-23] A major update of our software model checker is scheduled for
release in autumn 2016. The new release will bring a new, flexible execution
environment, based on the traditional virtual machine -- operating system -- user
pro­gram division, along with a more efficient multi-threaded verification core.
The user interface has also seen major improvements. Finally, the upcoming
version of DIVINE will make working with counterexamples a much smoother and
more pleasant experience.

Contacting Us
=============

If you have comments or questions about DIVINE, please send an email to divine
at fi.muni.cz.

Use in Publications
===================

When you refer to DIVINE in an academic paper, we would appreciate if you could
use the following reference (our currently most up-to-date tool paper):

    @InProceedings{BBH+13,
      author    = "Jiří Barnat and Luboš Brim and Vojtěch Havel
                   and Jan Havlíček and Jan Kriho and Milan Lenčo
                   and Petr Ročkai and Vladimír Štill and Jiří Weiser",
      title     = "{DiVinE 3.0 -- An Explicit-State Model Checker
                    for Multithreaded C \& C++ Programs}",
      booktitle = "{Computer Aided Verification (CAV 2013)}",
      pages     = "863--868",
      volume    = "8044",
      series    = "LNCS",
      year      = "2013",
      publisher = "Springer"}
