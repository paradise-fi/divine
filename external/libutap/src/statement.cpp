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

#include <cassert>

#include "utap/statement.h"

using namespace UTAP;

Statement::Statement()
{

}


EmptyStatement::EmptyStatement() : Statement()
{

}

int32_t EmptyStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitEmptyStatement(this); 
}

bool EmptyStatement::returns()
{
    return false;
}

ExprStatement::ExprStatement(expression_t expr)
    : Statement(), expr(expr)
{

}

int32_t ExprStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitExprStatement(this);
}

bool ExprStatement::returns()
{
    return false;
}


AssertStatement::AssertStatement(expression_t expr)
    : Statement(), expr(expr)
{

}

int32_t AssertStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitAssertStatement(this);
}

bool AssertStatement::returns()
{
    return false;
}

ForStatement::ForStatement(expression_t init,
   expression_t cond, expression_t step, Statement* _stat)
    : Statement(), init(init), cond(cond), step(step), stat(_stat) 
{ 
    assert(_stat!=NULL); 
}

int32_t ForStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitForStatement(this);
}

bool ForStatement::returns()
{
    return false;
}

IterationStatement::IterationStatement (symbol_t sym, frame_t f, Statement *s)
{
    this->symbol = sym;
    this->frame = f;
    this->stat = s;
}

int32_t IterationStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitIterationStatement(this);
}

bool IterationStatement::returns()
{
    return false;
}

WhileStatement::WhileStatement(expression_t cond,
                               Statement* _stat)
    : Statement(), cond(cond), stat(_stat) 
{ 
    assert(_stat!=NULL); 
}

int32_t WhileStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitWhileStatement(this);
}

bool WhileStatement::returns()
{
    return false;
}

DoWhileStatement::DoWhileStatement(Statement* _stat,
                                   expression_t cond)
    : Statement(), stat(_stat), cond(cond) 
{
    assert(_stat!=NULL);
}

int32_t DoWhileStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitDoWhileStatement(this);
}

bool DoWhileStatement::returns()
{
    return stat->returns();
}

BlockStatement::BlockStatement(frame_t frame)
    : Statement()
{
    this->frame = frame;
}

BlockStatement::~BlockStatement()
{
    while(!stats.empty()) delete pop_stat();
}

void BlockStatement::push_stat(Statement* stat)
{
    assert(stat!=NULL);
    stats.push_back(stat);
}

BlockStatement::const_iterator BlockStatement::begin() const
{
    return stats.begin();
}

BlockStatement::const_iterator BlockStatement::end() const
{
    return stats.end();
}

BlockStatement::iterator BlockStatement::begin()
{
    return stats.begin();
}

BlockStatement::iterator BlockStatement::end() 
{
    return stats.end();
}

Statement *BlockStatement::back() 
{
    assert(!stats.empty()); 
    return stats.back();
}

Statement* BlockStatement::pop_stat()
{ 
    Statement* st = back();
    stats.pop_back();
    return st;
}

int32_t BlockStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitBlockStatement(this);
}

bool BlockStatement::returns()
{
    return begin() != end() && back()->returns();
}

SwitchStatement::SwitchStatement(frame_t frame, expression_t cond)
    : BlockStatement(frame), cond(cond)
{

}

int32_t SwitchStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitSwitchStatement(this);
}

bool SwitchStatement::returns()
{
    return false;
}

CaseStatement::CaseStatement(frame_t frame, expression_t cond)
    : BlockStatement(frame), cond(cond)
{

}

int32_t CaseStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitCaseStatement(this);
}

bool CaseStatement::returns()
{
    return false;
}

DefaultStatement::DefaultStatement(frame_t frame)
    : BlockStatement(frame)
{

};

int32_t DefaultStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitDefaultStatement(this);
}

bool DefaultStatement::returns()
{
    return false;
}

IfStatement::IfStatement(expression_t cond,
                         Statement* _true, Statement* _false)
    : Statement(), cond(cond), trueCase(_true), falseCase(_false) 
{
    assert(_true!=NULL);
}

int32_t IfStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitIfStatement(this);
}

bool IfStatement::returns()
{
    return trueCase->returns() && (falseCase == NULL || falseCase->returns());
}

BreakStatement::BreakStatement() : Statement()
{

}

int32_t BreakStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitBreakStatement(this);
}

bool BreakStatement::returns()
{
    return false;
}

ContinueStatement::ContinueStatement() : Statement()
{

}

int32_t ContinueStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitContinueStatement(this);
}

bool ContinueStatement::returns()
{
    return false;
}

ReturnStatement::ReturnStatement()
    : Statement()
{

}

ReturnStatement::ReturnStatement(expression_t value)
    : Statement(), value(value)
{

}

int32_t ReturnStatement::accept(StatementVisitor *visitor)
{
    return visitor->visitReturnStatement(this);
}

bool ReturnStatement::returns()
{
    return true;
}

int32_t AbstractStatementVisitor::visitStatement(Statement *stat)
{
    return 0;
}

int32_t AbstractStatementVisitor::visitEmptyStatement(EmptyStatement *stat)
{
    return visitStatement(stat);
}

int32_t AbstractStatementVisitor::visitExprStatement(ExprStatement *stat)
{
    return visitStatement(stat);
}

int32_t AbstractStatementVisitor::visitAssertStatement(AssertStatement *stat)
{
    return visitStatement(stat);
}

int32_t AbstractStatementVisitor::visitForStatement(ForStatement *stat)
{
    return stat->stat->accept(this);
}

int32_t AbstractStatementVisitor::visitIterationStatement(IterationStatement *stat)
{
    return stat->stat->accept(this);
}

int32_t AbstractStatementVisitor::visitWhileStatement(WhileStatement *stat)
{
    return stat->stat->accept(this);
}

int32_t AbstractStatementVisitor::visitDoWhileStatement(DoWhileStatement *stat)
{
    return stat->stat->accept(this);
}

int32_t AbstractStatementVisitor::visitBlockStatement(BlockStatement *stat)
{
    int result = 0;
    BlockStatement::iterator i;
    for (i = stat->begin(); i != stat->end(); i++)
    {
        result = (*i)->accept(this);
    }
    return result;
}

int32_t AbstractStatementVisitor::visitSwitchStatement(SwitchStatement *stat)
{
    return visitBlockStatement(stat);
}

int32_t AbstractStatementVisitor::visitCaseStatement(CaseStatement *stat)
{
    return visitBlockStatement(stat);
}

int32_t AbstractStatementVisitor::visitDefaultStatement(DefaultStatement *stat)
{
    return visitBlockStatement(stat);
}

int32_t AbstractStatementVisitor::visitIfStatement(IfStatement *stat)
{
    if (stat->falseCase)
    {
        stat->trueCase->accept(this);
        return stat->falseCase->accept(this);
    } 
    else
    {
        return stat->trueCase->accept(this);
    }
}

int32_t AbstractStatementVisitor::visitBreakStatement(BreakStatement *stat)
{
    return visitStatement(stat);
}

int32_t AbstractStatementVisitor::visitContinueStatement(ContinueStatement *stat)
{
    return visitStatement(stat);
}

int32_t AbstractStatementVisitor::visitReturnStatement(ReturnStatement *stat)
{
    return visitStatement(stat);
}

int32_t ExpressionVisitor::visitExprStatement(ExprStatement *stat)
{
    visitExpression(stat->expr);
    return 0;
}

int32_t ExpressionVisitor::visitAssertStatement(AssertStatement *stat)
{
    visitExpression(stat->expr);
    return 0;
}

int32_t ExpressionVisitor::visitForStatement(ForStatement *stat)
{
    visitExpression(stat->init); 
    visitExpression(stat->cond);
    visitExpression(stat->step);
    return stat->stat->accept(this);
}

int32_t ExpressionVisitor::visitWhileStatement(WhileStatement *stat)
{
    visitExpression(stat->cond);
    return stat->stat->accept(this);
}

int32_t ExpressionVisitor::visitDoWhileStatement(DoWhileStatement *stat)
{
    visitExpression(stat->cond);
    return stat->stat->accept(this);
}

int32_t ExpressionVisitor::visitBlockStatement(BlockStatement *stat)
{
    /* Visit variable initialisers.
     */
    frame_t vars = stat->getFrame();
    for (size_t i = 0; i < vars.getSize(); i++)
    {
        if (vars[i].getData())
        {
            // REVISIT: This will only work if vars[i] is a variable!
            visitExpression(((variable_t *)vars[i].getData())->expr);
        }        
    }

    /* Visit statements.
     */
    BlockStatement::iterator s;
    for (s = stat->begin(); s != stat->end(); ++s) 
    {
        (*s)->accept(this);
    }
    return 0;
}

int32_t ExpressionVisitor::visitSwitchStatement(SwitchStatement *stat)
{
    visitExpression(stat->cond);
    return visitBlockStatement(stat);
}

int32_t ExpressionVisitor::visitCaseStatement(CaseStatement *stat)
{
    visitExpression(stat->cond);
    return visitBlockStatement(stat);
}

int32_t ExpressionVisitor::visitDefaultStatement(DefaultStatement *stat)
{
    return visitBlockStatement(stat);
}

int32_t ExpressionVisitor::visitIfStatement(IfStatement *stat)
{
    visitExpression(stat->cond);
    stat->trueCase->accept(this);
    if (stat->falseCase) 
    {
        stat->falseCase->accept(this);
    }
    return 0;
}

int32_t ExpressionVisitor::visitReturnStatement(ReturnStatement *stat)
{
    visitExpression(stat->value);
    return 0;
}


CollectChangesVisitor::CollectChangesVisitor(std::set<symbol_t> &changes)
    : changes(changes)
{

}

void CollectChangesVisitor::visitExpression(expression_t expr)
{
    expr.collectPossibleWrites(changes);
}

CollectDependenciesVisitor::CollectDependenciesVisitor(
    std::set<symbol_t> &dependencies)
    : dependencies(dependencies)
{

}

void CollectDependenciesVisitor::visitExpression(expression_t expr)
{
    expr.collectPossibleReads(dependencies);
}
