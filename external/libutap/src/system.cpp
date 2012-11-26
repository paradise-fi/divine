// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-

/* libutap - Uppaal Timed Automata Parser.
   Copyright (C) 2002-2006 Uppsala University and Aalborg University.

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

#include <stack>
#include <algorithm>
#include <cstdio>
#include <climits>
#include <cassert>
#include <sstream>

#include <boost/bind.hpp>

#include "utap/builder.h"
#include "utap/system.h"
#include "utap/statement.h"
#include "libparser.h"

using namespace UTAP;
using namespace Constants;

using boost::bind;
using std::list;
using std::stack;
using std::vector;
using std::map;
using std::pair;
using std::make_pair;
using std::min;
using std::max;
using std::set;
using std::string;
using std::ostream;

static const char *const unsupported
= "Internal error: Feature not supported in this mode.";
static const char *const invalid_type = "Invalid type";

function_t::~function_t()
{
    delete body;
}

bool declarations_t::addFunction(type_t type, string name, function_t *&fun)
{
    bool duplicate = frame.getIndexOf(name) != -1;
    functions.push_back(function_t());
    fun = &functions.back();
    fun->uid = frame.addSymbol(name, type, fun); // Add symbol
    return !duplicate;
}

state_t &template_t::addLocation(string name, expression_t inv)
{
    bool duplicate = frame.getIndexOf(name) != -1;

    states.push_back(state_t());
    state_t &state = states.back();
    state.uid = frame.addSymbol(name, type_t::createPrimitive(LOCATION), &state);
    state.locNr = states.size() - 1;
    state.invariant = inv;

    if (duplicate)
    {
        throw TypeException(boost::format("Duplicate definition of %1%") % name);
    }

    return state;
}

edge_t &template_t::addEdge(symbol_t src, symbol_t dst, bool control)
{
    int32_t nr = edges.empty() ? 0 : edges.back().nr + 1;
    edges.push_back(edge_t());
    edges.back().src = static_cast<state_t*>(src.getData());
    edges.back().dst = static_cast<state_t*>(dst.getData());
    edges.back().control = control;
    edges.back().nr = nr;
    return edges.back();
}

TimedAutomataSystem::TimedAutomataSystem()
{
    global.frame = frame_t::createFrame();
    addVariable(&global, type_t::createPrimitive(CLOCK), "t(0)", expression_t());
#ifdef ENABLE_CORA
    addVariable(&global, type_t::createPrimitive(COST), "cost", expression_t());
#endif

    hasPriorities = false;
    defaultChanPriority = 0;
}

TimedAutomataSystem::~TimedAutomataSystem()
{

}

list<template_t> &TimedAutomataSystem::getTemplates()
{
    return templates;
}

list<instance_t> &TimedAutomataSystem::getProcesses()
{
    return processes;
}

declarations_t &TimedAutomataSystem::getGlobals()
{
    return global;
}

/** Creates and returns a new template. The template is created with
 *  the given name and parameters and added to the global frame. The
 *  method does not check for duplicate declarations. An instance with
 *  the same name and parameters is added as well.
 */
template_t &TimedAutomataSystem::addTemplate(string name, frame_t params)
{
    type_t type = type_t::createInstance(params);

    templates.push_back(template_t());
    template_t &templ = templates.back();
    templ.parameters = params;
    templ.frame = frame_t::createFrame(global.frame);
    templ.frame.add(params);
    templ.templ = &templ;
    templ.uid = global.frame.addSymbol(name, type, (instance_t*)&templ);
    templ.arguments = 0;
    templ.unbound = params.getSize();

    return templ;
}

instance_t &TimedAutomataSystem::addInstance(
    string name, instance_t &inst, frame_t params,
    const vector<expression_t> &arguments)
{
    type_t type = type_t::createInstance(params);

    instances.push_back(instance_t());
    instance_t &instance = instances.back();
    instance.uid = global.frame.addSymbol(name, type, &instance);
    instance.unbound = params.getSize();
    instance.parameters = params;
    instance.parameters.add(inst.parameters);
    instance.mapping = inst.mapping;
    instance.arguments = arguments.size();
    instance.templ = inst.templ;

    for (size_t i = 0; i < arguments.size(); i++)
    {
        instance.mapping[inst.parameters[i]] = arguments[i];
    }

    return instance;
}

void TimedAutomataSystem::addProcess(instance_t &instance)
{
    type_t type;
    processes.push_back(instance);
    instance_t &process = processes.back();
    if (process.unbound == 0)
    {
        type = type_t::createProcess(process.templ->frame);
    }
    else
    {
        type = type_t::createProcessSet(instance.uid.getType());
    }
    process.uid = global.frame.addSymbol(
        instance.uid.getName(), type, &process);
}

// Add a regular variable
variable_t *TimedAutomataSystem::addVariable(
    declarations_t *context, type_t type, string name, expression_t initial)
{
    variable_t *var;
    var = addVariable(context->variables, context->frame, type, name);
    var->expr = initial;
    return var;
}

variable_t *TimedAutomataSystem::addVariableToFunction(
    function_t *function, frame_t frame, type_t type, string name,
    expression_t initial)
{
    variable_t *var;
    var = addVariable(function->variables, frame, type, name);
    var->expr = initial;
    return var;
}

// Add a regular variable
variable_t *TimedAutomataSystem::addVariable(
    list<variable_t> &variables, frame_t frame, type_t type, string name)
{
    bool duplicate = frame.getIndexOf(name) != -1;
    variable_t *var;

    // Add variable
    variables.push_back(variable_t());
    var = &variables.back();

    // Add symbol
    var->uid = frame.addSymbol(name, type, var);

    if (duplicate)
    {
        throw TypeException(boost::format("Duplicate definition of identifier %1%") % name);
    }

    return var;
}

void TimedAutomataSystem::addProgressMeasure(
    declarations_t *context, expression_t guard, expression_t measure)
{
    progress_t p;
    p.guard = guard;
    p.measure = measure;
    context->progress.push_back(p);
}

static void visit(SystemVisitor &visitor, frame_t frame)
{
    for (size_t i = 0; i < frame.getSize(); i++)
    {
        type_t type = frame[i].getType();

        if (type.getKind() == TYPEDEF)
        {
            visitor.visitTypeDef(frame[i]);
            continue;
        }

        void *data = frame[i].getData();
        type = type.stripArray();

        if ((type.is(Constants::INT)
             || type.is(Constants::BOOL)
             || type.is(CLOCK)
             || type.is(CHANNEL)
             || type.is(SCALAR)
             || type.getKind() == RECORD)
            && data != NULL) // <--- ignore parameters
        {
            visitor.visitVariable(*static_cast<variable_t*>(data));
        }
        else if (type.is(LOCATION))
        {
            visitor.visitState(*static_cast<state_t*>(data));
        }
        else if (type.is(FUNCTION))
        {
            visitor.visitFunction(*static_cast<function_t*>(data));
        }
    }
}

void TimedAutomataSystem::accept(SystemVisitor &visitor)
{
    visitor.visitSystemBefore(this);
    visit(visitor, global.frame);

    list<template_t>::iterator t;
    for (t = templates.begin(); t != templates.end();t++)
    {
        if (visitor.visitTemplateBefore(*t))
        {
            visit(visitor, t->frame);

            for_each(t->edges.begin(), t->edges.end(),
                     bind(&SystemVisitor::visitEdge, &visitor, _1));

            visitor.visitTemplateAfter(*t);
        }
    }

    for (size_t i = 0; i < global.frame.getSize(); i++)
    {
        type_t type = global.frame[i].getType();
        void *data = global.frame[i].getData();
        type = type.stripArray();

        if (type.is(PROCESS) || type.is(PROCESSSET))
        {
            visitor.visitProcess(*static_cast<instance_t*>(data));
        }
        else if (type.is(INSTANCE))
        {
            visitor.visitInstance(*static_cast<instance_t*>(data));
        }
    }

    visitor.visitSystemAfter(this);
}

void TimedAutomataSystem::setBeforeUpdate(expression_t e)
{
    beforeUpdate = e;
}

expression_t TimedAutomataSystem::getBeforeUpdate()
{
    return beforeUpdate;
}

void TimedAutomataSystem::setAfterUpdate(expression_t e)
{
    afterUpdate = e;
}

expression_t TimedAutomataSystem::getAfterUpdate()
{
    return afterUpdate;
}

void TimedAutomataSystem::setChanPriority(expression_t chan, int priority)
{
    hasPriorities |= (priority != 0);
    chan_priority_t chanPriority;
    chanPriority.chanElement = chan;
    chanPriority.chanPriority = priority;
    chanPriorities.push_back(chanPriority);
}

const std::list<chan_priority_t>& TimedAutomataSystem::getChanPriorities() const
{
    return chanPriorities;
}

std::list<chan_priority_t>& TimedAutomataSystem::getMutableChanPriorities()
{
    return chanPriorities;
}

void TimedAutomataSystem::setDefaultChanPriority(int priority)
{
    hasPriorities |= (priority != 0);
    defaultChanPriority = priority;
}

int TimedAutomataSystem::getTauPriority() const
{
    return defaultChanPriority;
}

void TimedAutomataSystem::setProcPriority(const char* name, int priority)
{
    hasPriorities |= (priority != 0);
    procPriority[name] = priority;
}

int TimedAutomataSystem::getProcPriority(const char* name) const
{
    return procPriority.find(name)->second;
}

bool TimedAutomataSystem::hasPriorityDeclaration() const
{
    return hasPriorities;
}

void TimedAutomataSystem::addPosition(
    uint32_t position, uint32_t offset, uint32_t line, std::string path)
{
    positions.add(position, offset, line, path);
}

const Positions::line_t &TimedAutomataSystem::findPosition(uint32_t position) const
{
    return positions.find(position);
}

void TimedAutomataSystem::addError(position_t position, std::string msg)
{
    errors.push_back(error_t(positions.find(position.start),
                             positions.find(position.end),
                             position, msg));
}

void TimedAutomataSystem::addWarning(position_t position, std::string msg)
{
    warnings.push_back(error_t(positions.find(position.start),
                               positions.find(position.end),
                               position, msg));
}

// Returns the errors
const vector<UTAP::error_t> &TimedAutomataSystem::getErrors() const
{
    return errors;
}

// Returns the warnings
const vector<UTAP::error_t> &TimedAutomataSystem::getWarnings() const
{
    return warnings;
}

bool TimedAutomataSystem::hasErrors() const
{
    return !errors.empty();
}

bool TimedAutomataSystem::hasWarnings() const
{
    return !warnings.empty();
}

void TimedAutomataSystem::clearErrors()
{
    errors.clear();
}

void TimedAutomataSystem::clearWarnings()
{
    warnings.clear();
}


