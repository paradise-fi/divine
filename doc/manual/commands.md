---
author: Petr Ročkai
title: DIVINE
section: 1
header: The DIVINE Model Checker
date: 2017-09-19
...

# Commandline Interface

The main interface to DIVINE is the command-line tool simply called `divine`.
Basic information about the binary can be obtained by issuing `divine
--version`:

    version: 4.0.12+8fbbb7005640
    source sha: 37282b999cc10a027cabbb9669365f9e630fb685
    runtime sha: b8939c99ca81161cbe69076e31073807f69dda72
    build date: 2017-09-19, 14:02 UTC
    build type: Debug

## Synopsis

    divine cc [compiler flags] <sources>
    divine verify [...] <input file>
    divine draw [...] <input file>
    divine run [...] <input file>
    divine sim [...] <input file>


## Input Options

All commands that work with inputs share a number of flags that influence the
input program.

    divine {...} [-D {string}]
                 [--autotrace {tracepoint}]
                 [--sequential]
                 [--disable-static-reduction]
                 [--relaxed-memory {string}]
                 [--lart {string}]

## State Space Visualisation & Simulation

    divine draw [--distance {int}]
                [--render {string}]
                {input options}

To visualise (a part of) the state space, you can use `divine draw`, which
creates a graphviz-compatible representation. By default, it will run "`dot
-Tx11`" to display the resulting graph. You can override the drawing command to
run by using the `--render` switch. The command will get the dot-formatted
graph description on its standard input. Out of common input options, the
`--autotrace` option is often quite useful with `draw`.

    divine sim [--batch] [--load-report {file}] [--skip-init]
               {input options}

The `sim` sub-command is used to interactively explore a program using a
terminal-based interface. Use `help` in the interactive prompt to obtain help on
available commands. See also [Interactive Debugging](#sim).

## Model Checking

    divine {check|verify} [model checking options]
                          [exploration options]
                          {input options}

These two commands are the main workhorse of model checking. The `verify`
command performs full model checking under conservative assumptions. The
`check` command is more liberal, and will assume, for instance, that `malloc`
will not fail. While `check` is likely to cover most scenarios, it omits cases
that are either very expensive to check or that appear in programs very often
and make the tool cumbersome to use.

The algorithms DIVINE uses are often resource-intensive and some are parallel
(multi-threaded). The `verify` and `check` commands will, in common cases, use
a parallel algorithm to explore the state space of the system. By default,
parallel algorithms will use available cores, 4 at most. A few switches control
resource use:

    divine {...} [--threads {int}]
                 [--max-memory {mem}]
                 [--max-time {int}]

`--threads {int} | -T {int}`
:    The number of threads to use for verification. The default is 4 or the number
     of cores if less than 4. For optimal performance, each thread should get
     one otherwise mostly idle CPU core. Your mileage may vary with
     hyper-threading (it is best to run a few benchmarks on your system to find
     the best configuration).

`--max-memory {mem}`
:    Limit the amount of memory `divine` is allowed to allocate. This is mainly
     useful to limit swapping. When the verification exceeds available RAM, it
     will usually take extremely long time to finish and put a lot of strain
     on the IO subsystem. It is recommended that you do not allow `divine` to
     swap excessively, either using this option or by some other means.

`--max-time {int}`
:    Put a limit of `{int}` seconds on the maximal running time.

Verification results can be written in a few forms, and resource use can also
be logged for benchmarking purposes:

    divine {...} [--report {fmt}]
                 [--no-report-file]
                 [--report-filename {string}]
                 [--num-callers {int}]

`--report {fmt}`
:   At the end of a verification run, produce a comprehensive, machine-readable
    report. Currently the available formats are: `none` -- disables the report
    entirely, `yaml` -- prints a concise, yaml-fomatted summary of the results
    (without memory statistics or machine-readable counterexample data) and
    `yaml-long` which prints everything as yaml.

`--no-report-file`
:   By default, the long-form verification results (equivalent to `--report
    yaml-long`) are stored in a file. This switch suppresses that behaviour.

`--report-filename {string}`
:   Store the long-form verification results in a file with the given name. If
    this option is not used, a unique filename is derived from the name of the
    input file.
