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
#include <boost/format.hpp>

#include "utap/expressionbuilder.h"

using namespace UTAP;
using namespace Constants;

using std::vector;
using std::string;
using std::map;

#define defaultIntMin -0x7FFF
#define defaultIntMax 0x7FFF

void ExpressionBuilder::ExpressionFragments::pop(uint32_t n)
{
    assert(n <= size());
    while (n--) pop();
}

ExpressionBuilder::ExpressionBuilder(TimedAutomataSystem *system)
  : system(system)
{
    pushFrame(system->getGlobals().frame);
    scalar_count = 0;
}

void ExpressionBuilder::addPosition(
    uint32_t position, uint32_t offset, uint32_t line, std::string path)
{
    system->addPosition(position, offset, line, path);
}

void ExpressionBuilder::handleError(string msg)
{
    system->addError(position, msg);
}

void ExpressionBuilder::handleWarning(string msg)
{
    system->addWarning(position, msg);
}

void ExpressionBuilder::pushFrame(frame_t frame)
{
    frames.push(frame);
}

void ExpressionBuilder::popFrame()
{
    frames.pop();
}

bool ExpressionBuilder::resolve(std::string name, symbol_t &uid)
{
    assert(!frames.empty());
    return frames.top().resolve(name, uid);
}

ExpressionBuilder::ExpressionFragments &ExpressionBuilder::getExpressions()
{
    return fragments;
}

bool ExpressionBuilder::isType(const char* name) 
{
    symbol_t uid;
    if (!resolve(name, uid))
    {
        return false;
    }
    return uid.getType().getKind() == TYPEDEF;
}

expression_t ExpressionBuilder::makeConstant(int value)
{
    return expression_t::createConstant(value, position);
}

type_t ExpressionBuilder::applyPrefix(PREFIX prefix, type_t type)
{    
    switch (prefix) 
    {
    case PREFIX_CONST:
        return type.createPrefix(CONSTANT, position);
    case PREFIX_META:
        return type.createPrefix(META, position);
    case PREFIX_URGENT:
        return type.createPrefix(URGENT, position);
    case PREFIX_BROADCAST:
        return type.createPrefix(BROADCAST, position);
    case PREFIX_URGENT_BROADCAST:
        return type.createPrefix(URGENT, position).createPrefix(BROADCAST, position);
    default:
        return type;
    }
}

void ExpressionBuilder::typeDuplicate()
{
    typeFragments.duplicate();
}

void ExpressionBuilder::typePop()
{
    typeFragments.pop();
}

void ExpressionBuilder::typeBool(PREFIX prefix) 
{
    type_t type = type_t::createPrimitive(Constants::BOOL, position);
    typeFragments.push(applyPrefix(prefix, type));
}

void ExpressionBuilder::typeInt(PREFIX prefix)
{
    type_t type = type_t::createPrimitive(Constants::INT, position);
    if (prefix != PREFIX_CONST)
    {
        type = type_t::createRange(type,
                                   makeConstant(defaultIntMin),
                                   makeConstant(defaultIntMax), 
                                   position);
    }
    typeFragments.push(applyPrefix(prefix, type));
}

void ExpressionBuilder::typeBoundedInt(PREFIX prefix)
{
    type_t type = type_t::createPrimitive(Constants::INT, position);
    type = type_t::createRange(type, fragments[1], fragments[0], position);
    fragments.pop(2);
    typeFragments.push(applyPrefix(prefix, type));
}

void ExpressionBuilder::typeChannel(PREFIX prefix)
{
    type_t type = type_t::createPrimitive(CHANNEL, position);
    typeFragments.push(applyPrefix(prefix, type));
}

void ExpressionBuilder::typeClock()
{
    type_t type = type_t::createPrimitive(CLOCK, position);
    typeFragments.push(type);
}

void ExpressionBuilder::typeVoid()
{
    type_t type = type_t::createPrimitive(VOID_TYPE, position);
    typeFragments.push(type);
}

static void collectDependencies(
    std::set<symbol_t> &dependencies, expression_t expr)
{
    std::set<symbol_t> symbols;
    expr.collectPossibleReads(symbols);
    while (!symbols.empty())
    {
        symbol_t s = *symbols.begin();
        symbols.erase(s);
        if (dependencies.find(s) == dependencies.end())
        {
            dependencies.insert(s);
            if (s.getData())
            {
                variable_t *v = static_cast<variable_t*>(s.getData());
                v->expr.collectPossibleReads(symbols);
            }
        }
    }
}

void ExpressionBuilder::typeScalar(PREFIX prefix)
{
    expression_t lower, upper;

    exprNat(1);
    exprBinary(MINUS);
    upper = fragments[0];
    lower = makeConstant(0);
    fragments.pop();

    type_t type = type_t::createPrimitive(SCALAR, position);
    type = type_t::createRange(type, lower, upper, position);
    type = applyPrefix(prefix, type);

    string count = (boost::format("%1%") % scalar_count++).str();

    type = type.createLabel(string("#scalarset") + count, position);

    if (currentTemplate)
    {
        /* Local scalar definitions are local to a particular process
         * - not to the template. Therefore we prefix it with the
         * template name and rename the template name to the process
         * name whenever evaluating a P.symbol expression (where P is
         * a processs). See exprDot().
         */
        type = type.createLabel(currentTemplate->uid.getName() + "::", position);

        /* There are restrictions on how the size of a scalar set is
         * given (may not depend on free process parameters).
         * Therefore mark all symbols in upper and those that they
         * depend on as restricted.
         */        
        collectDependencies(currentTemplate->restricted, upper);
    }
    typeFragments.push(type);
}

void ExpressionBuilder::typeName(PREFIX prefix, const char* name)
{
    symbol_t uid;
    assert(resolve(name, uid));

    if (!resolve(name, uid) || uid.getType().getKind() != TYPEDEF)
    {
        typeFragments.push(type_t::createPrimitive(VOID_TYPE));
        throw TypeException("Identifier is undeclared or not a type name");
    }

    type_t type = uid.getType()[0];

    /* We create a label here such that we can track the
     * position. This is not needed for type checking (we only use
     * name equivalence for scalarset, and they have a name embedded
     * in the type, see typeScalar()).
     */
    type = type.createLabel(uid.getName(), position);
    typeFragments.push(applyPrefix(prefix, type));
}

void ExpressionBuilder::exprTrue() 
{
    expression_t expr = makeConstant(1);
    expr.setType(type_t::createPrimitive(Constants::BOOL));
    fragments.push(expr);
}

void ExpressionBuilder::exprFalse() 
{
    expression_t expr = makeConstant(0);
    expr.setType(type_t::createPrimitive(Constants::BOOL));
    fragments.push(expr);
}
    
void ExpressionBuilder::exprId(const char *name) 
{
    symbol_t uid;
    
    if (!resolve(name, uid)) 
    {
        exprFalse();
        throw TypeException(boost::format("Unknown identifier: %1%") % name);
    }

    fragments.push(expression_t::createIdentifier(uid, position));
}

void ExpressionBuilder::exprDeadlock()
{
    fragments.push(expression_t::createDeadlock(position));
}

void ExpressionBuilder::exprNat(int32_t n) 
{
    fragments.push(makeConstant(n));
}

void ExpressionBuilder::exprCallBegin() 
{
}

// expects n argument expressions on the stack
void ExpressionBuilder::exprCallEnd(uint32_t n) 
{
    expression_t e;
    type_t type;
    instance_t *instance;

    /* n+1'th element from the top is the identifier. 
     */
    expression_t id = fragments[n];

    /* Create vector of sub expressions: The first expression
     * evaluates to the function or processset. The remaining
     * expressions are the arguments.
     */
    vector<expression_t> expr;
    for (int i = n; i >= 0; i--)
    {
        expr.push_back(fragments[i]);
    }
    fragments.pop(n + 1);

    /* The expression we create depends on whether id is a
     * function or a processset.
     */
    switch (id.getType().getKind())
    {
    case FUNCTION:
        if (expr.size() != id.getType().size())
        {
            handleError("Wrong number of arguments");
        }
        e = expression_t::createNary(FUNCALL, expr, position, id.getType()[0]);
        break;
        
    case PROCESSSET:
        if (expr.size() - 1!= id.getType().size())
        {
            handleError("Wrong number of arguments");
        }
        instance = static_cast<instance_t*>(id.getSymbol().getData());

        /* Process set lookups are represented as expressions indexing
         * into an array. To satisfy the type checker, we create a
         * type matching this structure.
         */
        type = type_t::createProcess(instance->templ->frame);
        for (size_t i = 0; i < instance->unbound; i++)
        {
            type = type_t::createArray(type, instance->parameters[instance->unbound - i - 1].getType());
        }

        /* Now create the expression. Each argument to the proces set
         * lookup is represented as an ARRAY expression.
         */
        e = id;
        e.setType(type);
        for (size_t i = 1; i < expr.size(); i++)
        {
            type = type.getSub();
            e = expression_t::createBinary(ARRAY, e, expr[i], position, type);
        }
        break;
        
    default:
        handleError("Function expected");
        e = makeConstant(0);
        break;
    }

    fragments.push(e);
}

// 2 expr     // array[index]
void ExpressionBuilder::exprArray() 
{
    // Pop sub-expressions
    expression_t var = fragments[1];
    expression_t index = fragments[0];
    fragments.pop(2);

    type_t element;
    type_t type = var.getType();
    if (type.isArray()) 
    {
        element = type.getSub();
    }
    else 
    {
        element = type_t();
    }

    fragments.push(expression_t::createBinary(
                       ARRAY, var, index, position, element));
}

// 1 expr
void ExpressionBuilder::exprPostIncrement() 
{
    fragments[0] = expression_t::createUnary(
        POSTINCREMENT, fragments[0], position);
}
    
void ExpressionBuilder::exprPreIncrement() 
{
    fragments[0] = expression_t::createUnary(
        PREINCREMENT, fragments[0], position, fragments[0].getType());
}
    
void ExpressionBuilder::exprPostDecrement() // 1 expr
{
    fragments[0] = expression_t::createUnary(
        POSTDECREMENT, fragments[0], position);
}
    
void ExpressionBuilder::exprPreDecrement() 
{
    fragments[0] = expression_t::createUnary(
        PREDECREMENT, fragments[0], position, fragments[0].getType());
}
    
void ExpressionBuilder::exprAssignment(kind_t op) // 2 expr
{
    expression_t lvalue = fragments[1];
    expression_t rvalue = fragments[0];
    fragments.pop(2);
    fragments.push(expression_t::createBinary(
                       op, lvalue, rvalue, position, lvalue.getType()));
}

void ExpressionBuilder::exprUnary(kind_t unaryop) // 1 expr
{
    switch (unaryop)
    {
    case PLUS:
        /* Unary plus can be ignored */
        break;
    case MINUS:
        unaryop = UNARY_MINUS;
        /* Fall through! */
    default:
        fragments[0] = expression_t::createUnary(unaryop, fragments[0], position);
    }
}
    
void ExpressionBuilder::exprBinary(kind_t binaryop) // 2 expr
{
    expression_t left = fragments[1];
    expression_t right = fragments[0];
    fragments.pop(2);
    fragments.push(expression_t::createBinary(
                       binaryop, left, right, position));
}

void ExpressionBuilder::exprTernary(kind_t ternaryop, bool firstMissing) // 3 expr
{
    expression_t first = firstMissing ? makeConstant(1) : fragments[2];
    expression_t second = fragments[1];
    expression_t third = fragments[0];
    fragments.pop(firstMissing ? 2 : 3);
    fragments.push(expression_t::createTernary(
                       ternaryop, first, second, third, position));
}

void ExpressionBuilder::exprInlineIf()
{
    expression_t c = fragments[2];
    expression_t t = fragments[1];
    expression_t e = fragments[0];
    fragments.pop(3);
    fragments.push(expression_t::createTernary(
                       INLINEIF, c, t, e, position, t.getType()));    
}

void ExpressionBuilder::exprComma()
{
    expression_t e1 = fragments[1];
    expression_t e2 = fragments[0];
    fragments.pop(2);
    fragments.push(expression_t::createBinary(
                       COMMA, e1, e2, position, e2.getType()));
}

void ExpressionBuilder::exprDot(const char *id)
{
    expression_t expr = fragments[0];
    type_t type = expr.getType();
    if (type.isRecord())
    {
        int32_t i  = type.findIndexOf(id);
        if (i == -1) 
        {
            std::string s = expr.toString(true);
            ParserBuilder::handleError("%s has no member named %s", 
                                       s.c_str(), id);
        } 
        else 
        {
            expr = expression_t::createDot(expr, i, position, type.getSub(i));
        }
    } 
    else if (type.isProcess())
    {
        symbol_t name = expr.getSymbol();
        instance_t *process = (instance_t *)name.getData();
        int32_t i = type.findIndexOf(id);
        if (i == -1) 
        {
            std::string s = expr.toString(true);
            ParserBuilder::handleError("%s has no member named %s", 
                                       s.c_str(), id);
        } 
        else if (type.getSub(i).isLocation())
        {
            expr = expression_t::createDot(expr, i, position, 
                                           type_t::createPrimitive(Constants::BOOL));
        }
        else 
        {
            type = type.getSub(i).rename(process->templ->uid.getName() + "::",
                                         name.getName() + "::");
            map<symbol_t, expression_t>::const_iterator arg;
            for (arg = process->mapping.begin(); arg != process->mapping.end(); arg++)
            {
                type = type.subst(arg->first, arg->second);
            }
            expr = expression_t::createDot(expr, i, position, type);
        }
    } 
    else 
    {
        std::string s = expr.toString(true);
        ParserBuilder::handleError("%s is not a structure", s.c_str());
    }
    fragments[0] = expr;
}

void ExpressionBuilder::exprForAllBegin(const char *name)
{
    type_t type = typeFragments[0];
    typeFragments.pop();

    if (!type.is(CONSTANT))
    {
        type = type.createPrefix(CONSTANT);
    }
    
    pushFrame(frame_t::createFrame(frames.top()));
    symbol_t symbol = frames.top().addSymbol(name, type);

    if (!type.isInteger() && !type.isScalar())
    {
        handleError("Quantifier must range over integer or scalar set");
    }
}

void ExpressionBuilder::exprForAllEnd(const char *name)
{
    /* Create the forall expression. The symbol is added as an identifier
     * expression as the first child. Notice that the frame is discarded
     * but the identifier expression will maintain a reference to the
     * symbol so it will not be deallocated.
     */
    fragments[0] = expression_t::createBinary(
        FORALL, 
        expression_t::createIdentifier(frames.top()[0], position), 
        fragments[0], position);
    popFrame();
}

void ExpressionBuilder::exprExistsBegin(const char *name)
{
    type_t type = typeFragments[0];
    typeFragments.pop();

    if (!type.is(CONSTANT))
    {
        type = type.createPrefix(CONSTANT);
    }
    
    pushFrame(frame_t::createFrame(frames.top()));
    symbol_t symbol = frames.top().addSymbol(name, type);

    if (!type.isInteger() && !type.isScalar())
    {
        handleError("Quantifier must range over integer or scalar set");
    }
}

void ExpressionBuilder::exprExistsEnd(const char *name)
{
    /* Create the exist expression. The symbol is added as an identifier
     * expression as the first child. Notice that the frame is discarded
     * but the identifier expression will maintain a reference to the
     * symbol so it will not be deallocated.
     */
    fragments[0] = expression_t::createBinary(
        EXISTS, expression_t::createIdentifier(
            frames.top()[0], position), fragments[0], position);
    popFrame();
}
