/*!\file
 * This file contains definition of reporter_t class (class for runtime
 * statistics and reports)
 *
 * \author Radek Pelanek
 */

#ifndef _DIVINE_REPORTER_HH_
#define _DIVINE_REPORTER_HH_

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include <map>
#include <string>
#include "common/sysinfo.hh"
#include "common/error.hh"
#include "common/types.hh"


namespace divine {
#endif //DOXYGEN_PROCESSING
  
  //! Class for messuring and reporting
  /*! Class reporter_t is used to measure and report (in standard format)
   * statistics
   * about the computation of an algorithm.
   *
   * \todo Currently it prints a memory consumtion only in the end of the run
   * of the program. It should print a maximum during a run of the program
  */
  class reporter_t {
  protected:
    vminfo_t vm_info;
    timeinfo_t time_info;
    std::map<std::string, double> specific_info; 
    std::map<std::string, std::string> specific_long_name;
    double time;
    double init_time;
    std::string file_name, alg_name, problem;
    size_int_t states_stored, succs_calls;
    //    int max_vmsize, max_vmrss, max_vmdata;
  public:

    /* Constructor does nothing useful. Instead you should use set_file_name, set_alg_name,
     * set_problem to inicialize the reporter.
     */
    reporter_t() {time = -1; file_name = "Unknown"; alg_name = "Unknown"; problem = "Unknown"; states_stored = 0; succs_calls = 0; init_time=0;}

    /*! Prints the report into the given ostream.
    */
    void print(std::ostream& out);

    /*! Prints the report into the file with 'standard name' (at the moment file_name.report).
     */
    void print();
    
    /*! Starts the timer which measure the (real) time taken by computation.
     * Convention: This should be called after  inicializations.
     */
    void start_timer() { time_info.reset(); }
    
    /*! Stops the timer.
     */
    void stop_timer() { time = time_info.gettime() + init_time; }

    /*! Sets name of the dve file which is the input of the algorithm.
     */
    void set_file_name(std::string s) { file_name = s ;}

    /*! Sets name of the algorithm.
     */
    void set_alg_name(std::string s) { alg_name = s ;}

    /*! Sets the description of the problem that the algorithm solves (e.g., 'reachability x==3', 'ltl GF hungry == 0') 
     */
    void set_problem(std::string s) { problem = s; }

    /*! Sets the number of stored states
     */
    void set_states_stored(size_int_t stored) { states_stored = stored; }

    /*! Sets the number of calling get_succs() function
     */
    void set_succs_calls(size_int_t calls) { succs_calls = calls; }

    /*! Sets all obligatory keys (algorithm name, input file name, problem, number of states and number of calling successor function)
     */
    void set_obligatory_keys(std::string _alg_name, std::string _file_name, std::string _problem, size_int_t _states_stored, size_int_t _succs_calls) 
    { alg_name = _alg_name; file_name = _file_name; problem = _problem; states_stored = _states_stored; succs_calls = _succs_calls; }

    /*! Sets any specific information which we want to report (e.g., set_info("states", states_num);
     */
    void set_info(std::string s, double a) { specific_info[s] = a; if (s=="InitTime") init_time=a; }
    
    void set_info(std::string s, double a, const std::string & long_name)
    { set_info(s,a); specific_long_name[s]=long_name; }

    /*! Returns the  time (to be used after stop_time); usefull e.g. for calculating speed
     */
    double get_time() { return time; }
    
  };
  
#ifndef DOXYGEN_PROCESSING
} //end of namespace
#endif //DOXYGEN_PROCESSING
  
#endif
