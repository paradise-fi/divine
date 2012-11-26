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
#include <algorithm>
#include <stdexcept>

#include "utap/builder.h"
#include "utap/system.h"
#include "utap/expression.h"

using namespace UTAP;
using namespace Constants;

using std::vector;
using std::set;
using std::min;
using std::max;
using std::pair;
using std::make_pair;
using std::map;
using std::ostream;
using Constants::kind_t;
using Constants::synchronisation_t;

struct expression_t::expression_data
{
    int count;                  /**< Reference counter */
    position_t position;        /**< The position of the expression */
    kind_t kind;                /**< The kind of the node */
    union 
    {
        int32_t value;          /**< The value of the node */
        int32_t index;
        synchronisation_t sync; /**< The sync value of the node  */
    };
    symbol_t symbol;            /**< The symbol of the node */
    type_t type;                /**< The type of the expression */
    expression_t *sub;          /**< Subexpressions */
};

expression_t::expression_t()
{
    data = NULL;
}

expression_t::expression_t(kind_t kind, const position_t &pos)
{
    data = new expression_data;
    data->count = 1;
    data->kind = kind;
    data->position = pos;
    data->value = 0;
    data->sub = NULL;
}

expression_t::expression_t(const expression_t &e)
{
    data = e.data;
    if (data)
    {
        data->count++;
    }
}

expression_t expression_t::clone() const
{
    expression_t expr(data->kind, data->position);
    expr.data->value = data->value;
    expr.data->type = data->type;
    expr.data->symbol = data->symbol;
    if (data->sub)
    {
        expr.data->sub = new expression_t[getSize()];
        std::copy(data->sub, data->sub + getSize(), expr.data->sub);
    }
    return expr;
}

expression_t expression_t::subst(symbol_t symbol, expression_t expr) const
{
    if (empty())
    {
        return *this;
    }
    else if (getKind() == IDENTIFIER && getSymbol() == symbol)
    {
        return expr;
    }
    else if (getSize() == 0)
    {
        return *this;
    }
    else
    {
        expression_t e = clone();
        for (size_t i = 0; i < getSize(); i++)
        {
            e[i] = e[i].subst(symbol, expr);
        }
        return e;
    }
}

expression_t::~expression_t()
{
    if (data) 
    {
        data->count--;
        if (data->count == 0) 
        {
            delete[] data->sub;
            delete data;
        }
    }
}

kind_t expression_t::getKind() const
{
    assert(data);
    return data->kind;
}

const position_t &expression_t::getPosition() const
{
    assert(data);
    return data->position;
}

size_t expression_t::getSize() const
{
    if (empty())
    {
        return 0;
    }

    switch (data->kind) 
    {
    case MINUS:
    case PLUS:
    case MULT:
    case DIV:
    case MOD:
    case BIT_AND:
    case BIT_OR:
    case BIT_XOR:
    case BIT_LSHIFT:
    case BIT_RSHIFT:
    case AND:
    case OR:
    case LT:
    case LE:
    case EQ:
    case NEQ:
    case GE:
    case GT:
    case MIN:
    case MAX:
        return 2;
    case UNARY_MINUS:
    case NOT:
        return 1;
        break;
    case IDENTIFIER:
    case CONSTANT:
        return 0;
        break;
    case LIST:
        return data->value;
        break;
    case ARRAY:
        return 2;
        break;
    case INLINEIF:
        return 3;
        break;
    case DOT:
        return 1;
    case COMMA:
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
        return 2;
    case SYNC:
        return 1;
    case DEADLOCK:
        return 0;
    case PREINCREMENT:
    case POSTINCREMENT:
    case PREDECREMENT:
    case POSTDECREMENT:
        return 1;
    case RATE:
        return 1;
    case FUNCALL:
        return data->value;
    case EG:
    case AG:
    case EF:
    case AF:
    case EF_R:
    case AG_R:
        return 1;
    case EG2:
    case AG2:
    case EF2:
    case AF2:
        return 2;
    case LEADSTO:
        return 2; 
    case A_UNTIL:
    case A_WEAKUNTIL:
        return 3;
    case FORALL:
    case EXISTS:
        return 2;
    case CONTROL:
    case EF_CONTROL:
        return 1;
    case CONTROL_TOPT:
        return 3;
    default:
        assert(0);
        return 0;
    }
    // FIXME: add INF and SUP
}
        
type_t expression_t::getType() const
{
    assert(data);
    return data->type;
}

void expression_t::setType(type_t type)
{
    assert(data);
    data->type = type;
}

int32_t expression_t::getValue() const
{
    assert(data && data->kind == CONSTANT);
    return data->value;
}

int32_t expression_t::getIndex() const
{
    assert(data && data->kind == DOT);
    return data->index;
}

synchronisation_t expression_t::getSync() const
{
    assert(data && data->kind == SYNC);
    return data->sync;
}

expression_t &expression_t::operator[](uint32_t i)
{
    assert(data && 0 <= i && i < getSize());
    return data->sub[i];
}
        
const expression_t expression_t::operator[](uint32_t i) const
{
    assert(data && 0 <= i && i < getSize());
    return data->sub[i];
}

expression_t &expression_t::get(uint32_t i)
{
    assert(data && 0 <= i && i < getSize());
    return data->sub[i];
}

const expression_t &expression_t::get(uint32_t i) const
{
    assert(data && 0 <= i && i < getSize());
    return data->sub[i];
}

bool expression_t::empty() const
{
    return data == NULL;
}

/** Two expressions are identical iff all the sub expressions
    are identical and if the kind, value and symbol of the 
    root are identical. */
bool expression_t::equal(const expression_t &e) const
{
    if (data == e.data)
    {
        return true;
    }

    if (getSize() != e.getSize()
        || data->kind != e.data->kind
        || data->value != e.data->value
        || data->symbol != e.data->symbol)
    {
        return false;
    }

    for (uint32_t i = 0; i < getSize(); i++)
    {
        if (!data->sub[i].equal(e[i]))
        {
            return false;
        }
    }

    return true;
}

expression_t &expression_t::operator=(const expression_t &e)
{
    if (data) 
    {
        data->count--;
        if (data->count == 0) 
        {
            delete[] data->sub;
            delete data;
        }
    }
    data = e.data;
    if (data)
    {
        data->count++;
    }
    return *this;
}
        
/**
   Returns the symbol of a variable reference. The expression must be
   a left-hand side value. The symbol returned is the symbol of the
   variable the expression if resulting in a reference to. NOTE: In
   case of inline if, the symbol referenced by the 'true' part is
   returned.
*/
symbol_t expression_t::getSymbol() 
{
    return ((const expression_t*)this)->getSymbol();
}

const symbol_t expression_t::getSymbol() const
{
    assert(data);

    switch (getKind()) 
    {
    case IDENTIFIER:
        return data->symbol;
      
    case DOT:
        return get(0).getSymbol();
      
    case ARRAY:
        return get(0).getSymbol();
      
    case PREINCREMENT:
    case PREDECREMENT:
        return get(0).getSymbol();
    
    case INLINEIF:
        return get(1).getSymbol();
      
    case COMMA:
        return get(1).getSymbol();

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
        return get(0).getSymbol();

    case SYNC:
        return get(0).getSymbol();

    case FUNCALL:
        return get(0).getSymbol();

    default:
        return symbol_t();
    }
}

void expression_t::getSymbols(std::set<symbol_t> &symbols) const
{
    if (empty()) 
    {
        return;
    }

    switch (getKind()) 
    {
    case IDENTIFIER:
        symbols.insert(data->symbol);
        break;

    case DOT:
        get(0).getSymbols(symbols);
        break;
      
    case ARRAY:
        get(0).getSymbols(symbols);
        break;
      
    case PREINCREMENT:
    case PREDECREMENT:
        get(0).getSymbols(symbols);
        break;
    
    case INLINEIF:
        get(1).getSymbols(symbols);
        get(2).getSymbols(symbols);
        break;
      
    case COMMA:
        get(1).getSymbols(symbols);
        break;

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
        get(0).getSymbols(symbols);
        break;

    case SYNC:
        get(0).getSymbols(symbols);
        break;        

    default:
        // Do nothing
        break;
    }
}

/** Returns true if expr might be a reference to a symbol in the
    set. */
bool expression_t::isReferenceTo(const std::set<symbol_t> &symbols) const
{
    std::set<symbol_t> s;
    getSymbols(s);    
    return find_first_of(symbols.begin(), symbols.end(), s.begin(), s.end()) 
        != symbols.end();
}

bool expression_t::changesVariable(const std::set<symbol_t> &symbols) const
{
    std::set<symbol_t> changes;
    collectPossibleWrites(changes);
    return find_first_of(symbols.begin(), symbols.end(),
                         changes.begin(), changes.end()) != symbols.end();
}

bool expression_t::changesAnyVariable() const
{
    std::set<symbol_t> changes;
    collectPossibleWrites(changes);
    return !changes.empty();
}

bool expression_t::dependsOn(const std::set<symbol_t> &symbols) const
{
    std::set<symbol_t> dependencies;
    collectPossibleReads(dependencies);
    return find_first_of(
        symbols.begin(), symbols.end(),
        dependencies.begin(), dependencies.end()) != symbols.end();
}

int expression_t::getPrecedence() const
{
    return getPrecedence(data->kind);
}

int expression_t::getPrecedence(kind_t kind) 
{
    switch (kind) 
    {
    case PLUS:
    case MINUS:
        return 70;
            
    case MULT:
    case DIV:
    case MOD:
        return 80;
            
    case BIT_AND:
        return 37;
            
    case BIT_OR:
        return 30;
            
    case BIT_XOR:
        return 35;

    case BIT_LSHIFT:
    case BIT_RSHIFT:
        return 60;
            
    case AND:
        return 25;
    case OR:
        return 20;

    case EQ:
    case NEQ:
        return 40;

    case MIN:
    case MAX:
        return 55;

    case LT:
    case LE:
    case GE:
    case GT:
        return 50;
            
    case IDENTIFIER:
    case CONSTANT:
    case DEADLOCK:
    case FUNCALL:
        return 110;
            
    case DOT:
    case ARRAY:
    case RATE:
        return 100;

    case UNARY_MINUS:
    case NOT:
        return 90;
            
    case INLINEIF:
        return 15;

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
        return 10;

    case FORALL:
    case EXISTS:
        return 8;

    case A_UNTIL:
    case A_WEAKUNTIL:
        return 7;

    case EF:
    case EG:
    case AF:
    case AG:
    case EF2:
    case EG2:
    case AF2:
    case AG2:
    case EF_R:
    case AG_R:
        return 6;

    case LEADSTO:
        return 5;

    case COMMA:
        return 4;

    case CONTROL:
    case EF_CONTROL:
    case CONTROL_TOPT:
        return 3;
        
    case SYNC:
        return 0;

    case PREDECREMENT:
    case POSTDECREMENT:
    case PREINCREMENT:
    case POSTINCREMENT:
        return 100;

    case LIST:
        return -1; // TODO

    default:
        throw std::runtime_error("Strange expression");
    }
    assert(0);
    return 0;
    // FIXME: add SUP and INF
}

static void ensure(char *&str, char *&end, int &size, int len)
{
    while (size <= len)
    {
        size *= 2;
    }
        
    char *nstr = new char[size];
    strncpy(nstr, str, end - str);
    end = nstr + (end - str);
    *end = 0;
    delete[] str;
    str = nstr;
}

static void append(char *&str, char *&end, int &size, const char *s)
{
    while (end < str + size && *s)
    {
        *(end++) = *(s++);
    }

    if (end == str + size) 
    {
        ensure(str, end, size, 2 * size);
        append(str, end, size, s);
    } 
    else 
    {
        *end = 0;
    }
}

static void append(char *&str, char *&end, int &size, char c)
{
    if (size - (end - str) < 2) 
    {
        ensure(str, end, size, size + 2);
    }
    *end = c;
    ++end;
    *end = 0;
}

void expression_t::toString(bool old, char *&str, char *&end, int &size) const
{
    char s[12];
    int precedence = getPrecedence();

    switch (data->kind) 
    {
    case PLUS:
    case MINUS:
    case MULT:
    case DIV:
    case MOD:
    case BIT_AND:
    case BIT_OR:
    case BIT_XOR:
    case BIT_LSHIFT:
    case BIT_RSHIFT:
    case AND:
    case OR:
    case LT:
    case LE:
    case EQ:
    case NEQ:
    case GE:
    case GT:
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
    case MIN:
    case MAX:

        if (precedence > get(0).getPrecedence())
        {
            append(str, end, size, '(');
        }
        get(0).toString(old, str, end, size);
        if (precedence > get(0).getPrecedence())
        {
            append(str, end, size, ')');
        }
        
        switch (data->kind) 
        {
        case PLUS:
            append(str, end, size, " + ");
            break;
        case MINUS:
            append(str, end, size, " - ");
            break;
        case MULT:
            append(str, end, size, " * ");
            break;
        case DIV:
            append(str, end, size, " / ");
            break;
        case MOD:
            append(str, end, size, " % ");
            break;
        case BIT_AND:
            append(str, end, size, " & ");
            break;
        case BIT_OR:
            append(str, end, size, " | ");
            break;
        case BIT_XOR:
            append(str, end, size, " ^ ");
            break;
        case BIT_LSHIFT:
            append(str, end, size, " << ");
            break;
        case BIT_RSHIFT:
            append(str, end, size, " >> ");
            break;
        case AND:
            append(str, end, size, " && ");
            break;
        case OR:
            append(str, end, size, " || ");
            break;
        case LT:
            append(str, end, size, " < ");
            break;
        case LE:
            append(str, end, size, " <= ");
            break;
        case EQ:
            append(str, end, size, " == ");
            break;
        case NEQ:
            append(str, end, size, " != ");
            break;
        case GE:
            append(str, end, size, " >= ");
            break;
        case GT:
            append(str, end, size, " > ");
            break;
        case ASSIGN:
            if (old)
            {
                append(str, end, size, " := ");
            }
            else
            {
                append(str, end, size, " = ");
            }
            break;
        case ASSPLUS:
            append(str, end, size, " += ");
            break;
        case ASSMINUS:
            append(str, end, size, " -= ");
            break;
        case ASSDIV:
            append(str, end, size, " /= ");
            break;
        case ASSMOD:
            append(str, end, size, " %= ");
            break;
        case ASSMULT:
            append(str, end, size, " *= ");
            break;
        case ASSAND:
            append(str, end, size, " &= ");
            break;
        case ASSOR:
            append(str, end, size, " |= ");
            break;
        case ASSXOR:
            append(str, end, size, " ^= ");
            break;
        case ASSLSHIFT:
            append(str, end, size, " <<= ");
            break;
        case ASSRSHIFT:
            append(str, end, size, " >>= ");
            break;
        case MIN:
            append(str, end, size, " <? ");
            break;
        case MAX:
            append(str, end, size, " >? ");
            break;
        default:
            assert(0);
        }
        
        if (precedence >= get(1).getPrecedence())
        {
            append(str, end, size, '(');
        }
        get(1).toString(old, str, end, size);
        if (precedence >= get(1).getPrecedence())
        {
            append(str, end, size, ')');
        }
        break;

    case IDENTIFIER:
        append(str, end, size, data->symbol.getName().c_str());
        break;
                
    case CONSTANT:
        snprintf(s, 12, "%d", data->value);
        append(str, end, size, s);
        break;

    case ARRAY:
        if (precedence > get(0).getPrecedence()) 
        {
            append(str, end, size, '(');
            get(0).toString(old, str, end, size);
            append(str, end, size, ')');
        }
        else 
        {
            get(0).toString(old, str, end, size);
        }
        append(str, end, size, '[');
        get(1).toString(old, str, end, size);
        append(str, end, size, ']');
        break;
            
    case UNARY_MINUS:
        append(str, end, size, '-');
        if (precedence > get(0).getPrecedence()) 
        {
            append(str, end, size, '(');
            get(0).toString(old, str, end, size);
            append(str, end, size, ')');
        }
        else 
        {
            get(0).toString(old, str, end, size);
        }
        break;

    case POSTDECREMENT:
    case POSTINCREMENT:
        if (precedence > get(0).getPrecedence()) 
        {
            append(str, end, size, '(');
            get(0).toString(old, str, end, size);
            append(str, end, size, ')');
        } 
        else 
        {
            get(0).toString(old, str, end, size);
        }
        append(str, end, size, getKind() == POSTDECREMENT ? "--" : "++");
        break;
     
    case PREDECREMENT:
    case PREINCREMENT:
        append(str, end, size, getKind() == PREDECREMENT ? "--" : "++");
        if (precedence > get(0).getPrecedence()) 
        {
            append(str, end, size, '(');
            get(0).toString(old, str, end, size);
            append(str, end, size, ')');
        } 
        else 
        {
            get(0).toString(old, str, end, size);
        }
        break;

    case NOT:
        append(str, end, size, '!');
        if (precedence > get(0).getPrecedence()) 
        {
            append(str, end, size, '(');
            get(0).toString(old, str, end, size);
            append(str, end, size, ')');
        }
        else 
        {
            get(0).toString(old, str, end, size);
        }
        break;

    case DOT:
    {
        type_t type = get(0).getType();
        if (type.isProcess() || type.isRecord())
        {
            if (precedence > get(0).getPrecedence()) 
            {
                append(str, end, size, '(');
                get(0).toString(old, str, end, size);
                append(str, end, size, ')');
            } 
            else 
            {
                get(0).toString(old, str, end, size);
            }
            append(str, end, size, '.');
            append(str, end, size, type.getRecordLabel(data->value).c_str());
        } 
        else 
        {
            assert(0);
        }
        break;
    }        
            
    case INLINEIF:
        if (precedence >= get(0).getPrecedence()) 
        {
            append(str, end, size, '(');
            get(0).toString(old, str, end, size);
            append(str, end, size, ')');
        }
        else 
        {
            get(0).toString(old, str, end, size);
        }

        append(str, end, size, " ? ");

        if (precedence >= get(1).getPrecedence()) 
        {
            append(str, end, size, '(');
            get(1).toString(old, str, end, size);
            append(str, end, size, ')');
        }
        else 
        {
            get(1).toString(old, str, end, size);
        }

        append(str, end, size, " : ");

        if (precedence >= get(2).getPrecedence()) 
        {
            append(str, end, size, '(');
            get(2).toString(old, str, end, size);
            append(str, end, size, ')');
        }
        else 
        {
            get(2).toString(old, str, end, size);
        }
        
        break;

    case COMMA:
        get(0).toString(old, str, end, size);
        append(str, end, size, ", ");
        get(1).toString(old, str, end, size);
        break;
            
    case SYNC:
        get(0).toString(old, str, end, size);
        switch (data->sync) 
        {
        case SYNC_QUE:
            append(str, end, size, '?');
            break;
        case SYNC_BANG:
            append(str, end, size, '!');
            break;
        }
        break;

    case DEADLOCK:
        append(str, end, size, "deadlock");
        break;

    case LIST:
        append(str, end, size, "{ ");
        get(0).toString(old, str, end, size);
        for (uint32_t i = 1; i < getSize(); i++) 
        {
            append(str, end, size, ", ");
            get(i).toString(old, str, end, size);
        }
        append(str, end, size, " }");
        break;

    case FUNCALL:
        get(0).toString(old, str, end, size);
         append(str, end, size, '(');
         if (getSize() > 1) 
        {
             get(1).toString(old, str, end, size);
             for (uint32_t i = 2; i < getSize(); i++) 
            {
                 append(str, end, size, ", ");
                 get(i).toString(old, str, end, size);
             }
         }
         append(str, end, size, ')');
         break;

    case RATE:
        get(0).toString(old, str, end, size);
        append(str, end, size, "'");
        break;
        
    case EF:
        append(str, end, size, "E<> ");
        get(0).toString(old, str, end, size);
        break;

    case EF_R:
        append(str, end, size, "E<>* ");
        get(0).toString(old, str, end, size);
        break;

    case EG:
        append(str, end, size, "E[] ");
        get(0).toString(old, str, end, size);
        break;

    case AF:
        append(str, end, size, "A<> ");
        get(0).toString(old, str, end, size);
        break;

    case AG:
        append(str, end, size, "A[] ");
        get(0).toString(old, str, end, size);
        break;

    case AG_R:
        append(str, end, size, "A[]* ");
        get(0).toString(old, str, end, size);
        break;

    case EF2:
        append(str, end, size, "EF ");
        get(0).toString(old, str, end, size);
        break;

    case EG2:
        append(str, end, size, "EG ");
        get(0).toString(old, str, end, size);
        break;

    case AF2:
        append(str, end, size, "AG ");
        get(0).toString(old, str, end, size);
        break;

    case AG2:
        append(str, end, size, "AG ");
        get(0).toString(old, str, end, size);
        break;

    case LEADSTO:
        get(0).toString(old, str, end, size);
        append(str, end, size, " --> ");
        get(1).toString(old, str, end, size);
        break;

    case A_UNTIL:
        append(str, end, size, "A[");
        get(0).toString(old, str, end, size);
        append(str, end, size, " U ");
        get(1).toString(old, str, end, size);
        append(str, end, size, "] ");
        break;

    case A_WEAKUNTIL:
        append(str, end, size, "A[");
        get(0).toString(old, str, end, size);
        append(str, end, size, " W ");
        get(1).toString(old, str, end, size);
        append(str, end, size, "] ");
        break;

    case FORALL:
        append(str, end, size, "forall (");
        append(str, end, size, get(0).getSymbol().getName().c_str());
        append(str, end, size, ":");
        append(str, end, size, get(0).getSymbol().getType().toString().c_str());
        append(str, end, size, ") ");
        get(1).toString(old, str, end, size);
        break;

    case EXISTS:
        append(str, end, size, "exists (");
        append(str, end, size, get(0).getSymbol().getName().c_str());
        append(str, end, size, ":");
        append(str, end, size, get(0).getSymbol().getType().toString().c_str());
        append(str, end, size, ") ");
        get(1).toString(old, str, end, size);
        break;

    case EF_CONTROL:
        append(str, end, size, "E<> ");
    case CONTROL:
        append(str, end, size, "control: ");
        get(0).toString(old, str, end, size);
        break;
    case CONTROL_TOPT:
        append(str, end, size, "control_t*(");
        get(0).toString(old, str, end, size);
        append(str, end, size, ",");
        get(1).toString(old, str, end, size);
        append(str, end, size, "): ");
        get(2).toString(old, str, end, size);
        break;
        
    default:
        throw std::runtime_error("Strange exception");
    }
}

bool expression_t::operator < (const expression_t e) const
{
    return data != NULL && e.data != NULL && data < e.data;
}
 
bool expression_t::operator == (const expression_t e) const
{
    return data == e.data;
}


/** Returns a string representation of the expression. The string
    returned must be deallocated with delete[]. Returns NULL is the
    expression is empty. */
std::string expression_t::toString(bool old) const
{
    if (empty()) 
    {
        return std::string();
    } 
    else 
    {
         int size = 16;
         char *s, *end;
         s = end = new char[size];
         toString(old, s, end, size);
        std::string result = s;
        delete[] s;
         return result;
    }
}

void expression_t::collectPossibleWrites(set<symbol_t> &symbols) const
{
    function_t *fun;
    symbol_t symbol;
    type_t type;

    if (empty())
    {
        return;
    }

    for (uint32_t i = 0; i < getSize(); i++) 
    {
        get(i).collectPossibleWrites(symbols);
    }

    switch (getKind()) 
    {
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
    case POSTINCREMENT:
    case POSTDECREMENT:
    case PREINCREMENT:
    case PREDECREMENT:
        get(0).getSymbols(symbols);
        break;
      
    case FUNCALL:
        // Add all symbols which are changed by the function
        symbol = get(0).getSymbol();
        if (symbol.getType().isFunction() && symbol.getData())
        {
            fun = (function_t*)symbol.getData();

            symbols.insert(fun->changes.begin(), fun->changes.end());

            // Add arguments to non-constant reference parameters
            type = fun->uid.getType();
            for (uint32_t i = 1; i < min(getSize(), type.size()); i++)
            {
                if (type[i].is(REF) && !type[i].isConstant())
                {
                    get(i).getSymbols(symbols);
                }
            }
        }
        break;

    default:
        break;
    }
}

void expression_t::collectPossibleReads(set<symbol_t> &symbols) const
{
    function_t *fun;
    frame_t parameters;
    symbol_t symbol;

    if (empty())
    {
        return;
    }

    for (uint32_t i = 0; i < getSize(); i++) 
    {
        get(i).collectPossibleReads(symbols);
    }

    switch (getKind()) 
    {
    case IDENTIFIER:
        symbols.insert(getSymbol());
        break;

    case FUNCALL:
        // Add all symbols which are used by the function        
        symbol = get(0).getSymbol();
        if (symbol.getType().isFunction() && symbol.getData())
        {
            fun = (function_t*)symbol.getData();
            symbols.insert(fun->depends.begin(), fun->depends.end());
        }
        break;

    default:
        break;
    }
}


expression_t expression_t::createConstant(int32_t value, position_t pos)
{
    expression_t expr(CONSTANT, pos);
    expr.data->value = value;
    expr.data->type = type_t::createPrimitive(Constants::INT);
    return expr;
}

expression_t expression_t::createIdentifier(symbol_t symbol, position_t pos)
{
    expression_t expr(IDENTIFIER, pos);
    expr.data->symbol = symbol;
    if (symbol != symbol_t())
    {
        expr.data->type = symbol.getType();
    }
    else
    {
        expr.data->type = type_t();
    }
    return expr;
}

expression_t expression_t::createNary(
    kind_t kind, const vector<expression_t> &sub, position_t pos, type_t type)
{
    assert(kind == LIST || kind == FUNCALL);
    expression_t expr(kind, pos);
    expr.data->value = sub.size();
    expr.data->sub = new expression_t[sub.size()];
    expr.data->type = type;
    for (uint32_t i = 0; i < sub.size(); i++)
    {
        expr.data->sub[i] = sub[i];
    }
    return expr;
}

expression_t expression_t::createUnary(
    kind_t kind, expression_t sub, position_t pos, type_t type)
{
    expression_t expr(kind, pos);
    expr.data->sub = new expression_t[1];
    expr.data->sub[0] = sub;
    expr.data->type = type;
    return expr;
}

expression_t expression_t::createBinary(
    kind_t kind, expression_t left, expression_t right, 
    position_t pos, type_t type)
{
    expression_t expr(kind, pos);
    expr.data->sub = new expression_t[2];
    expr.data->sub[0] = left;
    expr.data->sub[1] = right;
    expr.data->type = type;
    return expr;
}

expression_t expression_t::createTernary(
    kind_t kind, expression_t e1, expression_t e2, expression_t e3, 
    position_t pos, type_t type)
{
    expression_t expr(kind, pos);
    expr.data->sub = new expression_t[3];
    expr.data->sub[0] = e1;
    expr.data->sub[1] = e2;
    expr.data->sub[2] = e3;
    expr.data->type = type;
    return expr;
}

expression_t expression_t::createDot(
    expression_t e, int32_t idx, position_t pos, type_t type)
{
    expression_t expr(DOT, pos);
    expr.data->index = idx;
    expr.data->sub = new expression_t[1];
    expr.data->sub[0] = e;
    expr.data->type = type;
    return expr;
}

expression_t expression_t::createSync(
    expression_t e, synchronisation_t s, position_t pos)
{
    expression_t expr(SYNC, pos);
    expr.data->sync = s;
    expr.data->sub = new expression_t[1];
    expr.data->sub[0] = e;
    return expr;
}

expression_t expression_t::createDeadlock(position_t pos)
{
    expression_t expr(DEADLOCK, pos);
    expr.data->type = type_t::createPrimitive(CONSTRAINT);
    return expr;
}

ostream &operator<< (ostream &o, const expression_t &e) 
{
    o << e.toString();
    return o;
}
