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

#include <vector>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cassert>
#include <inttypes.h>
#include <stdexcept>
#include <sstream>
#include <boost/tuple/tuple.hpp>

#include "utap/systembuilder.h"

using namespace UTAP;
using namespace Constants;

using std::vector;
using std::pair;
using std::make_pair;
using std::min;
using std::max;
using std::string;

SystemBuilder::SystemBuilder(TimedAutomataSystem *system)
    : StatementBuilder(system)
{
    currentEdge = NULL;
    currentChanPriority = 0;
    currentProcPriority = 0;
};

/************************************************************
 * Variable and function declarations
 */
variable_t *SystemBuilder::addVariable(type_t type, const char*  name,
                                        expression_t init)
{
    if (currentFun)
    {
        return system->addVariableToFunction(currentFun, frames.top(), type, name, init);
    }
    else
    {
        return system->addVariable(getCurrentDeclarationBlock(), type, name, init);
    }
}

bool SystemBuilder::addFunction(type_t type, const char* name)
{
    return getCurrentDeclarationBlock()->addFunction(type, name, currentFun);
}

declarations_t *SystemBuilder::getCurrentDeclarationBlock()
{
    return (currentTemplate ? currentTemplate : &system->getGlobals());
}

/************************************************************
 * Guarded progress measure
 */
void SystemBuilder::declProgress(bool hasGuard)
{
    expression_t guard, measure;
    measure = fragments[0];
    fragments.pop();
    if (hasGuard)
    {
        guard = fragments[0];
        fragments.pop();
    }
    system->addProgressMeasure(getCurrentDeclarationBlock(), guard, measure);
}

/********************************************************************
 * Process declarations
 */
void SystemBuilder::procBegin(const char* name)
{
    if (frames.top().getIndexOf(name) != -1)
    {
        handleError("Identifier defined multiple times");
    }
    currentTemplate = &system->addTemplate(name, params);
    pushFrame(currentTemplate->frame);
    params = frame_t::createFrame();
}

void SystemBuilder::procEnd() // 1 ProcBody
{
    currentTemplate = NULL;
    popFrame();
}

/**
 * Add a state to the current template. An invariant expression is
 * expected on and popped from the expression stack if \a hasInvariant
 * is true.
 */
void SystemBuilder::procState(const char* name, bool hasInvariant) // 1 expr
{
    expression_t e;
    if (hasInvariant)
    {
        e = fragments[0];
        fragments.pop();
    }
    currentTemplate->addLocation(name, e);
}

void SystemBuilder::procStateCommit(const char* name)
{
    symbol_t uid;
    if (!resolve(name, uid) || !uid.getType().isLocation())
    {
        handleError("Location expected");
    } 
    else if (uid.getType().is(URGENT)) 
    {
        handleError("States cannot be committed and urgent at the same time");
    }
    else
    {
        uid.setType(uid.getType().createPrefix(COMMITTED, position));
    }
}

void SystemBuilder::procStateUrgent(const char* name)
{
    symbol_t uid;

    if (!resolve(name, uid) || !uid.getType().isLocation())
    {
        handleError("Location expected");
    } 
    else if (uid.getType().is(COMMITTED)) 
    {
        handleError("States cannot be committed and urgent at the same time");
    }
    else
    {
        uid.setType(uid.getType().createPrefix(URGENT, position));
    }
}

void SystemBuilder::procStateInit(const char* name)
{
    symbol_t uid;
    if (!resolve(name, uid) || !uid.getType().isLocation())
    {
        handleError("Location expected");
    }
    else
    {
        currentTemplate->init = uid;
    }
}

void SystemBuilder::procEdgeBegin(const char* from, const char* to, const bool control)
{
    symbol_t fid, tid;

    if (!resolve(from, fid) || !fid.getType().isLocation())
    {
        handleError("No such location (source)");
    }
    else if (!resolve(to, tid) || !tid.getType().isLocation())
    {
        handleError("No such location (destination)");
    }
    else
    {
        currentEdge = &currentTemplate->addEdge(fid, tid, control);
        currentEdge->guard = makeConstant(1);
        currentEdge->assign = makeConstant(1);
        pushFrame(currentEdge->select = frame_t::createFrame(frames.top()));
    }
}

void SystemBuilder::procEdgeEnd(const char* from, const char* to)
{
    popFrame();
}

void SystemBuilder::procSelect(const char *id)
{
    type_t type = typeFragments[0];
    typeFragments.pop();

    if (!type.is(CONSTANT))
    {
        type = type.createPrefix(CONSTANT);
    }

    if (!type.isScalar() && !type.isInteger())
    {
        handleError("Scalar set or integer expected");
    } 
    else if (!type.is(RANGE))
    {
        handleError("Range expected");
    }
    else
    {
        currentEdge->select.addSymbol(id, type);
    }
}

void SystemBuilder::procGuard()
{
    currentEdge->guard = fragments[0];
    fragments.pop();
}

void SystemBuilder::procSync(synchronisation_t type)
{
    currentEdge->sync = expression_t::createSync(fragments[0], type, position);
    fragments.pop();
}

void SystemBuilder::procUpdate()
{
    currentEdge->assign = fragments[0];
    fragments.pop();
}

/********************************************************************
 * System declaration
 */

void SystemBuilder::instantiationBegin(
    const char* name, size_t parameters, const char* templ_name)
{
    /* Make sure this identifier is new.
     */
    if (frames.top().getIndexOf(name) != -1)
    {
        handleError("Identifier defined multiple times");
    }

    /* Lookup symbol.
     */
    symbol_t id;
    if (!resolve(templ_name, id) || id.getType().getKind() != INSTANCE)
    {
        handleError("Not a template");
    }

    /* Push parameters to frame stack.
     */
    frame_t frame = frame_t::createFrame(frames.top());
    frame.add(params);
    pushFrame(frame);
    params = frame_t::createFrame();
}

void SystemBuilder::instantiationEnd(
    const char *name, size_t parameters, const char *templ_name, size_t arguments)
{
    /* Parameters are at the top of the frame stack.
     */
    frame_t params = frames.top();
    popFrame();
    
    /* Lookup symbol. In case of failure, instantiationBegin already
     * reported the problem.
     */
    symbol_t id;
    if (resolve(templ_name, id) && id.getType().getKind() == INSTANCE) 
    {
        instance_t *old_instance = static_cast<instance_t*>(id.getData());

        /* Check number of arguments. If too many arguments, pop the
         * rest.
         */
        size_t expected = id.getType().size();
        if (arguments < expected)
        {
            handleError("Too few arguments");
        }
        else if (arguments > expected)
        {
            handleError("Too many arguments");
        }
        else
        {
            /* Collect arguments from expression stack.
             */
            vector<expression_t> exprs(arguments);
            while (arguments)
            {
                arguments--;
                exprs[arguments] = fragments[0];
                fragments.pop();
            }

            /* Create template composition.
             */
            instance_t &new_instance = 
                system->addInstance(name, *old_instance, params, exprs);

            /* Propagate information about restricted variables. The
             * variables used in arguments to restricted parameters of
             * old_instance are restricted in new_instance.
             *
             * REVISIT: Move to system.cpp?
             */
            std::set<symbol_t> &restricted = old_instance->restricted;
            for (size_t i = 0; i < expected; i++)
            {
                if (restricted.find(old_instance->parameters[i]) != restricted.end())
                {
                    collectDependencies(new_instance.restricted, exprs[i]);
                }
            }
        }
    }
    fragments.pop(arguments);
}

// Adds process_t* pointer to system_line
// Checks for dublicate entries
void SystemBuilder::process(const char* name)
{
    symbol_t symbol;
    if (!resolve(name, symbol))
    {
        throw TypeException(boost::format("No such process: %1%") % name);
    }
    type_t type = symbol.getType();
    if (type.getKind() != INSTANCE)
    {
        throw TypeException(boost::format("Not a template: %1%") % symbol.getName());
    }
    if (type.size() > 0)
    {
        // FIXME: Check type of unbound parameters
    }
    system->addProcess(*static_cast<instance_t*>(symbol.getData()));
    procPriority(name);
}

void SystemBuilder::done()
{

}

void SystemBuilder::beforeUpdate()
{
    system->setBeforeUpdate(fragments[0]);
    fragments.pop();
}

void SystemBuilder::afterUpdate()
{
    system->setAfterUpdate(fragments[0]);
    fragments.pop();
}

/********************************************************************
 * Priority
 */

void SystemBuilder::incChanPriority()
{
    currentChanPriority++;
}

void SystemBuilder::incProcPriority()
{
    currentProcPriority++;
}

void SystemBuilder::defaultChanPriority()
{
    system->setDefaultChanPriority(currentChanPriority);
}

void SystemBuilder::procPriority(const char* name)
{
    symbol_t symbol;
    if (!resolve(name, symbol))
    {
        handleError((boost::format("No such process: %1%") % name).str());
    }
    else
    {
        system->setProcPriority(name, currentProcPriority);
    }
}

void SystemBuilder::chanPriority()
{
    system->setChanPriority(fragments[0], currentChanPriority);
    fragments.pop();
}
