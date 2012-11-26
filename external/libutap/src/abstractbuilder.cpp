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

#include <cstdarg>
#include <vector>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cassert>
#include <inttypes.h>

#include "utap/abstractbuilder.h"

using namespace UTAP;

void ParserBuilder::handleWarning(const char *msg, ...)
{
    char str[256];
    va_list ap;
    va_start(ap, msg);
    vsnprintf(str, 256, msg, ap);
    va_end(ap);

    handleWarning(std::string(str));
}

void ParserBuilder::handleError(const char *msg, ...)
{
    char str[256];
    va_list ap;
    va_start(ap, msg);
    vsnprintf(str, 256, msg, ap);
    va_end(ap);

    handleError(std::string(str));
}

AbstractBuilder::AbstractBuilder()
{
}

void AbstractBuilder::setPosition(uint32_t start, uint32_t end)
{
    position.start = start;
    position.end = end;
}

bool AbstractBuilder::isType(const char*)
{
    return false;
}

void AbstractBuilder::typeDuplicate()
{
    throw NotSupportedException("typeDuplicate is not supported");
}

void AbstractBuilder::typePop()
{
    throw NotSupportedException("typePop is not supported");
}

void AbstractBuilder::typeBool(PREFIX) 
{
    throw NotSupportedException("typeBool is not supported");
}

void AbstractBuilder::typeInt(PREFIX)
{
    throw NotSupportedException("typeInt is not supported");
}

void AbstractBuilder::typeBoundedInt(PREFIX)
{
    throw NotSupportedException("typeBoundedInt is not supported");
}

void AbstractBuilder::typeChannel(PREFIX)
{
    throw NotSupportedException("typeChannel is not supported");
}

void AbstractBuilder::typeClock()
{
    throw NotSupportedException("typeClock is not supported");
}

void AbstractBuilder::typeVoid()
{
    throw NotSupportedException("typeVoid is not supported");
}

void AbstractBuilder::typeScalar(PREFIX)
{
    throw NotSupportedException("typeScalar is not supported");
}

void AbstractBuilder::typeName(PREFIX, const char* name)
{
    throw NotSupportedException("typeName is not supported");
}

void AbstractBuilder::typeStruct(PREFIX, uint32_t fields)
{
    throw NotSupportedException("typeStruct is not supported");
}

void AbstractBuilder::typeArrayOfSize(size_t)
{
    throw NotSupportedException("typeArrayOfSize is not supported");
}

void AbstractBuilder::typeArrayOfType(size_t)
{
    throw NotSupportedException("typeArrayOfType is not supported");
}

void AbstractBuilder::structField(const char* name)
{
    throw NotSupportedException("structField is not supported");
}

void AbstractBuilder::declTypeDef(const char* name)
{
    throw NotSupportedException("declTypeDef is not supported");
}

void AbstractBuilder::declVar(const char* name, bool init)
{
    throw NotSupportedException("declVar is not supported");
}

void AbstractBuilder::declInitialiserList(uint32_t num)
{
    throw NotSupportedException("declInitialieserList is not supported");
}

void AbstractBuilder::declFieldInit(const char* name)
{
    throw NotSupportedException("declFieldInit is not supported");
}

void AbstractBuilder::declProgress(bool)
{
    throw NotSupportedException("declProgress is not supported");
}

void AbstractBuilder::declParameter(const char* name, bool)
{
    throw NotSupportedException("declParameter is not supported");
}
    
void AbstractBuilder::declFuncBegin(const char* name)
{
    throw NotSupportedException("declFuncBegin is not supported");
}

void AbstractBuilder::declFuncEnd()
{
    throw NotSupportedException("declFuncEnd is not supported");
}

void AbstractBuilder::procBegin(const char* name)
{
    throw NotSupportedException("procBegin is not supported");
}

void AbstractBuilder::procEnd()
{
    throw NotSupportedException("procEnd is not supported");
}

void AbstractBuilder::procState(const char* name, bool hasInvariant)
{
    throw NotSupportedException("procState is not supported");
}

void AbstractBuilder::procStateCommit(const char* name)
{
    throw NotSupportedException("procStateCommit is not supported");
}

void AbstractBuilder::procStateUrgent(const char* name)
{
    throw NotSupportedException("procStateUrgent is not supported");
}

void AbstractBuilder::procStateInit(const char* name)
{
    throw NotSupportedException("procStateInit is not supported");
}

void AbstractBuilder::procEdgeBegin(const char* from, const char* to, const bool control)
    
{
    throw NotSupportedException("procEdgeBegin is not supported");
}

void AbstractBuilder::procEdgeEnd(const char* from, const char* to)
{
    throw NotSupportedException("procEdgeEnd is not supported");
}
 
void AbstractBuilder::procSelect(const char* id)
{
    throw NotSupportedException("procSelect is not supported");
}

void AbstractBuilder::procGuard()
{
    throw NotSupportedException("procGuard is not supported");
}

void AbstractBuilder::procSync(Constants::synchronisation_t type)
{
    throw NotSupportedException("procSync is not supported");
}

void AbstractBuilder::procUpdate()
{
    throw NotSupportedException("procUpdate is not supported");
}
    
void AbstractBuilder::blockBegin()
{
    throw NotSupportedException("procBegin is not supported");
}

void AbstractBuilder::blockEnd()
{
    throw NotSupportedException("procEnd is not supported");
}

void AbstractBuilder::emptyStatement()
{
    throw NotSupportedException("emptyStatement is not supported");
}

void AbstractBuilder::forBegin()
{
    throw NotSupportedException("forBegin is not supported");
}

void AbstractBuilder::forEnd()
{
    throw NotSupportedException("forEnd is not supported");
}

void AbstractBuilder::iterationBegin (const char *name)
{
    throw NotSupportedException("iterationBegin is not supported");
}

void AbstractBuilder::iterationEnd (const char *name)
{
    throw NotSupportedException("iterationEnd is not supported");
}

void AbstractBuilder::whileBegin()
{
    throw NotSupportedException("whileBegin is not supported");
}

void AbstractBuilder::whileEnd()
{
    throw NotSupportedException("whileEnd is not supported");
}

void AbstractBuilder::doWhileBegin()
{
    throw NotSupportedException("doWhileBegin is not supported");
}

void AbstractBuilder::doWhileEnd()
{
    throw NotSupportedException("doWhileEnd is not supported");
}

void AbstractBuilder::ifBegin()
{
    throw NotSupportedException("ifBegin is not supported");
}

void AbstractBuilder::ifElse()
{
    throw NotSupportedException("ifElse is not supported");
}

void AbstractBuilder::ifEnd(bool)
{
    throw NotSupportedException("ifEnd is not supported");
}

void AbstractBuilder::breakStatement()
{
    throw NotSupportedException("breakStatement is not supported");
}

void AbstractBuilder::continueStatement()
{
    throw NotSupportedException("continueStatement is not supported");
}

void AbstractBuilder::switchBegin()
{
    throw NotSupportedException("switchStatement is not supported");
}

void AbstractBuilder::switchEnd()
{
    throw NotSupportedException("switchEnd is not supported");
}

void AbstractBuilder::caseBegin()
{
    throw NotSupportedException("caseBegin is not supported");

}

void AbstractBuilder::caseEnd()
{
    throw NotSupportedException("caseEnd is not supported");
}

void AbstractBuilder::defaultBegin()
{
    throw NotSupportedException("defaultBegin is not supported");
}

void AbstractBuilder::defaultEnd()
{
    throw NotSupportedException("defaultEnd is not supported");
}

void AbstractBuilder::exprStatement()
{
    throw NotSupportedException("exprStatement is not supported");
}

void AbstractBuilder::returnStatement(bool)
{
    throw NotSupportedException("returnStatement is not supported");
}

void AbstractBuilder::assertStatement()
{
    throw NotSupportedException("assertStatement is not supported");
}
    
void AbstractBuilder::exprTrue()
{
    throw NotSupportedException("exprTrue is not supported");
}

void AbstractBuilder::exprFalse()
{
    throw NotSupportedException("exprFalse is not supported");
}

void AbstractBuilder::exprId(const char * varName)
{
    throw NotSupportedException("exprId is not supported");
}

void AbstractBuilder::exprNat(int32_t)
{
    throw NotSupportedException("exprNar is not supported");
}

void AbstractBuilder::exprCallBegin()
{
    throw NotSupportedException("exprCallBegin is not supported");
}

void AbstractBuilder::exprCallEnd(uint32_t n)
{
    throw NotSupportedException("exprCallEnd is not supported");
}

void AbstractBuilder::exprArray()
{
    throw NotSupportedException("exprArray is not supported");
}

void AbstractBuilder::exprPostIncrement()
{
    throw NotSupportedException("exprPostIncrement is not supported");
}

void AbstractBuilder::exprPreIncrement()
{
    throw NotSupportedException("exprPreIncrement is not supported");
}

void AbstractBuilder::exprPostDecrement()
{
    throw NotSupportedException("exprPostDecrement is not supported");
}

void AbstractBuilder::exprPreDecrement()
{
    throw NotSupportedException("exprPreDecrement is not supported");
}

void AbstractBuilder::exprAssignment(Constants::kind_t op)
{
    throw NotSupportedException("exprAssignment is not supported");
}

void AbstractBuilder::exprUnary(Constants::kind_t unaryop)
{
    throw NotSupportedException("exprUnary is not supported");
}

void AbstractBuilder::exprBinary(Constants::kind_t binaryop)
{
    throw NotSupportedException("exprBinary is not supported");
}

void AbstractBuilder::exprTernary(Constants::kind_t ternaryop, bool firstMissing)
{
    throw NotSupportedException("exprTernary is not supported");
}

void AbstractBuilder::exprInlineIf()
{
    throw NotSupportedException("exprInlineIf is not supported");
}

void AbstractBuilder::exprComma()
{
    throw NotSupportedException("exprComma is not supported");
}

void AbstractBuilder::exprDot(const char *)
{
    throw NotSupportedException("exprDot is not supported");
}

void AbstractBuilder::exprDeadlock()
{
    throw NotSupportedException("exprDeadlock is not supported");
}

void AbstractBuilder::exprForAllBegin(const char *name)
{
    throw NotSupportedException("exprForAllBegin is not supported");
}

void AbstractBuilder::exprForAllEnd(const char *name)
{
    throw NotSupportedException("exprForAllEnd is not supported");
}

void AbstractBuilder::exprExistsBegin(const char *name)
{
    throw NotSupportedException("exprExistsBegin is not supported");
}

void AbstractBuilder::exprExistsEnd(const char *name)
{
    throw NotSupportedException("exprExistsEnd is not supported");
}

void AbstractBuilder::instantiationBegin(const char*, size_t, const char*)
{
    throw NotSupportedException("instantiationBegin is not supported");
}

void AbstractBuilder::instantiationEnd(const char *, size_t, const char *, size_t)
{
    throw NotSupportedException("instantiationEnd is not supported");
}

void AbstractBuilder::process(const char*)
{
    throw NotSupportedException("process is not supported");
}

void AbstractBuilder::done()
{
}

void AbstractBuilder::property()
{
    throw NotSupportedException("property");
}

void AbstractBuilder::paramProperty(size_t, Constants::kind_t)
{
    throw NotSupportedException("paramProperty");
}

void AbstractBuilder::declParamId(const char *)
{
    throw NotSupportedException("declParamId");
}

void AbstractBuilder::beforeUpdate()
{
    throw NotSupportedException("beforeUpdate");
}

void AbstractBuilder::afterUpdate()
{
    throw NotSupportedException("afterUpdate");
}

void AbstractBuilder::incChanPriority()
{
    throw NotSupportedException("incChanPriority");
}

void AbstractBuilder::incProcPriority()
{
    throw NotSupportedException("incProcPriority");
}

void AbstractBuilder::defaultChanPriority()
{
    throw NotSupportedException("defaultChanPriority");
}

void AbstractBuilder::procPriority(const char*)
{
    throw NotSupportedException("procPriority");
}

void AbstractBuilder::chanPriority()
{
    throw NotSupportedException("chanPriority");
}

void AbstractBuilder::ssQueryBegin()
{
    throw NotSupportedException("ssQueryBegin");
}

void AbstractBuilder::ssQueryEnd()
{
    throw NotSupportedException("ssQueryEnd");
}
