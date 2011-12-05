// -*- C++ -*- (c) 2011 Jan Kriho

#include <divine/dve/error.h>

namespace divine {
namespace dve {

const ErrorState ErrorState::e_overflow = {{{1, 0, 0, 0, 0}}};
const ErrorState ErrorState::e_arrayCheck = {{{0, 1, 0, 0, 0}}};
const ErrorState ErrorState::e_divByZero = {{{0, 0, 1, 0, 0}}};
const ErrorState ErrorState::e_other = {{{0, 0, 0, 1, 0}}};
const ErrorState ErrorState::e_none = {{{0, 0, 0, 0, 0}}};

}    
}