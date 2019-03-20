# 2018

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

# 2017

[2017-01-08] DIVINE 4 is now available. We are pleased to announce that, after
a few last-minute holdups, we are ready to label DIVINE 4 as stable. Please
bear in mind, however, that DIVINE is still an academic tool and unfortunately
only saw limited testing in the beta period. That said, our experience says
that it's already pretty useful in practice and we have some nice improvements
planned for [future releases] [rmap].

The latest source tarball for DIVINE 4 is, as
always, [available for download] [dl]. Starting on January 15th, there will be
semi-automated, fortnightly point releases in the 4.0 series (which will
include new features as they stabilise), and a 4.1 milestone in July this year.

# 2016

[2016-09-23] A major update of our software model checker is scheduled for
release in autumn 2016. The new release will bring a new, flexible execution
environment, based on the traditional virtual machine -- operating system --
user program division, along with a more efficient multi-threaded verification
core.  The user interface has also seen major improvements. Finally, the
upcoming version of DIVINE will make working with counterexamples a much
smoother and more pleasant experience.

[rmap]: roadmap.html
[dl]: download.html
