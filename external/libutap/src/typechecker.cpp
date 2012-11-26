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

#include <cmath>
#include <cstdio>
#include <cassert>
#include <list>
#include <stdexcept>
#include <boost/tuple/tuple.hpp>

#include "utap/utap.h"
#include "utap/typechecker.h"
#include "utap/systembuilder.h"

using std::exception;
using std::set;
using std::pair;
using std::make_pair;
using std::max;
using std::min;
using std::map;
using std::vector;
using std::list;

using boost::tie;

using namespace UTAP;
using namespace Constants;

/* The following are simple helper functions for testing the type of
 * expressions.
 */
static bool isCost(expression_t expr)
{
    return expr.getType().is(COST);
}

static bool isVoid(expression_t expr)
{
    return expr.getType().isVoid();
}

// static bool isScalar(expression_t expr)
// {
//     return expr.getType().isScalar();
// }

static bool isInteger(expression_t expr)
{
    return expr.getType().isInteger();
}

static bool isIntegral(expression_t expr)
{
    return expr.getType().isIntegral();
}

static bool isClock(expression_t expr)
{
    return expr.getType().isClock();
}

// static bool isRecord(expression_t expr)
// {
//     return expr.getType().isRecord();
// }

// static bool isArray(expression_t expr)
// {
//     return expr.getType().isArray();
// }

static bool isDiff(expression_t expr)
{
    return expr.getType().isDiff();
}

static bool isInvariant(expression_t expr)
{
    return expr.getType().isInvariant();
}

static bool isGuard(expression_t expr)
{
    return expr.getType().isGuard();
}

static bool isConstraint(expression_t expr)
{
    return expr.getType().isConstraint();
}

static bool isFormula(expression_t expr)
{
    return expr.getType().isFormula();
}

/**
 * Returns true iff type is a valid invariant. A valid invariant is
 * either an invariant expression or an integer expression.
 */
static bool isInvariantWR(expression_t expr)
{
    return isInvariant(expr) || (expr.getType().is(INVARIANT_WR));
}

/**
 * Returns true if values of this type can be assigned. This is the
 * case for integers, booleans, clocks, cost, scalars and arrays and
 * records of these. E.g. channels and processes are not assignable.
 */
static bool isAssignable(type_t type)
{
    switch (type.getKind())
    {
    case Constants::INT:
    case Constants::BOOL:
    case Constants::CLOCK:
    case Constants::COST:
    case Constants::SCALAR:
        return true;
    case ARRAY:
        return isAssignable(type[0]);
    case RECORD:
        for (size_t i = 0; i < type.size(); i++)
        {
            if (!isAssignable(type[i]))
            {
                return false;
            }
        }
        return true;
    default:
        return type.size() > 0 && isAssignable(type[0]);
    }
}

///////////////////////////////////////////////////////////////////////////

void CompileTimeComputableValues::visitVariable(variable_t &variable)
{
    if (variable.uid.getType().isConstant())
    {
        variables.insert(variable.uid);
    }
}

void CompileTimeComputableValues::visitInstance(instance_t &temp)
{
    frame_t parameters = temp.parameters;
    for (uint32_t i = 0; i < parameters.getSize(); i++)
    {
        type_t type = parameters[i].getType();
        if (!type.is(REF) && type.isConstant())
        {
            variables.insert(parameters[i]);
        }
    }
}

bool CompileTimeComputableValues::contains(symbol_t symbol) const
{
    return (variables.find(symbol) != variables.end());
}

///////////////////////////////////////////////////////////////////////////

class RateDecomposer
{
public:
    list<pair<expression_t,expression_t> > rate;
    expression_t invariant;

    void decompose(expression_t);
};

void RateDecomposer::decompose(expression_t expr)
{
    assert(isInvariantWR(expr));
    assert(isInvariant(expr) || expr.getKind() == AND || expr.getKind() == EQ);

    if (isInvariant(expr))
    {
        if (invariant.empty())
        {
            invariant = expr;
        }
        else
        {
            invariant = expression_t::createBinary(
                AND, invariant, expr);
        }
    }
    else if (expr.getKind() == AND)
    {
        decompose(expr[0]);
        decompose(expr[1]);
    }
    else
    {
        assert(expr[0].getType().getKind() == RATE
               ^ expr[1].getType().getKind() == RATE);

        if (expr[0].getType().getKind() == RATE)
        {
            rate.push_back(make_pair(expr[0][0], expr[1]));
        }
        else
        {
            rate.push_back(make_pair(expr[1][0], expr[0]));
        }
    }
}

///////////////////////////////////////////////////////////////////////////

TypeChecker::TypeChecker(
    TimedAutomataSystem *system)
    : system(system)
{
    system->accept(compileTimeComputableValues);

    checkExpression(system->getBeforeUpdate());
    checkExpression(system->getAfterUpdate());

    function = NULL;
}

template<class T>
void TypeChecker::handleWarning(T expr, std::string msg)
{
    system->addWarning(expr.getPosition(), msg.c_str());
}

template<class T>
void TypeChecker::handleError(T expr, std::string msg)
{
    system->addError(expr.getPosition(), msg.c_str());
}

/**
 * This method issues warnings for expressions, which do not change 
 * any variables. It is expected to be called for all expressions 
 * whose value is ignored. Unless such an expression has some kind
 * of side-effect, it does not have any purpose.
 *
 * Notice that in contrast to the regular side-effect analysis, 
 * this function accepts modifications of local variables as a 
 * side-effect.
 */
void TypeChecker::checkIgnoredValue(expression_t expr) 
{
    if (!expr.changesAnyVariable())
    {
        handleWarning(expr, "Expression does not have any effect");
    }
    else if (expr.getKind() == COMMA && !expr[1].changesAnyVariable())
    {
        handleWarning(expr[1], "Expression does not have any effect");        
    }
}

bool TypeChecker::isCompileTimeComputable(expression_t expr) const
{
    /* An expression is compile time computable if all identifers it
     * could possibly access during an evaluation are compile time
     * computable (i.e. their value is known at compile time).
     *
     * FIXME: We could maybe refine this to actually include function
     * local variables with compile time computable initialisers. This
     * would increase the class of models we accept while also getting
     * rid of the compileTimeComputableValues object.
     */
    set<symbol_t> reads;
    expr.collectPossibleReads(reads);
    for (set<symbol_t>::iterator i = reads.begin(); i != reads.end(); i++)
    {
        if (!i->getType().isFunction()
            && !compileTimeComputableValues.contains(*i))
        {
            return false;
        }
    }
    return true;
}

// static bool contains(frame_t frame, symbol_t symbol)
// {
//     for (size_t i = 0; i < frame.getSize(); i++)
//     {
//         if (frame[i] == symbol)
//         {
//             return true;
//         }
//     }
//     return false;
// }

/**
 * Check that the type is valid, i.e.:
 *
 * - all expressions such as array sizes, integer ranges, etc. are
 *   type correct,
 *
 * - only allowed prefixes are used (e.g. no urgent integers).
 *
 * - array sizes and integer bounds are compile time computable.
 *
 * If \a initialisable is true, then this method also checks that \a
 * type is initialisable.
 */
void TypeChecker::checkType(type_t type, bool initialisable)
{
    expression_t l, u;
    type_t size;
    frame_t frame;

    switch (type.getKind())
    {
    case LABEL:
        checkType(type[0], initialisable);
        break;

    case URGENT:
        if (!type.isLocation() && !type.isChannel())
        {
            handleError(type, "Prefix only allowed for locations and channels");
        }
        checkType(type[0], initialisable);
        break;

    case BROADCAST:
        if (!type.isChannel())
        {
            handleError(type, "Prefix only allowed for channels");
        }
        checkType(type[0], initialisable);
        break;

    case COMMITTED:
        if (!type.isLocation())
        {
            handleError(type, "Prefix only allowed for locations");
        }
        checkType(type[0], initialisable);
        break;

    case CONSTANT:
        checkType(type[0], true);
        break;

    case META:
        checkType(type[0], true);
        break;

    case REF:
        if (!type.isIntegral() && !type.isArray() && !type.isRecord()
            && !type.isChannel() && !type.isClock() && !type.isScalar())
        {
            handleError(type, "Reference to this type not allowed");
        }
        checkType(type[0], initialisable);
        break;

    case RANGE:
        if (!type.isInteger() && !type.isScalar())
        {
            handleError(type, "Range over this type not allowed");
        }
        tie(l, u) = type.getRange();
        if (checkExpression(l))
        {
            if (!isInteger(l))
            {
                handleError(l, "Integer expected");
            }
            if (!isCompileTimeComputable(l))
            {
                handleError(l, "Must be computable at compile time");
            }
        }
        if (checkExpression(u))
        {
            if (!isInteger(u))
            {
                handleError(u, "Integer expected");
            }
            if (!isCompileTimeComputable(u))
            {
                handleError(u, "Must be computable at compile time");
            }
        }

        break;

    case ARRAY:
        size = type.getArraySize();
        if (!size.is(RANGE))
        {
            handleError(type, "Invalid array size");
        }
        else
        {
            checkType(size);
        }
        checkType(type[0], initialisable);
        break;

    case RECORD:
        for (size_t i = 0; i < type.size(); i++)
        {
            checkType(type.getSub(i), true);
        }
        break;

    case Constants::INT:
    case Constants::BOOL:
        break;

    default:
        if (initialisable)
        {
            handleError(type, "This type cannot be declared const or meta");
        }
    }
}

void TypeChecker::visitSystemAfter(TimedAutomataSystem* system)
{
    std::list<chan_priority_t>& list = system->getMutableChanPriorities();
    std::list<chan_priority_t>::iterator i;
    for (i = list.begin(); i != list.end(); i++)
    {
        if (checkExpression(i->chanElement))
        {
            expression_t expr = i->chanElement;
            type_t channel = expr.getType();

            // Check that chanElement is a channel, or an array of channels.
            while (channel.isArray())
            {
                channel = channel.getSub();
            }
            if (!channel.isChannel())
            {
                handleError(expr, "Channel expected");
            }

            // Check index expressions
            while (expr.getKind() == ARRAY)
            {
                if (!isCompileTimeComputable(expr[1]))
                {
                    handleError(expr[1], "Must be computable at compile time");
                }
                else if (i->chanElement.changesAnyVariable())
                {
                    handleError(expr[1], "Index must be side-effect free");
                }
                expr = expr[0];
            }
        }
    }
}

void TypeChecker::visitProcess(instance_t &process)
{
    for (size_t i = 0; i < process.unbound; i++)
    {
        /* Unbound parameters of processes must be either scalars or
         * integers.
         */
        symbol_t parameter = process.parameters[i];
        type_t type = parameter.getType();
        if (!type.isScalar() && !type.isInteger() || type.is(REF))
        {
            handleError(type, "Free process parameters must be a bounded integer or scalar.");
        }

        /* Unbound parameters must not be used either directly or
         * indirectly in any array size declarations. I.e. they must
         * not be restricted.
         */
        if (process.restricted.find(parameter) != process.restricted.end())
        {
            handleError(type, "Free process parameters must not be used directly or indirectly in an array declaration.");
        }
    } 
}

void TypeChecker::visitVariable(variable_t &variable)
{
    SystemVisitor::visitVariable(variable);

    checkType(variable.uid.getType());
    if (!variable.expr.empty() && checkExpression(variable.expr))
    {
        if (!isCompileTimeComputable(variable.expr)) 
        {
            std::cerr << variable.expr << std::endl;
            handleError(variable.expr, "Must be computable at compile time");
        }
        else if (variable.expr.changesAnyVariable()) 
        {
            handleError(variable.expr, "Initialiser must be side-effect free");
        } 
        else 
        {
            variable.expr = 
                checkInitialiser(variable.uid.getType(), variable.expr);
        }
    }
}

void TypeChecker::visitState(state_t &state)
{
    SystemVisitor::visitState(state);

    if (!state.invariant.empty())
    {
        if (checkExpression(state.invariant))
        {
            if (!isInvariantWR(state.invariant))
            {
                std::string s = "Expression of type ";
                s += state.invariant.getType().toString();
                s += " cannot be used as an invariant";
                handleError(state.invariant, s);
            }
            else if (state.invariant.changesAnyVariable())
            {
                handleError(state.invariant, "Invariant must be side-effect free");
            }
            else
            {
                RateDecomposer decomposer;
                decomposer.decompose(state.invariant);
                state.invariant = decomposer.invariant;
                if (!decomposer.rate.empty())
                {
                    state.costrate = decomposer.rate.front().second;
                }
            }
        }
    }
}

void TypeChecker::visitEdge(edge_t &edge)
{
    SystemVisitor::visitEdge(edge);

    // select
    frame_t select = edge.select;
    for (size_t i = 0; i < select.getSize(); i++)
    {
        checkType(select[i].getType());
    }

    // guard
    if (!edge.guard.empty())
    {
        if (checkExpression(edge.guard))
        {
            if (!isGuard(edge.guard))
            {
                std::string s = "Expression of type ";
                s += edge.guard.getType().toString();
                s += " cannot be used as a guard";
                handleError(edge.guard, s);
            }
            else if (edge.guard.changesAnyVariable())
            {
                handleError(edge.guard, "Guard must be side-effect free");
            }
        }
    }

    // sync
    if (!edge.sync.empty())
    {
        if (checkExpression(edge.sync))
        {
            type_t channel = edge.sync.get(0).getType();
            if (!channel.isChannel())
            {
                handleError(edge.sync.get(0), "Channel expected");
            }
            else if (edge.sync.changesAnyVariable())
            {
                handleError(edge.sync,
                            "Synchronisation must be side-effect free");
            }
            else
            {
                bool hasClockGuard =
                    !edge.guard.empty() && !isIntegral(edge.guard);
                bool isUrgent = channel.is(URGENT);
                bool receivesBroadcast = channel.is(BROADCAST)
                    && edge.sync.getSync() == SYNC_QUE;

                if (isUrgent && hasClockGuard)
                {
                    handleError(edge.sync,
                                "Clock guards are not allowed on urgent edges");
                }
                else if (receivesBroadcast && hasClockGuard)
                {
                    handleError(edge.sync,
                                "Clock guards are not allowed on broadcast receivers");
                }
            }
        }
    }

    // assignment
    checkAssignmentExpression(edge.assign);
}

void TypeChecker::visitProgressMeasure(progress_t &progress)
{
    checkExpression(progress.guard);
    checkExpression(progress.measure);

    if (!progress.guard.empty() && !isIntegral(progress.guard))
    {
        handleError(progress.guard,
                    "Progress measure must evaluate to a boolean");
    }

    if (!isIntegral(progress.measure))
    {
        handleError(progress.measure,
                    "Progress measure must evaluate to a value");
    }
}

void TypeChecker::visitInstance(instance_t &instance)
{
    SystemVisitor::visitInstance(instance);

    /* Check the parameters of the instance. 
     */
    type_t type = instance.uid.getType();
    for (size_t i = 0; i < type.size(); i++)
    {
        checkType(type[i]);
    }

    /* Check arguments.
     */
    for (size_t i = type.size(); i < type.size() + instance.arguments; i++)
    {
        symbol_t parameter = instance.parameters[i];
        expression_t argument = instance.mapping[parameter];

        if (!checkExpression(argument))
        {
            continue;
        }

        // For template instantiation, the argument must be side-effect free
        if (argument.changesAnyVariable())
        {
            handleError(argument, "Argument must be side-effect free");
            continue;
        }

        // We have three ok cases:
        // - Value parameter with computable argument
        // - Constant reference with computable argument
        // - Reference parameter with unique lhs argument
        // If non of the cases are true, then we generate an error
        bool ref = parameter.getType().is(REF);
        bool constant = parameter.getType().isConstant();
        bool computable = isCompileTimeComputable(argument);

        if (!ref && !computable
            || ref && !constant && !isUniqueReference(argument) 
            || ref && constant && !computable)
        {
            handleError(argument, "Incompatible argument");
            continue;
        }

        checkParameterCompatible(parameter.getType(), argument);
    }
}

void TypeChecker::visitProperty(expression_t expr)
{
    if (checkExpression(expr))
    {
        if (expr.changesAnyVariable())
        {
            handleError(expr, "Property must be side-effect free");
        }
        if (!expr.getType().is(FORMULA))
        {
            handleError(expr, "Property must be a valid formula");
        }
        for (uint32_t i = 0; i < expr.getSize(); i++)
        {
            /* No nesting except for constraints and control formula */
            if (!isConstraint(expr[i]) &&
                expr.getType().is(FORMULA) &&
                expr.getKind() != CONTROL &&
                expr.getKind() != EF_CONTROL &&
                expr.getKind() != CONTROL_TOPT)
            {
                handleError(expr[i], "Nesting of path quantifiers is not allowed");
            }
        }
    }
}

/** 
 * Checks that \a expr is a valid assignment expression. Errors or
 * warnings are issued via calls to handleError() and
 * handleWarning(). Returns true if no errors were issued, false
 * otherwise.
 *
 * An assignment expression is any:
 *
 *  - expression of an expression statement,
 *
 *  - initialisation or step expression in a for-clause 
 *
 *  - expression in the update field of an edge
 */
bool TypeChecker::checkAssignmentExpression(expression_t expr)
{
    if (!checkExpression(expr))
    {
        return false;
    }
     
    if (!isAssignable(expr.getType()) && !isVoid(expr))
    {
        handleError(expr, "Invalid assignment expression");
        return false;
    }

    if (expr.getKind() != CONSTANT  || expr.getValue() != 1)
    {
        checkIgnoredValue(expr);
    }

    return true;
}

/** Checks that the expression can be used as a condition (e.g. for if). */
bool TypeChecker::checkConditionalExpressionInFunction(expression_t expr)
{
    if (!isIntegral(expr))
    {
        handleError(expr, "Boolean expected");
        return false;
    }
    return true;
}


static bool validReturnType(type_t type)
{
    frame_t frame;

    switch (type.getKind())
    {
    case Constants::RECORD:
        for (size_t i = 0; i < type.size(); i++)
        {
            if (!validReturnType(type[i]))
            {
                return false;
            }
        }
        return true;

    case Constants::RANGE:
    case Constants::LABEL:
        return validReturnType(type[0]);

    case Constants::INT:
    case Constants::BOOL:
    case Constants::SCALAR:
        return true;

    default:
        return false;
    }
}

void TypeChecker::visitFunction(function_t &fun)
{
    SystemVisitor::visitFunction(fun);

    /* Check that the return type is consistent and is a valid return
     * type.
     */
    type_t return_type = fun.uid.getType()[0];
    checkType(return_type);
    if (!return_type.isVoid() && !validReturnType(return_type))
    {
        handleError(return_type, "Invalid return type");
    }

    /* Type check the function body: Type checking return statements
     * requires access to the return type, hence we store a pointer to
     * the current function being type checked in the \a function
     * member.
     */
    function = &fun;
    fun.body->accept(this);
    function = NULL;

    /* Collect identifiers of things external to the function accessed
     * or changed by the function. Notice that neither local variables
     * nor parameters are considered to be changed or accessed by a
     * function.
     */
    CollectChangesVisitor visitor(fun.changes);
    fun.body->accept(&visitor);

    CollectDependenciesVisitor visitor2(fun.depends);
    fun.body->accept(&visitor2);

    list<variable_t> &vars = fun.variables;
    for (list<variable_t>::iterator i = vars.begin(); i != vars.end(); i++)
    {
        fun.changes.erase(i->uid);
        fun.depends.erase(i->uid);
    }
    size_t parameters = fun.uid.getType().size() - 1;
    for (size_t i = 0; i < parameters; i++)
    {
        fun.changes.erase(fun.body->getFrame()[i]);
        fun.depends.erase(fun.body->getFrame()[i]);
    }
}

int32_t TypeChecker::visitEmptyStatement(EmptyStatement *stat)
{
    return 0;
}

int32_t TypeChecker::visitExprStatement(ExprStatement *stat)
{
    checkAssignmentExpression(stat->expr);
    return 0;
}

int32_t TypeChecker::visitAssertStatement(AssertStatement *stat)
{
    if (checkExpression(stat->expr) && stat->expr.changesAnyVariable())
    {
        handleError(stat->expr, "Assertion must be side-effect free");
    }
    return 0;
}

int32_t TypeChecker::visitForStatement(ForStatement *stat)
{
    checkAssignmentExpression(stat->init);

    if (checkExpression(stat->cond))
    {
        checkConditionalExpressionInFunction(stat->cond);
    }

    checkAssignmentExpression(stat->step);

    return stat->stat->accept(this);
}

int32_t TypeChecker::visitIterationStatement(IterationStatement *stat)
{
    type_t type = stat->symbol.getType();
    checkType(type);

    /* We only support iteration over scalars and integers.
     */
    if (!type.isScalar() && !type.isInteger())
    {
        handleError(type, "Scalar set or integer expected");
    }
    else if (!type.is(RANGE))
    {
        handleError(type, "Range expected");
    }

    return stat->stat->accept(this);
}

int32_t TypeChecker::visitWhileStatement(WhileStatement *stat)
{
    if (checkExpression(stat->cond))
    {
        checkConditionalExpressionInFunction(stat->cond);
    }
    return stat->stat->accept(this);
}

int32_t TypeChecker::visitDoWhileStatement(DoWhileStatement *stat)
{
    if (checkExpression(stat->cond))
    {
        checkConditionalExpressionInFunction(stat->cond);
    }
    return stat->stat->accept(this);
}

int32_t TypeChecker::visitBlockStatement(BlockStatement *stat)
{
    BlockStatement::iterator i;

    /* Check type and initialiser of local variables (parameters are
     * also considered local variables).
     */
    frame_t frame = stat->getFrame();
    for (uint32_t i = 0; i < frame.getSize(); i++)
    {
        symbol_t symbol = frame[i];
        checkType(symbol.getType());
        if (symbol.getData())
        {
            variable_t *var = static_cast<variable_t*>(symbol.getData());
            if (!var->expr.empty() && checkExpression(var->expr))
            {
                if (var->expr.changesAnyVariable())
                {
                    /* This is stronger than C. However side-effects in
                     * initialisers are nasty: For records, the evaluation
                     * order may be different from the order in the input
                     * file.
                     */
                    handleError(var->expr, "Initialiser must be side-effect free");
                } 
                else
                {
                    var->expr = checkInitialiser(symbol.getType(), var->expr);
                }
            }
        }        
    }    

    /* Check statements.
     */
    for (i = stat->begin(); i != stat->end(); ++i)
    {
        (*i)->accept(this);
    }
    return 0;
}

int32_t TypeChecker::visitIfStatement(IfStatement *stat)
{
    if (checkExpression(stat->cond))
    {
        checkConditionalExpressionInFunction(stat->cond);
    }
    stat->trueCase->accept(this);
    if (stat->falseCase)
    {
        stat->falseCase->accept(this);
    }
    return 0;
}

int32_t TypeChecker::visitReturnStatement(ReturnStatement *stat)
{
    if (!stat->value.empty())
    {
        checkExpression(stat->value);

        /* The only valid return types are integers and records. For these
         * two types, the type rules are the same as for parameters.
         */
        type_t return_type = function->uid.getType()[0];
        checkParameterCompatible(return_type, stat->value);
    }
    return 0;
}

/**
 * Returns a value indicating the capabilities of a channel. For
 * urgent channels this is 0, for non-urgent broadcast channels this
 * is 1, and in all other cases 2. An argument to a channel parameter
 * must have at least the same capability as the parameter.
 */
static int channelCapability(type_t type)
{
    assert(type.isChannel());
    if (type.is(URGENT))
    {
        return 0;
    }
    if (type.is(BROADCAST))
    {
        return 1;
    }
    return 2;
}

/**
 * Returns true if two scalar types are name-equivalent.
 */
static bool isSameScalarType(type_t t1, type_t t2)
{
    if (t1.getKind() == REF || t1.getKind() == CONSTANT || t1.getKind() == META)
    {
        return isSameScalarType(t1[0], t2);
    }
    else if (t2.getKind() == REF || t2.getKind() == CONSTANT || t2.getKind() == META)
    {
        return isSameScalarType(t1, t2[0]);
    }
    else if (t1.getKind() == LABEL && t2.getKind() == LABEL)
    {
        return t1.getLabel(0) == t2.getLabel(0) 
            && isSameScalarType(t1[0], t2[0]);
    }
    else if (t1.getKind() == SCALAR && t2.getKind() == SCALAR)
    {
        return true;
    }
    else if (t1.getKind() == RANGE && t2.getKind() == RANGE)
    {
        return isSameScalarType(t1[0], t2[0])
            && t1.getRange().first.equal(t2.getRange().first)
            && t1.getRange().second.equal(t2.getRange().second);
    }
    else
    {
        return false;
    }
}

/**
 * Returns true iff argument type is compatible with parameter type.
 */
bool TypeChecker::isParameterCompatible(type_t paramType, expression_t arg)
{
    bool ref = paramType.is(REF);
    bool constant = paramType.isConstant();
    bool lvalue = isModifiableLValue(arg);

    type_t argType = arg.getType();

    // For non-const reference parameters, we require a modifiable
    // lvalue argument
    if (ref && !constant && !lvalue)
    {
        return false;
    }

    if (paramType.isChannel() && argType.isChannel())
    {
        return channelCapability(argType) >= channelCapability(paramType);
    }
    else if (ref && lvalue)
    {
        return areEquivalent(argType, paramType);
    }
    else
    {
        return areAssignmentCompatible(paramType, argType);
    }
}

/**
 * Checks whether argument type is compatible with parameter type.
 */
bool TypeChecker::checkParameterCompatible(type_t paramType, expression_t arg)
{
    if (!isParameterCompatible(paramType, arg))
    {
        handleError(arg, "Inompatible argument");
        return false;
    }
    return true;
}

/**
 * Checks whether init is a valid initialiser for a variable or
 * constant of the given type. For record types, the initialiser is
 * reordered to fit the order of the fields and the new initialiser is
 * returned. REVISIT: Can a record initialiser have side-effects? Then
 * such reordering is not valid.
 */
expression_t TypeChecker::checkInitialiser(type_t type, expression_t init)
{
    if (areAssignmentCompatible(type, init.getType()))
    {
        return init;
    }
    else if (type.isArray() && init.getKind() == LIST)
    {
        type_t subtype = type.getSub();
        vector<expression_t> result(init.getSize(), expression_t());
        for (uint32_t i = 0; i < init.getType().size(); i++) 
        {
            if (!init.getType().getLabel(i).empty()) 
            {
                handleError(
                    init[i], "Field name not allowed in array initialiser");
            }
            result[i] = checkInitialiser(subtype, init[i]);
        }
        return expression_t::createNary(
            LIST, result, init.getPosition(), type);
    }
    else if (type.isRecord() || init.getKind() == LIST) 
    {
        /* In order to access the record labels we have to strip any
         * prefixes and labels from the record type.
         */
        vector<expression_t> result(type.getRecordSize(), expression_t());
        int32_t current = 0;
        for (uint32_t i = 0; i < init.getType().size(); i++, current++) 
        {
            std::string label = init.getType().getLabel(i);
            if (!label.empty())
            {
                current = type.findIndexOf(label);
                if (current == -1)
                {
                    handleError(init[i], "Unknown field");
                    break;
                }
            }

            if (current >= (int32_t)type.getRecordSize()) 
            {
                handleError(init[i], "Excess elements in intialiser");
                break;
            }

            if (!result[current].empty())
            {
                handleError(init[i], "Multiple initialisers for field");
                continue;
            }

            result[current] = checkInitialiser(type.getSub(current), init[i]);
        }

        // Check that all fields do have an initialiser.
        for (size_t i = 0; i < result.size(); i++) 
        {
            if (result[i].empty())
            {
                handleError(init, "Incomplete initialiser");
                break;
            }
        }

        return expression_t::createNary(
            LIST, result, init.getPosition(), type);
    }
    handleError(init, "Invalid initialiser");
    return init;
}

/** Returns true if arguments of an inline if are compatible. The
    'then' and 'else' arguments are compatible if and only if they
    have the same base type. In case of arrays, they must have the
    same size and the subtypes must be compatible. In case of records,
    they must have the same type name.
*/
bool TypeChecker::areInlineIfCompatible(type_t t1, type_t t2) const
{
    if (t1.isIntegral() && t2.isIntegral())
    {
        return true;
    }
    else
    {
        return areEquivalent(t1, t2);
    }
}

/**
 * Returns true iff \a a and \a b are structurally
 * equivalent. However, CONST, META, and REF are ignored. Scalar sets
 * are checked using named equivalence.
 */
bool TypeChecker::areEquivalent(type_t a, type_t b) const
{
    if (a.isInteger() && b.isInteger())
    {
        return !a.is(RANGE)
            || !b.is(RANGE)
            || (a.getRange().first.equal(b.getRange().first)
                && a.getRange().second.equal(b.getRange().second));
    }
    else if (a.isBoolean() && b.isBoolean())
    {
        return true;
    }
    else if (a.isClock() && b.isClock())
    {
        return true;
    }
    else if (a.isChannel() && b.isChannel())
    {
        return channelCapability(a) == channelCapability(b);
    }
    else if (a.isRecord() && b.isRecord())
    {
        size_t aSize = a.getRecordSize();
        size_t bSize = b.getRecordSize();
        if (aSize == bSize)
        {
            for (size_t i = 0; i < aSize; i++)
            {
                if (a.getRecordLabel(i) != b.getRecordLabel(i)
                    || !areEquivalent(a.getSub(i), b.getSub(i)))
                {
                    return false;
                }
            }
            return true;
        }
    }
    else if (a.isArray() && b.isArray())
    {
        type_t asize = a.getArraySize();
        type_t bsize = b.getArraySize();

        if (asize.isInteger() && bsize.isInteger())
        {
            return asize.getRange().first.equal(bsize.getRange().first)
                && asize.getRange().second.equal(bsize.getRange().second)
                && areEquivalent(a.getSub(), b.getSub());
        }
        else if (asize.isScalar() && bsize.isScalar())
        {
            return isSameScalarType(asize, bsize)
                && areEquivalent(a.getSub(), b.getSub());
        }
        return false;
    }
    else if (a.isScalar() && b.isScalar())
    {
        return isSameScalarType(a, b);
    }

    return false;
}

/** Returns true if lvalue and rvalue are assignment compatible.  This
    is the case when an expression of type rvalue can be assigned to
    an expression of type lvalue. It does not check whether lvalue is
    actually a left-hand side value. In case of integers, it does not
    check the range of the expressions.
*/
bool TypeChecker::areAssignmentCompatible(type_t lvalue, type_t rvalue) const
{
    if (lvalue.isClock() && rvalue.isIntegral())
    {
        return true;
    }
    else if (lvalue.isIntegral() && rvalue.isIntegral())
    {
        return true;
    }
    return areEquivalent(lvalue, rvalue);
}

/** 
 * Returns true if two types are compatible for comparison using the
 * equality operator. 
 *
 * Two types are compatible if they are structurally
 * equivalent. However for scalar we use name equivalence.  Prefixes
 * like CONST, META, URGENT, COMMITTED, BROADCAST, REF and TYPENAME
 * are ignored.
 *
 * Clocks are not handled by this method: If t1 or t2 are clock-types,
 * then false is returned.
 */
bool TypeChecker::areEqCompatible(type_t t1, type_t t2) const
{
    if (t1.isIntegral() && t2.isIntegral())
    {
        return true;
    }
    else
    {
        return areEquivalent(t1, t2);
    }
}

/** Type check and checkExpression the expression. This function performs
    basic type checking of the given expression and assigns a type to
    every subexpression of the expression. It checks that only
    left-hand side values are updated, checks that functions are
    called with the correct arguments, checks that operators are used
    with the correct operands and checks that operands to assignment
    operators are assignment compatible. Errors are reported by
    calling handleError(). This function does not check/compute the
    range of integer expressions and thus does not produce
    out-of-range errors or warnings. Returns true if no type errors
    were found, false otherwise.
*/
bool TypeChecker::checkExpression(expression_t expr)
{
    /* Do not checkExpression empty expressions.
     */
    if (expr.empty())
    {
        return true;
    }

    /* CheckExpression sub-expressions.
     */
    bool ok = true;
    for (uint32_t i = 0; i < expr.getSize(); i++)
    {
        ok &= checkExpression(expr[i]);
    }

    /* Do not checkExpression the expression if any of the sub-expressions
     * contained errors.
     */
    if (!ok)
    {
        return false;
    }

    /* CheckExpression the expression. This depends on the kind of expression
     * we are dealing with.
     */
    type_t type, arg1, arg2, arg3;
    switch (expr.getKind())
    {
    case PLUS:
        if (isIntegral(expr[0]) && isIntegral(expr[1]))
        {
            type = type_t::createPrimitive(Constants::INT);
        }
        else if (isInteger(expr[0]) && isClock(expr[1]) 
            || isClock(expr[0]) && isInteger(expr[1]))
        {
            type = type_t::createPrimitive(CLOCK);
        }
        else if (isDiff(expr[0]) && isInteger(expr[1])
                 || isInteger(expr[0]) && isDiff(expr[1]))
        {
            type = type_t::createPrimitive(DIFF);
        }
        break;

    case MINUS:
        if (isIntegral(expr[0]) && isIntegral(expr[1]))
        {
            type = type_t::createPrimitive(Constants::INT);
        }
        else if (isClock(expr[0]) && isInteger(expr[1]))
            // removed  "|| isInteger(expr[0].type) && isClock(expr[1].type)"
            // in order to be able to convert into ClockGuards
        {
            type = type_t::createPrimitive(CLOCK);
        }
        else if (isDiff(expr[0]) && isInteger(expr[1])
                 || isInteger(expr[0]) && isDiff(expr[1])
                 || isClock(expr[0]) && isClock(expr[1]))
        {
            type = type_t::createPrimitive(DIFF);
        }
        break;

    case AND:
        if (isIntegral(expr[0]) && isIntegral(expr[1]))
        {
            type = type_t::createPrimitive(Constants::BOOL);
        }
        else if (isInvariant(expr[0]) && isInvariant(expr[1]))
        {
            type = type_t::createPrimitive(INVARIANT);
        }
        else if (isInvariantWR(expr[0]) && isInvariantWR(expr[1]))
        {
            type = type_t::createPrimitive(INVARIANT_WR);
        }
        else if (isGuard(expr[0]) && isGuard(expr[1]))
        {
            type = type_t::createPrimitive(GUARD);
        }
        else if (isConstraint(expr[0]) && isConstraint(expr[1]))
        {
            type = type_t::createPrimitive(CONSTRAINT);
        }
        break;

    case OR:
        if (isIntegral(expr[0]) && isIntegral(expr[1]))
        {
            type = type_t::createPrimitive(Constants::BOOL);
        }
        else if (isIntegral(expr[0]) && isInvariant(expr[1]))
        {
            type = type_t::createPrimitive(INVARIANT);
        }
        else if (isIntegral(expr[0]) && isGuard(expr[1]))
        {
            type = type_t::createPrimitive(GUARD);
        }
        else if (isConstraint(expr[0]) && isConstraint(expr[1]))
        {
            type = type_t::createPrimitive(CONSTRAINT);
        }
        break;

    case LT:
    case LE:
        if (isIntegral(expr[0]) && isIntegral(expr[1]))
        {
            type = type_t::createPrimitive(Constants::BOOL);
        }
        else if (isClock(expr[0]) && isClock(expr[1])
            || isClock(expr[0]) && isInteger(expr[1])
            || isDiff(expr[0]) && isInteger(expr[1])
            || isInteger(expr[0]) && isDiff(expr[1]))
        {
            type = type_t::createPrimitive(INVARIANT);
        }
        else if (isInteger(expr[0]) && isClock(expr[1]))
        {
            type = type_t::createPrimitive(GUARD);
        }
        break;

    case EQ:
        if (isClock(expr[0]) && isClock(expr[1])
            || isClock(expr[0]) && isInteger(expr[1])
            || isInteger(expr[0]) && isClock(expr[1])
            || isDiff(expr[0]) && isInteger(expr[1])
            || isInteger(expr[0]) && isDiff(expr[1]))
        {
            type = type_t::createPrimitive(GUARD);
        }
        else if (areEqCompatible(expr[0].getType(), expr[1].getType()))
        {
            type = type_t::createPrimitive(Constants::BOOL);
        }
        else if (expr[0].getType().is(RATE) && isInteger(expr[1])
                 || isInteger(expr[0]) && expr[1].getType().is(RATE))
        {
            type = type_t::createPrimitive(INVARIANT_WR);
        }
        break;

    case NEQ:
        if (areEqCompatible(expr[0].getType(), expr[1].getType()))
        {
            type = type_t::createPrimitive(Constants::BOOL);
        }
        else if (isClock(expr[0]) && isClock(expr[1])
            || isClock(expr[0]) && isInteger(expr[1])
            || isInteger(expr[0]) && isClock(expr[1])
            || isDiff(expr[0]) && isInteger(expr[1])
            || isInteger(expr[0]) && isDiff(expr[1]))
        {
            type = type_t::createPrimitive(CONSTRAINT);
        }
        break;

    case GE:
    case GT:
        if (isIntegral(expr[0]) && isIntegral(expr[1]))
        {
            type = type_t::createPrimitive(Constants::BOOL);
        }
        else if (isClock(expr[0]) && isClock(expr[1])
            || isInteger(expr[0]) && isClock(expr[1])
            || isDiff(expr[0]) && isInteger(expr[1])
            || isInteger(expr[0]) && isDiff(expr[1]))
        {
            type = type_t::createPrimitive(INVARIANT);
        }
        else if (isClock(expr[0]) && isInteger(expr[1]))
        {
            type = type_t::createPrimitive(GUARD);
        }
        break;

    case MULT:
    case DIV:
    case MOD:
    case BIT_AND:
    case BIT_OR:
    case BIT_XOR:
    case BIT_LSHIFT:
    case BIT_RSHIFT:
    case MIN:
    case MAX:
        if (isIntegral(expr[0]) && isIntegral(expr[1]))
        {
            type = type_t::createPrimitive(Constants::INT);
        }
        break;

    case NOT:
        if (isIntegral(expr[0]))
        {
            type = type_t::createPrimitive(Constants::BOOL);
        }
        else if (isConstraint(expr[0]))
        {
            type = type_t::createPrimitive(CONSTRAINT);
        }
        break;

    case UNARY_MINUS:
        if (isIntegral(expr[0]))
        {
            type = type_t::createPrimitive(Constants::INT);
        }
        break;

    case RATE:
        if (isCost(expr[0]))
        {
            type = type_t::createPrimitive(RATE);
        }
        break;

    case ASSIGN:
        if (!areAssignmentCompatible(expr[0].getType(), expr[1].getType()))
        {
            handleError(expr, "Incompatible types");
            return false;
        }
        else if (!isModifiableLValue(expr[0]))
        {
            handleError(expr[0], "Left hand side value expected");
            return false;
        }
        type = expr[0].getType();
        break;

    case ASSPLUS:
        if (!isInteger(expr[0]) && !isCost(expr[0]) || !isIntegral(expr[1]))
        {
            handleError(expr, "Increment operator can only be used for integer and cost variables.");
        }
        else if (!isModifiableLValue(expr[0]))
        {
            handleError(expr[0], "Left hand side value expected");
        }
        type = expr[0].getType();
        break;

    case ASSMINUS:
    case ASSDIV:
    case ASSMOD:
    case ASSMULT:
    case ASSAND:
    case ASSOR:
    case ASSXOR:
    case ASSLSHIFT:
    case ASSRSHIFT:
        if (!isIntegral(expr[0]) || !isIntegral(expr[1]))
        {
            handleError(expr, "Non-integer types must use regular assignment operator");
            return false;
        }
        else if (!isModifiableLValue(expr[0]))
        {
            handleError(expr[0], "Left hand side value expected");
            return false;
        }
        type = expr[0].getType();
        break;

    case POSTINCREMENT:
    case PREINCREMENT:
    case POSTDECREMENT:
    case PREDECREMENT:
        if (!isModifiableLValue(expr[0]))
        {
            handleError(expr[0], "Left hand side value expected");
            return false;
        }
        else if (!isInteger(expr[0]))
        {
            handleError(expr, "Integer expected");
            return false;
        }
        type = type_t::createPrimitive(Constants::INT);
        break;

    case INLINEIF:
        if (!isIntegral(expr[0]))
        {
            handleError(expr, "First argument of inline if must be an integer");
            return false;
        }
        if (!areInlineIfCompatible(expr[1].getType(), expr[2].getType()))
        {
            handleError(expr, "Incompatible arguments to inline if");
            return false;
        }
        type = expr[1].getType();
        break;

    case COMMA:
        if (!isAssignable(expr[0].getType()) && !isVoid(expr[0])) 
        {
            handleError(expr[0], "Incompatible type for comma expression");
            return false;
        }
        if (!isAssignable(expr[1].getType()) && !isVoid(expr[1]))
        {
            handleError(expr[1], "Incompatible type for comma expression");
            return false;
        }
        checkIgnoredValue(expr[0]);
        type = expr[1].getType();
        break;

    case FUNCALL:
    {
        checkExpression(expr[0]);

        bool result = true;
        type_t type = expr[0].getType();
        size_t parameters = type.size() - 1;
        for (uint32_t i = 0; i < parameters; i++) 
        {
            type_t parameter = type[i + 1];
            expression_t argument = expr[i + 1];
            result &= checkParameterCompatible(parameter, argument);
        }
        return result;
    }

    case ARRAY:
        arg1 = expr[0].getType();
        arg2 = expr[1].getType();

        /* The left side must be an array.
         */
        if (!arg1.isArray())
        {
            handleError(expr[0], "Array expected");
            return false;
        }
        type = arg1.getSub();

        /* Check the type of the index.
         */
        if (arg1.getArraySize().isInteger() && arg2.isIntegral())
        {

        }
        else if (arg1.getArraySize().isScalar() && arg2.isScalar())
        {
            if (!isSameScalarType(arg1.getArraySize(), arg2))
            {
                handleError(expr[1], "Incompatible type");
                return false;
            }
        }
        else
        {
            handleError(expr[1], "Incompatible type");
        }
        break;


    case FORALL:
        checkType(expr[0].getSymbol().getType());

        if (isIntegral(expr[1]))
        {
            type = type_t::createPrimitive(Constants::BOOL);
        }
        else if (isInvariant(expr[1]))
        {
            type = type_t::createPrimitive(INVARIANT);
        }
        else if (isGuard(expr[1]))
        {
            type = type_t::createPrimitive(GUARD);
        }
        else if (isConstraint(expr[1]))
        {
            type = type_t::createPrimitive(CONSTRAINT);
        }
        else
        {
            handleError(expr[1], "Boolean expected");
        }

        if (expr[1].changesAnyVariable()) 
        {
            handleError(expr[1], "Expression must be side-effect free");
        }
        break;

    case EXISTS:
        checkType(expr[0].getSymbol().getType());

        if (isIntegral(expr[1]))
        {
            type = type_t::createPrimitive(Constants::BOOL);
        }
        else if (isConstraint(expr[1]))
        {
            type = type_t::createPrimitive(CONSTRAINT);
        }
        else
        {
            handleError(expr[1], "Boolean expected");
        }

        if (expr[1].changesAnyVariable())
        {
            handleError(expr[1], "Expression must be side-effect free");
        }
        break;

    case AF:
    case AG:
    case EF:
    case EG:
    case AF2:
    case AG2:
    case EF2:
    case EG2:
    case EF_R:
    case AG_R:
    case EF_CONTROL:
    case CONTROL:
    case CONTROL_TOPT:
        if (isFormula(expr[0]))
        {
            type = type_t::createPrimitive(FORMULA);
        }
        break;

    case LEADSTO:
    case A_UNTIL:
    case A_WEAKUNTIL:
        if (isFormula(expr[0]) && isFormula(expr[1]))
        {
            type = type_t::createPrimitive(FORMULA);
        }
        break;

    default:
        return true;
    }

    if (type.unknown())
    {
        handleError(expr, "Type error");
        return false;
    }
    else
    {
        expr.setType(type);
        return true;
    }
}

/**
 * Returns true if expression evaluates to a modifiable l-value.
 */
bool TypeChecker::isModifiableLValue(expression_t expr) const
{
    type_t t, f;
    switch (expr.getKind())
    {
    case IDENTIFIER:
        return expr.getType().isNonConstant();

    case DOT:
        /* Processes can only be used in properties, which must be
         * side-effect anyway. Therefore returning false below is
         * acceptable for now (REVISIT).
         */
        if (expr[0].getType().isProcess())
        {
            return false;
        }
        // REVISIT: Not correct if records contain constant fields.
        return isModifiableLValue(expr[0]);

    case ARRAY:
        return isModifiableLValue(expr[0]);

    case PREINCREMENT:
    case PREDECREMENT:
    case ASSIGN:
    case ASSPLUS:
    case ASSMINUS:
    case ASSDIV:
    case ASSMOD:
    case ASSMULT:
    case ASSAND:
    case ASSOR:
    case ASSXOR:
    case ASSLSHIFT:
    case ASSRSHIFT:
        return true;

    case INLINEIF:
        return isModifiableLValue(expr[1]) && isModifiableLValue(expr[2])
            && areEquivalent(expr[1].getType(), expr[2].getType());

    case COMMA:
        return isModifiableLValue(expr[1]);

    case FUNCALL:
        // Functions cannot return references (yet!)

    default:
        return false;
    }
}

/**
 * Returns true iff \a expr evaluates to an lvalue.
 */
bool TypeChecker::isLValue(expression_t expr) const
{
    type_t t, f;
    switch (expr.getKind())
    {
    case IDENTIFIER:
    case PREINCREMENT:
    case PREDECREMENT:
    case ASSIGN:
    case ASSPLUS:
    case ASSMINUS:
    case ASSDIV:
    case ASSMOD:
    case ASSMULT:
    case ASSAND:
    case ASSOR:
    case ASSXOR:
    case ASSLSHIFT:
    case ASSRSHIFT:
        return true;

    case DOT:
    case ARRAY:
        return isLValue(expr[0]);

    case INLINEIF:
        return isLValue(expr[1]) && isLValue(expr[2])
            && areEquivalent(expr[1].getType(), expr[2].getType());

    case COMMA:
        return isLValue(expr[1]);

    case FUNCALL:
        // Functions cannot return references (yet!)

    default:
        return false;
    }
}

/** Returns true if expression is a reference to a unique variable.
    This is similar to expr being an l-value, but in addition we
    require that the reference does not depend on any non-computable
    expressions. Thus i[v] is a l-value, but if v is a non-constant
    variable, then it does not result in a unique reference.
*/
bool TypeChecker::isUniqueReference(expression_t expr) const
{
    switch (expr.getKind())
    {
    case IDENTIFIER:
        return true;

    case DOT:
        return isUniqueReference(expr[0]);

    case ARRAY:
        return isUniqueReference(expr[0])
            && isCompileTimeComputable(expr[1]);

    case PREINCREMENT:
    case PREDECREMENT:
    case ASSIGN:
    case ASSPLUS:
    case ASSMINUS:
    case ASSDIV:
    case ASSMOD:
    case ASSMULT:
    case ASSAND:
    case ASSOR:
    case ASSXOR:
    case ASSLSHIFT:
    case ASSRSHIFT:
        return isUniqueReference(expr[0]);

    case INLINEIF:
        return false;

    case COMMA:
        return isUniqueReference(expr[1]);

    case FUNCALL:
        // Functions cannot return references (yet!)

    default:
        return false;
    }
}

bool parseXTA(FILE *file, TimedAutomataSystem *system, bool newxta)
{
    SystemBuilder builder(system);
    parseXTA(file, &builder, newxta);
    if (!system->hasErrors())
    {
        TypeChecker checker(system);
        system->accept(checker);
    }
    return !system->hasErrors();
}

bool parseXTA(const char *buffer, TimedAutomataSystem *system, bool newxta)
{
    SystemBuilder builder(system);
    parseXTA(buffer, &builder, newxta);
    if (!system->hasErrors())
    {
        TypeChecker checker(system);
        system->accept(checker);
    }
    return !system->hasErrors();
}

int32_t parseXMLBuffer(const char *buffer, TimedAutomataSystem *system, bool newxta)
{
    int err;

    SystemBuilder builder(system);
    err = parseXMLBuffer(buffer, &builder, newxta);

    if (err)
    {
        return err;
    }

    if (!system->hasErrors())
    {
        TypeChecker checker(system);
        system->accept(checker);
    }

    return 0;
}

int32_t parseXMLFile(const char *file, TimedAutomataSystem *system, bool newxta)
{
    int err;

    SystemBuilder builder(system);
    err = parseXMLFile(file, &builder, newxta);
    if (err)
    {
        return err;
    }

    if (!system->hasErrors())
    {
        TypeChecker checker(system);
        system->accept(checker);
    }

    return 0;
}

expression_t parseExpression(const char *str,
                             TimedAutomataSystem *system, bool newxtr)
{
    ExpressionBuilder builder(system);
    parseXTA(str, &builder, newxtr, S_EXPRESSION, "");
    expression_t expr = builder.getExpressions()[0];
    if (!system->hasErrors())
    {
        TypeChecker checker(system);
        checker.checkExpression(expr);
    }
    return expr;
}
