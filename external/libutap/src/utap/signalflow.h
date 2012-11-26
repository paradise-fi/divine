// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-

/* libutap - Uppaal Timed Automata Parser.
   Copyright (C) 2002-2003 Uppsala University and Aalborg University.
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA
*/

#ifndef UTAP_SIGNALFLOW_HH
#define UTAP_SIGNALFLOW_HH

#include "utap/system.h"
#include "utap/statement.h"

#include <list>
#include <set>
#include <map>
#include <stack>

namespace UTAP
{
    /**
     * Class SignalFlow is for analysing UPPAAL specifications and
     * extracting the timed automata input/output "interface"
     * information which can be treated as a data flow or
     * entity-relationship map of the system.
     *
     * The result can be processed by the dot (graphviz.org) to
     * produce a "human-readable" picture.  The other (tron) format is
     * used in TRON project.  Feel free to add more "formats" and/or
     * tune the dot output.
     *
     * The system must be built by TypeChecker/SystemBuilder before
     * SignalFlow.  Simply create using constructor and then use
     * print* methods.  The rest of methods are used internally by
     * visitor pattern.  Feel free to add new print* methods or
     * inheriting classes.
     *
     * Author: Marius Mikucionis <marius@cs.aau.dk>
     */
    class SignalFlow: public StatementVisitor
    {
    public:
        struct less_str {// must be somewhere in utilities, replace if found
            bool operator() (const char* s1, const char* s2) const {
                return (strcmp(s1,s2)<0);
            }
        };
        typedef std::set<const char*, const less_str> strs_t;// string set
        struct proc_t; // info on process input/output
        typedef std::map<const proc_t*, strs_t> proc2strs_t;//proc->string set
        typedef std::map<const char*, strs_t> str2strs_t;//string->string set

        typedef struct proc_t { // info about process input/output
            const char* name; // name of the process
            strs_t inChans, outChans; // input/output channels used by process
            str2strs_t rdVars, wtVars; // read/written variables with channels
            proc2strs_t outEdges; // outChans indexed by destination process
            proc_t(const char* _name): name(_name) {}
        };
        typedef std::set<proc_t*> procs_t;
        typedef std::map<const char*, procs_t, less_str> str2procs_t;

    protected:
        int verbosity;//0 - silent, 1 - errors, 2 - warnings, 3 - diagnostics
        const char* title; // title of the Uppaal TA system
        procs_t procs; // list of all processes in the system
        str2procs_t receivers, transmiters;// processes sorted by vars/chans
        strs_t processes, channels, variables;
        proc_t* cTA; // current automaton in traversal
        instance_t* cP; // current process in traversal
        const char* cChan; // channel on current transition in traversal
        bool inp, out, sync, paramsExpanded;// current expression state
        std::stack<std::pair<bool, bool> > ioStack;// remember I/O state

        bool checkParams(const symbol_t &s);// maps parameter to global symbol
        void addChan(const symbol_t &, strs_t &, str2procs_t&);
        void addVar(const symbol_t &, str2strs_t&, str2procs_t&);
        void visitProcess(instance_t &);
        void visitExpression(const expression_t &);
        void pushIO(){
            ioStack.push(std::make_pair<bool, bool>(inp, out));
        }
        void popIO() {
            inp = ioStack.top().first;
            out = ioStack.top().second;
            ioStack.pop();
        }

    public:
/**
 * Analyse the system and extract I/O information:
 */
        SignalFlow(const char* _title, TimedAutomataSystem& ta);

        void setVerbose(int verbose) { verbosity = verbose; }
/**
 * All strings are from TASystem (don't dispose TASystem before SignalFlow).
 */
        virtual ~SignalFlow();

/**
 * Print I/O information in TRON format into given output stream.
 */
        void printForTron(std::ostream &os);

/**
 * Print I/O information in DOT format into given output stream.
 * ranked -- puts oposite "ranks" on variables and channels
 * erd -- puts boxes and diamonds rather than (compact) ellipses.
 * cEdged -- channels are moved on edges rather than separate nodes.
 */
        void printForDot(std::ostream &os, bool ranked, bool erd, bool cEdged);

/**
 * System visitor pattern extracts read/write information from UCode.
 * This is actually "const" visitor and should contain "const Statement *stat".
 * Not tested (contains sample implementation but tested only on v3.4 specs).
 */
        int32_t visitEmptyStatement(EmptyStatement *stat);
        int32_t visitExprStatement(ExprStatement *stat);
        int32_t visitForStatement(ForStatement *stat);
        int32_t visitIterationStatement(IterationStatement *stat);
        int32_t visitWhileStatement(WhileStatement *stat);
        int32_t visitDoWhileStatement(DoWhileStatement *stat);
        int32_t visitBlockStatement(BlockStatement *stat);
        int32_t visitSwitchStatement(SwitchStatement *stat);
        int32_t visitCaseStatement(CaseStatement *stat);
        int32_t visitDefaultStatement(DefaultStatement *stat);
        int32_t visitIfStatement(IfStatement *stat);
        int32_t visitBreakStatement(BreakStatement *stat);
        int32_t visitContinueStatement(ContinueStatement *stat);
        int32_t visitReturnStatement(ReturnStatement *stat);
        int32_t visitAssertStatement(UTAP::AssertStatement *stat);
    };

/**
 * print -- template for pretty printing lists.
 * similar to ostream_iterator except it does not print last delimiter.
 */
    template <class T>
    struct print: public std::unary_function<T, void>
    {
        std::ostream& os;
        const char *infix;
        bool need;
        print(std::ostream& out, const char* sep):
            os(out), infix(sep), need(false) {}
        void operator()(const T& x)
            {
                if (need) os << infix;
                os << x; need = true;
            }
    };

    inline std::ostream& operator<<(std::ostream& os, const SignalFlow::strs_t& s)
    {
        for_each(s.begin(), s.end(), print<const char*>(os, ", "));
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const SignalFlow::procs_t& ps)
    {
        SignalFlow::procs_t::const_iterator p=ps.begin(), e=ps.end();
        if (p!=e) { os << (*p)->name; ++p; }
	while (p!=e) { os << ", " << (*p)->name; ++p; }
        return os;
    }

/**
 * Partitions the system into environment and IUT according to TRON
 * assumptions. inputs/outputs are channel names. Environment processes shout
 * on inputs and listens to outputs, while IUT processes shout on outputs and
 * listen to inputs. There are additional rules to complete the partitioning
 * (to cathegorize the internal processes):
 * 1) channels, that are not declared as inputs/outputs, are non-observable,
 *    called internal.
 * 2) internal channel belongs to environment (IUT) if it is used by
 *    environment (IUT) process (respectively). Model is inconsistent and
 *    cannot be partitioned if the internal channel is used by both environment
 *    and IUT.
 * 3) process belongs to environment (IUT) if it uses the internal environment
 *    (IUT) channel (respectively).
 * 4) variable belongs to environment (IUT) if it is accessed by environment
 *    (IUT) process without observable input/output channel synchronization.
 *    Variable is not cathegorized (can be either) if accessed consistently
 *    only during observable input/output channel synchronization.
 * 5) process belongs to environment (IUT) if accesses environment (IUT)
 *    variable (respectively) without observable channel synchronization.
 * Returns:
 *  0 if partitioning was consistent and complete,
 *  1 if partitioning was consistent but incomplete (some proc/chan is free)
 *  2 if partitioning was inconsistent (some proc/chan/var is both Env and IUT)
 */
    class Partitioner: public SignalFlow
    {
    protected:
        procs_t procsEnv, procsIUT, procsBad;
        strs_t chansIntEnv, chansIntIUT, observable, chansBad;
        strs_t varsEnv, varsIUT, varsBad;
        strs_t chansInp, chansOut;
        const char* rule;

        void addProcs(const strs_t& chans, const str2procs_t& index,
                      procs_t& result, procs_t& exclude);
        void addIntChans(const procs_t& procs,
                         strs_t& result, strs_t& exclude);
        void addIntVars(const procs_t& procs, strs_t& result,
                        strs_t& exclude);
        void addProcsByVars(const strs_t& vars, procs_t& procs,
                            procs_t& exclude);
    public:
        Partitioner(const char* _title, TimedAutomataSystem& ta):
            SignalFlow(_title, ta) {}
        ~Partitioner();

        int partition(const strs_t& inputs, const strs_t& outputs);
        int partition(std::istream& ioinfo);
        void printForDot(std::ostream &os, bool ranked, bool erd, bool cEdged);
        void printViolation(const proc_t* process, const char* variable);
        void fillWithEnvProcs(strs_t& procs);
        void fillWithIUTProcs(strs_t& procs);
    };
}

#endif
