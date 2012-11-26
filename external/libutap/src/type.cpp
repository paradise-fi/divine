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

#include "utap/type.h"
#include "utap/expression.h"
#include <boost/format.hpp>

using std::string;
using std::vector;

using namespace UTAP;
using namespace Constants;

struct type_t::child_t
{
    string label;
    type_t child;
};

struct type_t::type_data
{
    int32_t count;                // Reference count
    kind_t kind;                // Kind of type object
    position_t position;        // Position in the input file
    expression_t expr;          // 
    size_t size;                // Number of children
    child_t *children;          // The children
};

type_t::type_t()
{
    data = NULL;
}

type_t::type_t(kind_t kind, const position_t &pos, size_t size)
{
    data = new type_data;
    data->count = 1;
    data->kind = kind;
    data->position = pos;
    data->size = size;
    data->children = new child_t[size];
}

type_t::type_t(const type_t &type)
{
    data = type.data;
    if (data)
    {
        data->count++;
    }
}

type_t::~type_t()
{
    if (data) 
    {
        data->count--;
        if (data->count == 0)
        {
            delete[] data->children;
            delete data;
        }
    }
}

const type_t &type_t::operator = (const type_t &type)
{
    if (data) 
    {
        data->count--;
        if (data->count == 0)
        {
            delete[] data->children;
            delete data;
        }
    }
    data = type.data;
    if (data) 
    {
        data->count++;
    }
    return *this;
}

bool type_t::operator == (const type_t &type) const
{
    return data == type.data;
}

bool type_t::operator != (const type_t &type) const
{
    return data != type.data;
}

bool type_t::operator < (const type_t &type) const
{
    return data < type.data;
}

size_t type_t::size() const
{
    assert(data);
    return data->size;
}

const type_t type_t::operator[](uint32_t i) const
{
    assert(i < size());
    return data->children[i].child;
}

const type_t type_t::get(uint32_t i) const
{
    assert(i < size());
    return data->children[i].child;
}

const std::string &type_t::getLabel(uint32_t i) const
{
    assert(i < size());
    return data->children[i].label;
}

int32_t type_t::findIndexOf(std::string label) const
{
    assert(isRecord() || isProcess());
    type_t type = strip();
    size_t n = type.size();
    for (size_t i = 0; i < n; i++)
    {
        if (type.getLabel(i) == label)
        {
            return i;
        }
    }
    return -1;
}

kind_t type_t::getKind() const
{
    return unknown() ? UNKNOWN : data->kind;
}

bool type_t::isPrefix() const 
{
    switch (getKind())
    {
    case Constants::UNKNOWN:
    case Constants::VOID_TYPE:
    case Constants::CLOCK:
    case Constants::INT:
    case Constants::BOOL:
    case Constants::SCALAR:
    case Constants::LOCATION:
    case Constants::CHANNEL:
    case Constants::COST:
    case Constants::INVARIANT:
    case Constants::INVARIANT_WR:
    case Constants::GUARD:
    case Constants::DIFF:
    case Constants::CONSTRAINT:
    case Constants::FORMULA:
    case Constants::ARRAY:
    case Constants::RECORD:
    case Constants::PROCESS:
    case Constants::PROCESSSET:
    case Constants::FUNCTION:
    case Constants::INSTANCE:
    case Constants::RANGE:
    case Constants::REF:
    case Constants::TYPEDEF:
    case Constants::LABEL:
    case Constants::RATE:
        return false;

    default:
        return true;
    }
}

bool type_t::unknown() const
{
    return data == NULL || data->kind == UNKNOWN;
}

bool type_t::is(kind_t kind) const
{
    return getKind() == kind
        || isPrefix() && get(0).is(kind)
        || getKind() == RANGE && get(0).is(kind)
        || getKind() == REF && get(0).is(kind)
        || getKind() == LABEL && get(0).is(kind);
}

type_t type_t::getSub() const
{
    assert(isArray());
    if (getKind() == REF || getKind() == LABEL)
    {
        return get(0).getSub();
    }
    else if (isPrefix())
    {
        return get(0).getSub().createPrefix(getKind());
    }
    else
    {
        return get(0);
    }
}

type_t type_t::getSub(size_t i) const
{
    assert(isRecord() || isProcess());
    if (getKind() == REF || getKind() == LABEL)
    {
        return get(0).getSub(i);
    }
    else if (isPrefix())
    {
        return get(0).getSub(i).createPrefix(getKind());
    }
    else 
    {
        return get(i);
    }
}

type_t type_t::getArraySize() const
{
    if (isPrefix() || getKind() == REF || getKind() == LABEL)
    {
        return get(0).getArraySize();
    }
    else
    {
        assert(getKind() == ARRAY);
        return get(1);
    }
}

size_t type_t::getRecordSize() const
{
    if (isPrefix() || getKind() == REF || getKind() == LABEL)
    {
        return get(0).getRecordSize();
    }
    else
    {
        assert(getKind() == RECORD);
        return size();
    }
}

string type_t::getRecordLabel(size_t i) const
{
    if (isPrefix() || getKind() == REF || getKind() == LABEL)
    {
        return get(0).getRecordLabel(i);
    }
    else
    {
        assert(getKind() == RECORD || getKind() == PROCESS);
        return getLabel(i);
    }
}

std::pair<expression_t, expression_t> type_t::getRange() const
{
    assert(is(RANGE));
    if (getKind() == RANGE)
    {
        return std::make_pair(get(1).getExpression(), get(2).getExpression());
    }
    else
    {
        return get(0).getRange();
    }
}

expression_t type_t::getExpression() const
{
    assert(data);
    return data->expr;
}

type_t type_t::strip() const
{
    if (isPrefix() || getKind() == RANGE || getKind() == REF || getKind() == LABEL)
    {
        return get(0).strip();
    }
    else
    {
        return *this;
    }
}

type_t type_t::stripArray() const
{
    type_t type = strip(); 
    while (type.getKind() == ARRAY)
    {
        type = type.get(0).strip();
    }
    return type;
}

type_t type_t::rename(std::string from, std::string to) const
{
    type_t type(getKind(), getPosition(), size());
    type.data->expr = getExpression();
    for (size_t i = 0; i < size(); i++)
    {
        type.data->children[i].child = get(i).rename(from, to);
        type.data->children[i].label = getLabel(i);
    }
    if (getKind() == LABEL && getLabel(0) == from)
    {
        type.data->children[0].label = to;
    }
    return type;
}

type_t type_t::subst(symbol_t symbol, expression_t expr) const
{
    type_t type = type_t(getKind(), getPosition(), size());
    for (size_t i = 0; i < size(); i++)
    {
        type.data->children[i].label = getLabel(i);
        type.data->children[i].child = get(i).subst(symbol, expr);
    }
    if (!data->expr.empty())
    {
        type.data->expr = data->expr.subst(symbol, expr);
    }
    return type;
}

position_t type_t::getPosition() const
{
    return data->position;
}

bool type_t::isIntegral() const
{
    return is(Constants::INT) || is(Constants::BOOL);
}

bool type_t::isInvariant() const
{
    return is(INVARIANT) || isIntegral();
}

bool type_t::isGuard() const
{
    return is(GUARD) || isInvariant();
}

bool type_t::isConstraint() const
{
    return is(CONSTRAINT) || isGuard();
}

bool type_t::isFormula() const
{
    return is(FORMULA) || isConstraint();
}

bool type_t::isConstant() const
{
    switch (getKind())
    {
    case FUNCTION:
    case PROCESS:
    case INSTANCE:
        return false;
    case CONSTANT:
        return true;
    case RECORD:
        for (size_t i = 0; i < size(); i++)
        {
            if (!get(i).isConstant())
            {
                return false;
            }
        }
        return true;
    default:
        return size() > 0 && get(0).isConstant();
    }
}

bool type_t::isNonConstant() const
{
    switch (getKind())
    {
    case FUNCTION:
    case PROCESS:
    case INSTANCE:
        return false;
    case CONSTANT:
        return false;
    case RECORD:
        for (size_t i = 0; i < size(); i++)
        {
            if (!get(i).isNonConstant())
            {
                return false;
            }
        }
        return true;
    default:
        return size() == 0 || get(0).isNonConstant();
    }
}

type_t type_t::createRange(type_t type, expression_t lower, expression_t upper,
                           position_t pos)
{
    type_t t(RANGE, pos, 3);
    t.data->children[0].child = type;
    t.data->children[1].child = type_t(UNKNOWN, pos, 0);
    t.data->children[2].child = type_t(UNKNOWN, pos, 0);
    t[1].data->expr = lower;
    t[2].data->expr = upper;
    return t;
}
        
type_t type_t::createRecord(const vector<type_t> &types,
                            const vector<string> &labels,
                            position_t pos)
{
    assert(types.size() == labels.size());
    type_t type(RECORD, pos, types.size());
    for (size_t i = 0; i < types.size(); i++)
    {
        type.data->children[i].child = types[i];
        type.data->children[i].label = labels[i];
    }
    return type;
}

type_t type_t::createFunction(type_t ret, 
                              const std::vector<type_t> &parameters, 
                              const std::vector<std::string> &labels,
                              position_t pos)
{
    assert(parameters.size() == labels.size());
    type_t type(FUNCTION, pos, parameters.size() + 1);
    type.data->children[0].child = ret;
    for (size_t i = 0; i < parameters.size(); i++)
    {
        type.data->children[i + 1].child = parameters[i];
        type.data->children[i + 1].label = labels[i];
    }
    return type;
}

type_t type_t::createArray(type_t sub, type_t size, position_t pos)
{
    type_t type(ARRAY, pos, 2);
    type.data->children[0].child = sub;
    type.data->children[1].child = size;
    return type;
}

type_t type_t::createTypeDef(std::string label, type_t type, position_t pos)
{
    type_t t(TYPEDEF, pos, 1);
    t.data->children[0].label = label;
    t.data->children[0].child = type;
    return t;
}

type_t type_t::createInstance(frame_t parameters, position_t pos)
{
    type_t type(INSTANCE, pos, parameters.getSize());
    for (size_t i = 0; i < parameters.getSize(); i++)
    {
        type.data->children[i].child = parameters[i].getType();
        type.data->children[i].label = parameters[i].getName();
    }
    return type;
}

type_t type_t::createProcess(frame_t frame, position_t pos)
{
    type_t type(PROCESS, pos, frame.getSize());
    for (size_t i = 0; i < frame.getSize(); i++)
    {
        type.data->children[i].child = frame[i].getType();
        type.data->children[i].label = frame[i].getName();
    }
    return type;
}

type_t type_t::createProcessSet(type_t instance, position_t pos)
{
    type_t type(PROCESSSET, pos, instance.size());
    for (size_t i = 0; i < instance.size(); i++)
    {
        type.data->children[i].child = instance[i];
        type.data->children[i].label = instance.getLabel(i);
    }
    return type;
}

type_t type_t::createPrimitive(kind_t kind, position_t pos) 
{
    return type_t(kind, pos, 0);
}

type_t type_t::createPrefix(kind_t kind, position_t pos) const
{
    type_t type(kind, pos, 1);
    type.data->children[0].child = *this;
    return type;
}

type_t type_t::createLabel(string label, position_t pos) const
{
    type_t type(LABEL, pos, 1);
    type.data->children[0].child = *this;
    type.data->children[0].label = label;
    return type;
}

string type_t::toString() const
{
    std::string str;
    std::string kind;

    if (data == NULL)
    {
        return "unknown";
    }

    if (!data->expr.empty())
    {
        return string("\"") + data->expr.toString() + string("\"");
    }

    switch (getKind())
    {
    case Constants::UNKNOWN:
        kind = "unknown";
        break;

    case Constants::RANGE:
        kind = "range";
        break;
        
    case Constants::ARRAY:
        kind = "array";
        break;

    case Constants::RECORD:
        kind = "struct";
        break;
        
    case Constants::CONSTANT:
        kind = "const";
        break;
        
    case Constants::REF:
        kind = "ref";
        break;
        
    case Constants::URGENT:
        kind = "urgent";
        break;

    case Constants::COMMITTED:
        kind = "committed";
        break;

    case Constants::BROADCAST:
        kind = "broadcast";
        break;

    case Constants::VOID_TYPE:
        kind = "void";
        break;

    case Constants::CLOCK:
        kind = "clock";
        break;

    case Constants::INT:
        kind = "int";
        break;

    case Constants::BOOL:
        kind = "bool";
        break;

    case Constants::SCALAR:
        kind = "scalar";
        break;

    case Constants::CHANNEL:
        kind = "channel";
        break;

    case Constants::INVARIANT:
        kind = "invariant";
        break;

    case Constants::GUARD:
        kind = "guard";
        break;

    case Constants::DIFF:
        kind = "diff";
        break;

    case Constants::CONSTRAINT:
        kind = "constraint";
        break;

    case Constants::FORMULA:
        kind = "formula";
        break;

    case Constants::COST:
        kind = "cost";
        break;

    case Constants::RATE:
        kind = "rate";
        break;

    case Constants::TYPEDEF:
        kind = "def";
        break;

    case Constants::PROCESS:
        kind = "process";
        break;
        
    case Constants::INSTANCE:
        kind = "instance";
        break;

    case Constants::LABEL:
        kind = "label";
        break;

    case Constants::FUNCTION:
        kind = "function";
        break;

    case Constants::LOCATION:
        kind = "location";
        break;

    default:
        kind = (boost::format("type(%1%)") % getKind()).str();
        break;
    }

    str = "(";
    str += kind;
    for (uint32_t i = 0; i < size(); i++)
    {
        str += " ";
        if (!getLabel(i).empty())
        {
            str += getLabel(i);
            str += ":";
        }
        str += get(i).toString();
    }
    str += ")";
    
    return str;
}


std::ostream &operator << (std::ostream &o, type_t t)
{
    o << t.toString();
    return o;
}
