#include <stack>
#include <iostream>
#include "common/types.hh"
#include "common/error.hh"
#include "common/deb.hh"

using namespace divine;
using namespace std;

error_vector_t divine::gerr;

size_int_t error_string_t::alloc_str_count = 0;

void error_vector_t::set_push_callback(const ERR_psh_callback_t func)
{ push_callback = func; }

void error_vector_t::set_throw_callback(const ERR_thr_callback_t func)
{ throw_callback = func; }

void error_vector_t::push(error_string_t& mes, const ERR_id_t id)
{
 err_vector.push_back(Pair(mes,id));
 mes.recreate();
}

void error_vector_t::that(const ERR_c_triplet_t & st)
{
 error_string_t aux_str;
 aux_str << st.message;  //aux_str allocates memory
                         //but push copies poiter to the element on the stack
 push(aux_str, st.id); 
 aux_str.delete_content();
 throw_callback(*this,ERR_throw_t(st.type, st.id));
}

void error_vector_t::that(error_string_t & mes,
                          const ERR_type_t err_type, const ERR_id_t id)
{ push(mes,id); throw_callback(*this,ERR_throw_t(err_type, id)); }

void error_vector_t::pop(const ERR_nbr_t index)
{ err_vector.erase(err_vector.begin()+index); }

void error_vector_t::pop(const ERR_nbr_t begin, const ERR_nbr_t end)
{ err_vector.erase(err_vector.begin() + begin, err_vector.begin()+(end-1)); }

void error_vector_t::print(ERR_char_string_t mes)
{ if (!is_silent) cerr << mes << endl; }

void error_vector_t::perror(const ERR_nbr_t i)
{ if (!is_silent) cerr << err_vector[i].message << endl; }

void error_vector_t::perror_back()
{ if (!is_silent) cerr << err_vector.back().message << endl; }

void error_vector_t::perror_front()
{ if (!is_silent) cerr << err_vector.front().message << endl; }

void error_vector_t::perror(ERR_char_string_t mes, const ERR_nbr_t i)
{ if (!is_silent) cerr << mes << ": " << err_vector[i].message << endl; }

void error_vector_t::perror_back(ERR_char_string_t mes)
{ if (!is_silent) cerr << mes << ": " << err_vector.back().message << endl; }

void error_vector_t::perror_front(ERR_char_string_t mes)
{ if (!is_silent) cerr << mes << ": " << err_vector.front().message << endl; }

const error_string_t error_vector_t::string_back()
{ return err_vector.back().message; }

const error_string_t error_vector_t::string_front()
{ return err_vector.front().message; }

const error_string_t error_vector_t::string(const ERR_nbr_t i)
{ return err_vector[i].message; }

ERR_id_t error_vector_t::id_back()
{ return err_vector.back().id; }

ERR_id_t error_vector_t::id_front()
{ return err_vector.front().id; }

ERR_id_t error_vector_t::id(const ERR_nbr_t i)
{ return err_vector[i].id; }

void error_vector_t::pop_back()
{ err_vector.back().message.delete_content(); err_vector.pop_back(); }

void error_vector_t::pop_front()
{ err_vector.front().message.delete_content();
  err_vector.erase(err_vector.begin()); }
                      
void error_vector_t::operator<<(const thr & e)
{ that(string_err,e.t,e.c); }

void error_vector_t::operator<<(const psh & e)
{ push(string_err,e.c); push_callback(*this,ERR_throw_t(e.t,e.c)); }


void error_vector_t::pop_back(const ERR_nbr_t n)
{
 ERR_nbr_t iter;
 if (n<=0 || n>err_vector.size()) iter = err_vector.size();
 else iter = n;
 for (;iter!=0;--iter) pop_back();
}

void error_vector_t::pop_front(const ERR_nbr_t n)
{
 ERR_nbr_t iter;
 if (n<=0 || n>err_vector.size()) iter = err_vector.size();
 else iter = n;
 for (;iter!=0;--iter) pop_front();
}

void error_vector_t::flush()
{
 for (ERR_nbr_t i = 0; i!=count(); i++) perror(i);
 clear();
}

void divine::ERR_default_thr_callback
   (error_vector_t & terr, const ERR_throw_t error_type)
{
 terr.flush();
 throw error_type;
}

void divine::ERR_default_psh_callback
   (error_vector_t & terr, const ERR_throw_t error_type)
{
 terr.perror_back();
 terr.pop_back();
}

