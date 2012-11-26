// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-

/* libutap - Uppaal Timed Automata Parser.
   Copyright (C) 2002 Uppsala University and Aalborg University.
   
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

#ifndef UTAP_STATEMENT_H
#define UTAP_STATEMENT_H

#include "utap/expression.h"
#include "utap/symbols.h"
#include "utap/system.h"

namespace UTAP
{
    class StatementVisitor;

    class Statement 
    {
    public:
        virtual ~Statement() {};
        virtual int32_t accept(StatementVisitor *visitor) = 0;
        virtual bool returns() = 0;
    protected:
        Statement();
    };

    class EmptyStatement: public Statement 
    {
    public:
        EmptyStatement();
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();
    };

    class ExprStatement: public Statement 
    {
    public:
        expression_t expr;
        ExprStatement(expression_t);
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();
    };

    class AssertStatement: public Statement 
    {
    public:
        expression_t expr;
        AssertStatement(expression_t);
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();
    };

    class ForStatement: public Statement 
    {
    public:
        expression_t init;
        expression_t cond;
        expression_t step;
        Statement *stat;
        ForStatement(expression_t, expression_t, expression_t, Statement*);
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();
    };

    /**
     * Statement class for the iterator loop-construction.
     */
    class IterationStatement: public Statement
    {
    protected:
        frame_t frame;
    public:
        symbol_t symbol;
        Statement *stat;
        IterationStatement(symbol_t, frame_t, Statement *);
         frame_t getFrame() { return frame; }
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();
     };

    class WhileStatement: public Statement 
    {
    public:
        expression_t cond;
        Statement *stat;
        WhileStatement(expression_t, Statement*);
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();
    };

    class DoWhileStatement: public Statement 
    {
    public:
        Statement *stat;
        expression_t cond;
        DoWhileStatement(Statement*, expression_t);
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();
    };

    class BlockStatement: public Statement, public declarations_t 
    {
    public:
        typedef std::vector<Statement *>::const_iterator const_iterator;
        typedef std::vector<Statement *>::iterator iterator;
    protected:
        std::vector<Statement*> stats;
        frame_t frame;
    public:
        BlockStatement(frame_t);
        virtual ~BlockStatement();
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();

         frame_t getFrame() { return frame; }
        void push_stat(Statement* stat);
        Statement* pop_stat();
        Statement* back();
        const_iterator begin() const;
        const_iterator end() const;
        iterator begin();
        iterator end();
    };

    class SwitchStatement: public BlockStatement
    {
    public:
        expression_t cond;
        SwitchStatement(frame_t, expression_t);
        virtual int32_t accept(StatementVisitor *visitor);  
        virtual bool returns();
    };

    class CaseStatement: public BlockStatement 
    {
    public:
        expression_t cond;
        CaseStatement(frame_t, expression_t);
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();
    };

    class DefaultStatement: public BlockStatement 
    {
    public:
        DefaultStatement(frame_t);
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();
    };

    class IfStatement: public Statement 
    {
    public:
        expression_t cond;
        Statement *trueCase;
        Statement *falseCase;
        IfStatement(expression_t, Statement*, 
                    Statement* falseStat=NULL);
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();
    };

    class BreakStatement: public Statement 
    {
    public:
        BreakStatement();
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();
    };

    class ContinueStatement: public Statement 
    {
    public:
        ContinueStatement();
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();
    };

    class ReturnStatement: public Statement 
    {
    public:
        expression_t value;
        ReturnStatement();
        ReturnStatement(expression_t);
        virtual int32_t accept(StatementVisitor *visitor);
        virtual bool returns();
    };

    class StatementVisitor
    {
    public:
        virtual ~StatementVisitor() {};
        virtual int32_t visitEmptyStatement(EmptyStatement *stat)=0;
        virtual int32_t visitExprStatement(ExprStatement *stat)=0;
        virtual int32_t visitAssertStatement(AssertStatement *stat)=0;
        virtual int32_t visitForStatement(ForStatement *stat)=0;
        virtual int32_t visitIterationStatement(IterationStatement *stat)=0;
        virtual int32_t visitWhileStatement(WhileStatement *stat)=0;
        virtual int32_t visitDoWhileStatement(DoWhileStatement *stat)=0;
        virtual int32_t visitBlockStatement(BlockStatement *stat)=0;
        virtual int32_t visitSwitchStatement(SwitchStatement *stat)=0;
        virtual int32_t visitCaseStatement(CaseStatement *stat)=0;
        virtual int32_t visitDefaultStatement(DefaultStatement *stat)=0;
        virtual int32_t visitIfStatement(IfStatement *stat)=0;
        virtual int32_t visitBreakStatement(BreakStatement *stat)=0;
        virtual int32_t visitContinueStatement(ContinueStatement *stat)=0;
        virtual int32_t visitReturnStatement(ReturnStatement *stat)=0;
    };

    class AbstractStatementVisitor : public StatementVisitor
    {
    protected:
        virtual int32_t visitStatement(Statement *stat);
    public:
        virtual int32_t visitEmptyStatement(EmptyStatement *stat);
        virtual int32_t visitExprStatement(ExprStatement *stat);
        virtual int32_t visitAssertStatement(AssertStatement *stat);
        virtual int32_t visitForStatement(ForStatement *stat);
        virtual int32_t visitIterationStatement(IterationStatement *stat);
        virtual int32_t visitWhileStatement(WhileStatement *stat);
        virtual int32_t visitDoWhileStatement(DoWhileStatement *stat);
        virtual int32_t visitBlockStatement(BlockStatement *stat);
        virtual int32_t visitSwitchStatement(SwitchStatement *stat);
        virtual int32_t visitCaseStatement(CaseStatement *stat);
        virtual int32_t visitDefaultStatement(DefaultStatement *stat);
        virtual int32_t visitIfStatement(IfStatement *stat);
        virtual int32_t visitBreakStatement(BreakStatement *stat);
        virtual int32_t visitContinueStatement(ContinueStatement *stat);
        virtual int32_t visitReturnStatement(ReturnStatement *stat);
    };

    class ExpressionVisitor : public AbstractStatementVisitor
    {
    protected:
        virtual void visitExpression(expression_t) = 0;
    public:
        virtual int32_t visitExprStatement(ExprStatement *stat);
        virtual int32_t visitAssertStatement(AssertStatement *stat);
        virtual int32_t visitForStatement(ForStatement *stat);
        virtual int32_t visitWhileStatement(WhileStatement *stat);
        virtual int32_t visitDoWhileStatement(DoWhileStatement *stat);
        virtual int32_t visitBlockStatement(BlockStatement *stat);
        virtual int32_t visitSwitchStatement(SwitchStatement *stat);
        virtual int32_t visitCaseStatement(CaseStatement *stat);
        virtual int32_t visitDefaultStatement(DefaultStatement *stat);
        virtual int32_t visitIfStatement(IfStatement *stat);
        virtual int32_t visitReturnStatement(ReturnStatement *stat);
    };

    class CollectChangesVisitor : public ExpressionVisitor
    {
    protected:
        virtual void visitExpression(expression_t);
        std::set<symbol_t> &changes;
    public:
        CollectChangesVisitor(std::set<symbol_t> &);
    };

    class CollectDependenciesVisitor : public ExpressionVisitor
    {
    protected:
        virtual void visitExpression(expression_t);
        std::set<symbol_t> &dependencies;
    public:
        CollectDependenciesVisitor(std::set<symbol_t> &);
    };

}
#endif
