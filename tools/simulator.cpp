#include <getopt.h>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>                                                                
#include <list>
#include <map>
#include <limits.h>
#include "sevine.h"
//#include "divine_utils.hh"

#include "divine/generator/common.h"

#define MAX_CMD_LENGTH 100 //maximal length of the user's command
#define MAX_LENGTH_VAR 5   //maximal number of characters of vector's item to write to output file

using namespace std;
using namespace divine;

const char * version_str = "Program simulator - last modification: 4.3.2004";
const char * input2output_str = "================"; //this command won't be set as unresolved and will be rewritten to output - e.g. to separate several sequences of transitions in the output file
const char * state_of_process_command = "state\0";
const char * const_trace_out_name = "trace.txt"; //default name of the output tracefile
const char transnames_separator = '&'; //separator char to concat transition name with synchronized trans. name
const char cmd_quit = 'q'; //quit whole program
const char cmd_init = 'i'; //go to the initial state
const char cmd_rand = 'r'; //execute random (enabled) transition
const char cmd_pvar = 'p'; //print requested variable (once times)
const char cmd_svar = 's'; //print requested variable and add it to show after every transition execution
const char cmd_hvar = 'h'; //hide variable showing after every transition execution

const char switch_help           = 'h'; //switch of help
const char switch_show           = 's'; //switch of namefile of variables to permanently show
const char switch_out            = 'o'; //switch of output file name (default name in trace_out_name)
const char switch_in             = 'i'; //switch of namefile of input trace to execute (silent mode)
const char switch_interactive_in = 'I'; //similar to switch_in but not silent (program waits to press Enter after each transition execution)
const char switch_vers           = 'v'; //switch of printing program version
const char *getopt_string        = "hs:o:i:I:v"; //BE AWARE: this constant has to correspond to constants switch_*

struct var_print_t //structure about variable to show after each transition execution
{ 
  char * name; //name of the variable in the source file
  std::size_t gid; //UID of the variable
  std::size_t length; //length of the vector; scalar => length=0
  std::size_t vect_index; //to print only one index of the vector; scalar => must be set to 0
  bool is_state; //if the "variable" is the name of state or real variable
};

list<var_print_t> show_vars_list; //list of variables "to show" (after each transition execution)
ofstream f_trace_out; //filestream to write executed trace

//prints short help
void print_help(void)
{
  cout << "Available arguments of the program \"simulator\":" << endl;
  cout << " latest argument:  the name of input file with automaton (*.dve)" << endl;
  cout << '-' << switch_help << " :  this help" << endl;
  cout << '-' << switch_vers << " :  prints the version of the program" << endl;
  cout << '-' << switch_out << " <filename> :  executed trace is written to the given file (by default to " << const_trace_out_name << ")" << endl;
  cout << '-' << switch_show << " <filename> :  variables \"to show\" are taken from this file" << endl;
  cout << '-' << switch_in << " <filename> :  from this file will be taken commands to execution (silent version)" << endl;
  cout << '-' << switch_interactive_in << " <filename> :  similar to previous, but not silent - " 
       << "program prints to the output as normally and waits to press Enter after each command" << endl;
  cout << endl;
  cout << "Commands during program execution:" << endl;
  cout << "\t nr_of_transition - execution of that transition (or by typing transition name)" << endl;
  cout << "\t " << cmd_quit << " - quit the program" << endl;
  cout << "\t " << cmd_init << " - go to the initial state" << endl;
  cout << "\t " << cmd_rand << " - choosing and executing a random transition" << endl;
  cout << "\t " << cmd_pvar << " proc_name.var_name - prints the actual value of given variable" << endl;
  cout << "\t " << cmd_svar << " proc_name.var_name - adds the variable to the list \"to show\"" << endl;
  cout << "\t " << cmd_hvar << " proc_name.var_name - deletes the variable from the list \"to show\"" << endl;
  cout << "\t the latest three commands understand \"*\" instead of var_name or whole argument, moreover " 
       << "you can type \"state\" instead of var_name to print/show/hide the local state of given process" << endl;
}

//auxiliary function
unsigned Char2Int(char * ch)
{ 
  unsigned i=0,ret=0;
  while (ch[i])
  { 
    if (isdigit(ch[i])) ret=ret*10+ch[i]-48;
    else return UINT_MAX;
    i++;
  }
  return ret;
}

//function parses given string to extract information about variable
//returns true iff the given name is correct name of global or local variable
//if it returns true, then var_print_t &var is fullfilled according to the parsed name
bool ParseVarName(char * cmd, unsigned pos, unsigned proc, dve_symbol_table_t *Table, var_print_t& var)
{ 
  std::size_t symbol_sid;
  char * tmp = strchr(cmd+pos,'['); //is variable an item of the vector?
  if (tmp) //extract name of vector only, not index
    { 
      char tmp2[tmp-cmd-pos+1];
      strncpy(tmp2,cmd+pos,tmp-cmd-pos);
      tmp2[tmp-cmd-pos]='\0';
      symbol_sid = ((proc)?(Table->find_symbol(tmp2,proc-1)):(Table->find_global_symbol(tmp2)));
    }
  else 
    symbol_sid = ((proc)?(Table->find_symbol(cmd+pos,proc-1)):(Table->find_global_symbol(cmd+pos)));
  dve_symbol_t *symbol;
  if (symbol_sid!=NO_ID)
    symbol = Table->get_symbol(symbol_sid);
  if ((symbol_sid==NO_ID)||(!(symbol->is_variable())))
    { 
      if (cmd[pos]!='*')
	{ 
	  if ((strcmp(cmd+pos,state_of_process_command)==0)&&(proc>0)) //user asks for state of process
	    { 
	      var.is_state = true;
	      var.name=cmd+2;
	      var.gid=proc; //var.gid is ProcessID, not UID of process
	      var.vect_index=var.length=0;
	      return true;
	    }
	  else cerr << "1: Incorrect command \"" << cmd << "\"." << endl;
	}
      return false;
    }
  var.name=cmd+2;
  var.gid=symbol->get_gid();
  var.vect_index=var.length=0;
  var.is_state=false;
  if (symbol->is_vector()) //print all items or only desired?
    {
      if (tmp) //only one item, extract index
	{ 
	  pos=tmp-cmd+1; proc=0; //proc means now index of the vector
	  while (isdigit(cmd[pos]))
	    { 
	      proc=proc*10+cmd[pos++]-48; }
	  if (proc>=symbol->get_vector_size()) //isn't index out of range?
	    {
	      cerr << "Too big index of the vector in command \"" << cmd+2 << "\"." << endl;
	      return false;
	    }
	  var.vect_index=proc;
	  var.length=0;
	}
      else 
	var.length=symbol->get_vector_size()-1;
    }
  return true;
}

//to extract gid of process from command-string naming the process
//returns true iff the given process name exists
//if it returns true, then unsigned& proc contains GID+1 of corresponding process
//BE AWARE: due to historical reasons, proc contains really GID+1 of the process. proc=0 is reserved to global variables; the whole program uses this fact
bool ParseProcGID (char * cmd, unsigned pos, unsigned& proc, dve_explicit_system_t * System, dve_symbol_table_t *Table)
{
  dve_symbol_t *symbol;
  for (proc=1; proc<=System->get_process_count(); proc++)
    { 
      symbol = Table->get_process(proc-1);
      std::size_t length = (pos-1>symbol->get_name_length())?(pos-1):(symbol->get_name_length());
      if (strncmp(cmd,symbol->get_name(),length)==0)
	return true;
    }
  return false;
}

//deleting list of vars "to show"
void DeleteVarList(list<var_print_t> *varlist)
{
  while (!(varlist->empty()))
    { 
      delete[] varlist->front().name;
      varlist->pop_front();
    }
}

//inserting new variable to list of vars "to show"; the insertion fails iff the variable is allready maintained in varlist
bool InsertVarToShow(list<var_print_t> *varlist, var_print_t *var, const char *name=0)
{
  list<var_print_t>::iterator i = varlist->begin();
  while ((i != varlist->end())
	 &&((i->gid!=var->gid)||(i->vect_index!=var->vect_index)||(i->is_state!=var->is_state)))
    ++i;
  if (i != varlist->end())
    { 
      cerr << "The variable \"" << name << "\" is allready dumping." << endl;
      return false;
    }
  if (name) //set the variable name (name=0 iff var->name has been already set
    {
      char *tmp = new char [strlen(name)+1];
      tmp = strcpy(tmp,name);
      var->name=tmp;
    }
  varlist->push_back(*var);
  return true;
}

//printout of value(s) of a given variable to the given stream (used for cout or f_trace_out)
//the body of the procedure is little bit complicated due to the effort of correct formating of output
void PrintVar(ostream *out, dve_explicit_system_t * System, state_t * s, var_print_t * var, bool to_screen)
{
  if (to_screen) 
    *out << var->name << ": ";
  unsigned length = out->tellp(); //strlen of output string - necessary to formatting
  if (var->is_state) //printing name of state, not real variable //gid-1 musi byt!
    { 
      dve_symbol_table_t * Table = System->get_symbol_table();
      std::size_t process_state=System->get_state_of_process(*s,var->gid-1);
      dve_symbol_t *symbol = Table->get_state(dynamic_cast<dve_process_t*>(System->get_process(var->gid-1))->get_state_gid(process_state));
      *out << symbol->get_name();
    }
  else //printing real variable
    { 
      if (var->length) //printout whole vector
	{ 
	  *out << '{';
	  for (unsigned i=0;i<var->length;i++)
	    *out << System->get_var_value(*s, var->gid, i) << '|';
	  *out << System->get_var_value(*s, var->gid, var->length) << "}";
	}
      else //printout only one item of the vector
	*out << System->get_var_value(*s, var->gid, var->vect_index);
    }
  //text formatting - on screen skip to next line, in the file skip by tabulators
  if (to_screen)
    *out << endl;
  else
    { 
      length=((unsigned)out->tellp())-length; //now: length=size of written text
      unsigned max_length=(strlen(var->name)>8)?(strlen(var->name)):(8);
      if ((var->length*MAX_LENGTH_VAR)>max_length) //items of the variable can consume more chars then its name
	max_length=var->length*MAX_LENGTH_VAR;
      for (length=length-length%8;length<max_length;length+=8) //how many tabs must I write
	*out << '\t';
    }
}

void SetNameOfVarFromGID(var_print_t &var, dve_symbol_table_t *Table)
{
  dve_symbol_t *symbol = Table->get_variable(var.gid);
  var.length=(symbol->is_vector())?(symbol->get_vector_size()):(0);
  char *tmp = new char [strlen(symbol->get_name())+1];
  tmp = strcpy(tmp,symbol->get_name());
  var.name=tmp;
  var.name[symbol->get_name_length()]='\0';
}

//adding all variables of a given process to the list of variables "to show"
void AddAllVars(std::size_t proc_gid, dve_explicit_system_t * System, dve_symbol_table_t *Table, list<var_print_t> *varlist)
{ 
  char name[MAX_CMD_LENGTH]; //name of the adding variable (with the name of the process)
  dve_process_t *proc = dynamic_cast<dve_process_t*>(System->get_process(proc_gid));
  std::size_t varcount = proc->get_variable_count();
  dve_symbol_t *symbol;
  std::size_t pos = 0; //position of begin of varname (after name of process)
  if (proc) //write down name of process and '.'
    { 
      symbol = Table->get_process(proc_gid);
      strcpy(name,symbol->get_name());
      pos = symbol->get_name_length();
      name[pos++]='.';
    }
  var_print_t var; var.vect_index=0;
  var.is_state=false;
  for (std::size_t i=0; i<varcount; i++)
    { 
      var.gid=proc->get_variable_gid(i);
      SetNameOfVarFromGID(var, Table);
      InsertVarToShow(varlist, &var);
    }
}

//adding all variables (both local and global) to the list of variables "to show"
void AddAllVars(dve_explicit_system_t * System, dve_symbol_table_t *Table, list<var_print_t> *varlist)
{
  //adding all global variables
  var_print_t var; 
  var.vect_index=0;
  var.is_state=false;
  for (size_int_t gid=0; gid<System->get_global_variable_count(); gid++)
    {
      var.gid=System->get_global_variable_gid(gid);
      SetNameOfVarFromGID(var, Table);
      InsertVarToShow(varlist, &var);
    }
  //adding all local variables
  for (unsigned proc_gid=0; proc_gid<System->get_process_count(); proc_gid++)
    AddAllVars(proc_gid, System, Table, varlist);
}

//printout all variables of a given process in a given state
void PrintAllVars(unsigned proc_gid, dve_symbol_table_t *Table, dve_explicit_system_t * System, state_t * s)
{
  dve_process_t *proc = dynamic_cast<dve_process_t*>(System->get_process(proc_gid));
  dve_symbol_t *proc_as_symbol = Table->get_process(proc_gid);
  std::size_t varcount = proc->get_variable_count();
  var_print_t var; var.vect_index=0; var.name=new char[1]; var.name[0]='\0'; var.is_state=false;
  //var.name="" because the name of each variable is written before calling PrintVar - simplier and quicklier
  for (std::size_t i=0; i<varcount; i++)
    {
      var.gid=proc->get_variable_gid(i);
      dve_symbol_t *symbol = Table->get_variable(var.gid);
      var.length=(symbol->is_vector())?(symbol->get_vector_size()):(0);
      cout << proc_as_symbol->get_name() << '.' << symbol->get_name();
      PrintVar(&cout, System, s, &var, true);
    }
  delete[] var.name;
}

//printout all variables (both local and global) in a given state
void PrintAllVars(dve_symbol_table_t *Table, dve_explicit_system_t * System, state_t * s)
{
  //printing global variables
  var_print_t var; var.vect_index=0; var.name=new char[1]; var.name[0]='\0'; var.is_state=false;
  for (size_int_t gid=0; gid<System->get_global_variable_count(); gid++)
    {
      var.gid=System->get_global_variable_gid(gid);
      dve_symbol_t *symbol = Table->get_variable(var.gid);
      var.length=(symbol->is_vector())?(symbol->get_vector_size()):(0);
      cout << symbol->get_name();
      PrintVar(&cout, System, s, &var, true);
    }
  delete[] var.name;
  //printing local variables
  for (unsigned proc_gid=0; proc_gid<System->get_process_count(); proc_gid++)
    PrintAllVars(proc_gid, Table, System, s);
}

//reading variables "to show" from the given file
//returned value: 0 - all is OK; 1 - file not found; 2 - some variable is incorrect
int read_vars_to_show(char * filename, dve_explicit_system_t * System, dve_symbol_table_t *Table, list<var_print_t> *varlist)
{
  fstream f;
  f.open(filename, ios_base::in);
  if (!f)
    return 1;
  char cmd[MAX_CMD_LENGTH];
  f.getline(cmd,MAX_CMD_LENGTH);
  while (f.gcount())
    {
      unsigned pos = 0, proc_index = 0;
      if (cmd[0]=='*')
	{ 
	  AddAllVars(System, Table, varlist);
	  break; //reading of values can be finished - everything was added to show
	}
      while ((cmd[pos])&&(cmd[pos]!='.')) //similar to strchr
	pos++;
      if (cmd[pos]) //cmd contains '.' => it contains a local variable, thus extract the name of the process
	{ 
	  pos++;
	  if (!ParseProcGID(cmd, pos, proc_index, System, Table))
	    { 
	      cerr << "2: Incorrect name of the variable \"" << cmd << "\" in the file " << filename << ", program terminates." << endl;
	      f.close();
	      DeleteVarList(varlist);
	      return 2;
	    }
	}
      else //global variable
	{ 
	  proc_index = 0;
	  pos = 0; //begin of the variable name
	}
      var_print_t var;
      if (!ParseVarName(cmd, pos, proc_index, Table, var))
	{ 
	  if (cmd[pos]=='*')
	    AddAllVars(proc_index, System, Table, varlist);
	  else
	    { 
	      f.close();
	      return 2;
	    }
	}
      else //add to show a single global variable
	{ 
	  InsertVarToShow(varlist, &var, cmd);
	}
      f.getline(cmd,MAX_CMD_LENGTH);
    }
  f.close();
  return 0;
}

//printout values of variables "to show" to output file
void WriteVarNamesToShow(list<var_print_t> *varlist, bool system_in_initial_state)
{
  if (!system_in_initial_state)
    f_trace_out << "----------" << endl;
  f_trace_out << "Variables\t\t\t";
  for (list<var_print_t>::iterator var=varlist->begin(); var != varlist->end(); ++var)
  {
    f_trace_out << var->name;
    unsigned length=strlen(var->name);
    unsigned max_length=(strlen(var->name)>8)?(strlen(var->name)):(8);
    if ((var->length*MAX_LENGTH_VAR)>max_length) //items of the variable can consume more chars then its name
      max_length=var->length*MAX_LENGTH_VAR;
    for (length=length-length%8;length<max_length;length+=8) //how many tabs must I write
       f_trace_out << '\t';
  }
  f_trace_out << endl;
  if (system_in_initial_state)
    f_trace_out << "Init.state:\t\t\t";
}

int main(int argc, char * argv[])
{ 
  srand(time(0)); //for random step of simulation
  char * trace_out_name = NULL;
  char * vars_show_file_name = 0;
  char * trace_in_file_name = 0;
  bool write_to_cout = true;
  int c;
  static struct option longopts[] = {
    { "help",       no_argument, 0, switch_help},
    { "version",    no_argument, 0, switch_vers},
    { "show",       no_argument, 0, switch_show},
    { NULL, 0, NULL, 0 }
  };
  //"hs:o:i:I:v"
  while ((c = getopt_long(argc, argv, getopt_string, longopts, NULL)) != -1)
    {
      switch (c) {
      case switch_help: case '?': { print_help(); return 0; }
      case switch_out: { 
	trace_out_name = new char[strlen(optarg)+1];
	strcpy(trace_out_name,optarg);
	trace_out_name[strlen(optarg)]='\0';
	break;
      }
      case switch_show: {
	vars_show_file_name = optarg;
	break;
      }
      case switch_in: {
	write_to_cout=false;
	trace_in_file_name = optarg;
	break;
      }
      case switch_interactive_in: {
	write_to_cout=true;
	trace_in_file_name = optarg;
	break;
      }
      case switch_vers: { cout << version_str << endl; return 0; }
      } //end of "switch (c)
    }

  if (argc < optind+1) //missing the latest argument (input model)
    {
      print_help();
      return 1;
    }  
  
  if (trace_out_name==NULL) //name of the output file not set
  { trace_out_name = new char[strlen(const_trace_out_name)+1];
    strcpy(trace_out_name,const_trace_out_name);
    trace_out_name[strlen(const_trace_out_name)]='\0';
  }
  generator::Allocator Generator;
  dve_explicit_system_t System(gerr);
  System.setAllocator(&Generator);
  
  const char * VSTUP = argv[optind]; //the latest parameter is the filename of input model
  int file_opening = System.read(VSTUP);
  try
   {
    if (file_opening==system_t::ERR_FILE_NOT_OPEN)
      gerr << "Input file \"" << VSTUP << "\" not found." << thr();
    else if (file_opening)
      gerr << "Syntax error in " << VSTUP << thr();
   }
  catch (ERR_throw_t & err)
  { return err.id; }
  
  state_t s2, s = System.get_initial_state();
  dve_symbol_table_t * Symbol_table = System.get_symbol_table();

  //opening outputfile now necessary - to write names of vars "to show"
  f_trace_out.open(trace_out_name, ios_base::out);
  /* if switch "-s" set => read variables to show from the file */
  if (vars_show_file_name)
  { if (write_to_cout)
      cout << "Variables \"to show\" from the file: " << vars_show_file_name << endl;
    int i = read_vars_to_show(vars_show_file_name, &System, Symbol_table, &show_vars_list);
    switch (i)
    { 
      case 1: { cerr << "Input file \"" << vars_show_file_name << "\" not found." << endl;
                return 1;
              }
      case 2: return 1;
      default: { cerr << "Variables \"to show\" were loaded correctly." << endl;
                 WriteVarNamesToShow(&show_vars_list,true);
               }
    }
  }

  /* if switch "-i" set => open corresponding input file */
  ifstream *cmd_stream;
  if (trace_in_file_name)
  { cmd_stream=new ifstream(trace_in_file_name, ios_base::in);
    if (!cmd_stream)
    { cerr << "Input file \"" << vars_show_file_name << "\" not found." << endl;
      return 1;
    }
  }
  else cmd_stream = (ifstream*)&cin;

  bool finish = false; //indicates end of simulation
  bool print_trans = true; //indicates if new state and transitions must be written
  bool init_state = true; //Am I in the initial state? (for good write of trace)
  char cmd[MAX_CMD_LENGTH]; //string of user's command
  cmd[0]='\0';
  succ_container_t succs; //list of enabled successors states
  enabled_trans_container_t enabled_trans(System);
  const std::size_t print_mode = ES_FMT_PRINT_VAR_NAMES |
                                 ES_FMT_PRINT_STATE_NAMES |  ES_FMT_PRINT_PROCESS_NAMES;

  string *trans_name;
  int succs_result;
  do //main loop
  { 
    if (print_trans)
    { 
      try
      {
        succs_result = System.get_succs(s, succs, enabled_trans);
      }
      catch (ERR_throw_t)
      {
        for (ERR_nbr_t i = 0; i!=gerr.count(); i++)
        { gerr.perror("Successor error",i); }
        gerr.clear();
        return 2;
      }
      trans_name=new string[succs.size()]; //to retain transitions' names
      
      if (write_to_cout)
      { cout << "State: " << endl;
        if ((System.get_with_property())&&(System.is_accepting(s))) 
          cout << "(ACC)";
        if (System.is_erroneous(s))
          cout << "(ERR)";
        System.DBG_print_state_CR(s, cout, print_mode);    
        cout << endl;
      }
      for (list<var_print_t>::iterator i = show_vars_list.begin(); i != show_vars_list.end(); ++i)
      {
        if (write_to_cout)
          PrintVar(&cout, &System, &s, &(*i), true);
        PrintVar(&f_trace_out, &System, &s, &(*i), false);
      }
      if ((cmd[0])||(!show_vars_list.empty()))
        f_trace_out << endl;
      
      if ((succs_error(succs_result))||((succs_deadlock(succs_result))&&(enabled_trans.size()==0)))
      { 
	if (write_to_cout)	  
          cout << "System is in " << (succs_error(succs_result)?"an erroneous state":"a deadlock") << ", no successors." << endl;
      }
      else
      { std::size_t proc_gid = 0, trans_nr = enabled_trans.get_count(proc_gid);
        string tmp_s; //to future recognization of transition: name of the proces
        for (std::size_t info_index=0; info_index!=enabled_trans.size();
             info_index++, trans_nr--)
        { const dve_transition_t *trans,*strans,*ptrans;
          std::size_t strans_proc_gid,ptrans_proc_gid;
          if (System.get_system_synchro()==SYSTEM_ASYNC && write_to_cout)
            if ((info_index==0)||(trans_nr==0))
             {
              while (trans_nr==0)
                trans_nr=enabled_trans.get_count(++proc_gid); 
//              cout << info_index << ' ' << proc_gid << ' ' << enabled_trans.get_count(proc_gid) << endl;
              tmp_s = Symbol_table->get_process(proc_gid)->get_name();
              cout << "Transitions of " << (proc_gid+1) << ". process ("
                   << tmp_s << "):" << endl;
             }
          
          trans_name[info_index]= enabled_trans[info_index].to_string();
          
          if (System.get_system_synchro()==SYSTEM_ASYNC)
           {
            trans = System.get_sending_or_normal_trans(enabled_trans[info_index]);
            strans = System.get_receiving_trans(enabled_trans[info_index]); //synchronized transition with any else process?
            ptrans = System.get_property_trans(enabled_trans[info_index]); //synchronized with property automaton (or process)?
            
            if (strans) strans_proc_gid = strans->get_process_gid();
            if (ptrans) ptrans_proc_gid = ptrans->get_process_gid();
         
            if (write_to_cout)
            { cout << info_index << " (" << trans_name[info_index] << "): ";
              trans->write(cout);
              cout << endl;
            }
            
            if ((strans)&&(write_to_cout)) //strans_proc_gid allready set earlier
	      {
		cout << "   - sync. with "<< strans_proc_gid+1 <<". process ("
		     << Symbol_table->get_process(strans_proc_gid)->get_name()
		     << "): " << endl;
		for (unsigned i=0; i<6+trans_name[info_index].length(); i++)
		  cout << ' ';
		strans->write(cout);
		cout << endl;
	      }
            
            if ((ptrans)&&(write_to_cout)) //ptrans_proc_id allready set earlier
              cout << "   - sync. with property (process "
		   << Symbol_table->get_process(ptrans_proc_gid)->get_name()
		   << ")" << endl;
           }
          else
           {
            if (write_to_cout) cout << info_index << " "
                                    << trans_name[info_index] << endl;
           }
          if ((info_index<succs.size()-1)&&(write_to_cout))
            cout << endl;
        } //end of for-cycle
      } //end of "if is_erroneous(s)"
    } //end of "if ((print_trans)&&.."
    
    s2.ptr = 0; //succesor state not yet chosen
    if (write_to_cout)
    { if (succs.size()==0) //deadlock
        cout << "System is in deadlock." << endl;
      cout << "Command:";
    }
    cmd_stream->getline(cmd,MAX_CMD_LENGTH);
    if ((trace_in_file_name)&&(write_to_cout)) //write command readed from input file
    { cout << cmd;
      if ((cmd[0])&&(cmd[0]!='q'))
        cin.get(); //waits for 'Enter' - I don't know how to implement "repeat until keypressed;"
    }
    if (!cmd[0]) //end of input file => quit,
      cmd[0]='q'; //but it is set when Enter is pressed, too
    print_trans = true; //will be set to false iff command unresolved
    string tmp_str = cmd;
    int index = -1;
    if (isdigit(cmd[0]))
    { index = Char2Int(cmd);
    }
    else if (succs.size()>0)
    { do
        index++;
      while (((unsigned)index<succs.size())&&(trans_name[index]!=tmp_str));
    }
    if ((succs.size()>0)&&((unsigned)index<succs.size())) //choosen transition execution
    { s2 = succs[index];
      init_state&=(s2==s);
      f_trace_out << trans_name[index];
      if (!show_vars_list.empty())
      { f_trace_out << '\t';
        if (trans_name[index].length()<8)
          f_trace_out << '\t';
        if (trans_name[index].length()<16)
          f_trace_out << '\t';
        if (trans_name[index].length()<24)
          f_trace_out << '\t';
      }
      if (write_to_cout)
        cout << endl << endl << 
        "--------------------------------------------------------------------------------" << endl; //free lines to parse previous and next step of the execution
    }
    else switch (cmd[0])
    { case cmd_init: { Generator.delete_state(s); //dealocate old state
                       s = System.get_initial_state();
                       if (write_to_cout)
                         cout << "Initial state created." << endl;
                       f_trace_out.close();
                       f_trace_out.open(trace_out_name, ios_base::out);
                       init_state = true;
                       break;
                     }                     
      case cmd_quit: { finish = true; Generator.delete_state(s); /*to dealocate state 's'*/ break; }
      case cmd_rand: { if (succs.size()==0)
                       { cerr << "No successors, can't be executed." << endl;
                         print_trans=false;
                         break;
                       }                    
                       int rnd = rand()%succs.size();
                       s2 = succs[rnd];
                       init_state&=(s2==s);
                       if (write_to_cout)
                         cout << "Choosen transition \"" << trans_name[rnd] << 
                           "\"." << endl << endl << endl << "--------------------------------------------------------------------------------" << endl;
                       f_trace_out << trans_name[rnd];
                       if (!show_vars_list.empty())
                       { f_trace_out << "\t";
                         if (trans_name[rnd].length()<8)
                           f_trace_out << '\t';
                         if (trans_name[rnd].length()<16)
                           f_trace_out << '\t';
                         if (trans_name[rnd].length()<24)
                           f_trace_out << '\t';
                       }
                       break;
                     }
      case cmd_pvar: { // example: "p 2.local" OR "p global" (or "p .global")
                       unsigned pos = 2, proc_index;
                       if (cmd[2]=='*')
                       { PrintAllVars(Symbol_table, &System, &s);
                         break;
                       }
                       else
                       { while ((cmd[pos])&&(cmd[pos]!='.')) //similar to strchr
                           pos++;
                         if (cmd[pos]) //cmd contains '.' => it contains a local variable, thus extract the name of the process
                         { pos++; //begin of the variable name
                           if (!ParseProcGID(cmd+2, pos-2, proc_index, &System, Symbol_table))
                           { cerr << "3: Incorrect command \"" << cmd << "\"." << endl;
                             break;
                           }
                         }
                         else //global variable
                         { proc_index = 0;
                           pos=2; //begin of the variable name
                         }
                       }
                       var_print_t var;
                       if (!ParseVarName(cmd, pos, proc_index, Symbol_table, var))
                       { if (cmd[pos]=='*') //print all variables of this process
                           PrintAllVars(proc_index, Symbol_table, &System, &s);
                         break; //command unresolved
                       }
                       PrintVar(&cout, &System, &s, &var, true);
                       break; 
                     }
      case cmd_svar: { // example: "s 2.local" OR "s global" (or "s .global")
                       unsigned pos = 2, proc_index = 0;
                       if (cmd[2]=='*')
                       { AddAllVars(&System, Symbol_table, &show_vars_list);
                         WriteVarNamesToShow(&show_vars_list,init_state);
                         if (!init_state)
                           f_trace_out << "\t\t\t\t";
                         for (list<var_print_t>::iterator i = 
                          show_vars_list.begin(); i!=show_vars_list.end(); ++i)
                           PrintVar(&cout, &System, &s, &(*i), true);
                         break;
                       }
                       { while ((cmd[pos])&&(cmd[pos]!='.')) //similar to strchr
                           pos++;
                         if (cmd[pos]) //cmd contains '.' => it contains a local variable, thus extract the name of the process
                         { pos++; //begin of the variable name
                           if (!ParseProcGID(cmd+2, pos-2, proc_index, &System, Symbol_table))
                           { cerr << "4: Incorrect command \"" << cmd << "\"." << endl;
                             break;
                           }
                         }
                         else //global variable
                         { proc_index = 0;
                           pos = 2; //begin of the variable name
                         }
                       }
                       var_print_t var;
                       if (!ParseVarName(cmd, pos, proc_index, Symbol_table, var))
                       { if (cmd[pos]=='*')
                         { AddAllVars(proc_index, &System, Symbol_table, &show_vars_list);
                           WriteVarNamesToShow(&show_vars_list,init_state);
                           if (!init_state)
                             f_trace_out << "\t\t\t\t";
                           for (list<var_print_t>::iterator i = 
                            show_vars_list.begin(); i!=show_vars_list.end();++i)
                             PrintVar(&cout, &System, &s, &(*i), true);
                         }
                         break; //command unresolved
                       }
                       if (InsertVarToShow(&show_vars_list, &var, cmd+2))
                       { WriteVarNamesToShow(&show_vars_list,init_state);
                         if (!init_state)
                           f_trace_out << "\t\t\t\t";
                       }
                       if (write_to_cout)
                         PrintVar(&cout, &System, &s, &var, true);
                       break;
                     }
      case cmd_hvar: { 
                       unsigned pos = 2, proc_index = 0;
                       if (cmd[2]=='*')
                       { DeleteVarList(&show_vars_list);
                         WriteVarNamesToShow(&show_vars_list,init_state);
                         if (!init_state)
                           f_trace_out << "\t\t\t\t";
                         break;
                       }
                       { while ((cmd[pos])&&(cmd[pos]!='.')) //similar to strchr
                           pos++;
                         if (cmd[pos]) //cmd contains '.' => it contains a local variable, thus extract the name of the process
                         { pos++; //begin of the variable name
                           if (!ParseProcGID(cmd+2, pos-2, proc_index, &System, Symbol_table))
                           { cerr << "5: Incorrect command \"" << cmd << "\"." << endl;
                              break;
                           } 
                         } 
                         else //global variable
                         { proc_index = 0;
                           pos = 2; //begin of the variable name
                         }
                       }
                       if (cmd[pos]=='*') //delete all local variables of given process
                       { list<var_print_t>::iterator i = show_vars_list.begin();
                         while (i != show_vars_list.end())
                         { if (strncmp(i->name,cmd+2,pos-4)==0)
                           { delete[] (*i).name;
                             show_vars_list.erase(i);
                             i = show_vars_list.begin();
                           }
                           else 
                           { while ((i != show_vars_list.end())
                                    &&(strncmp(i->name,cmd+2,pos-4)!=0))
                             { ++i; }
                           }
                         }
                         WriteVarNamesToShow(&show_vars_list,init_state);
                         if (!init_state)
                           f_trace_out << "\t\t\t\t";
                         break;
                       }
                       var_print_t var;
                       if (!ParseVarName(cmd, pos, proc_index, Symbol_table, var))
                          break; //command unresolved
                       list<var_print_t>::iterator i = show_vars_list.begin();
                       while ((i != show_vars_list.end())
                             &&((i->gid!=var.gid)||(i->vect_index!=var.vect_index)||(i->is_state!=var.is_state)))
                         ++i;
                       if (i == show_vars_list.end())
                       { cerr << "The variable \"" << cmd+2 << "\" is not dumping." << endl;
                       }
                       else
                       { delete[] (*i).name;
                         show_vars_list.erase(i);
                         WriteVarNamesToShow(&show_vars_list,init_state);
                         if (!init_state)
                           f_trace_out << "\t\t\t\t";
                       }
                       break;
                     }
      default: { if (strcmp(input2output_str,cmd)==0) //write this separator string to the output
                 { f_trace_out << input2output_str; //no "<< endl", automatically added
                 }
                 else
                 { cerr << "6: Incorrect command \"" << cmd << "\"." << endl; 
                   print_trans = false;
                 }
               } //unknown or not yet implemented command
    } //end of switch (cmd[0])
    if (print_trans) //some action executed, generate new succs etc. => succs. must be deleted
    { for (size_t i=0; i!=succs.size(); ++i)
        if ((s2.ptr==0)||(succs[i]!=s2))
          Generator.delete_state(succs[i]);
      if ((s2.ptr!=0)&&(s2!=s)) { Generator.delete_state(s); s=s2; }
      delete[] trans_name;
    }
  } //end of while 
  while (!finish); //that means user choose "q" - end of simulation
  //closing tracefile and deleting allocated memory
  if (trace_in_file_name) //close input file of trace
  { cmd_stream->close();
    delete cmd_stream;
  }
  f_trace_out.close();
  delete[] trace_out_name;
  
  //erasing items of the list of showed variables
  DeleteVarList(&show_vars_list);

  //Table.dispose_list_of_varnames(vars);
  DEB(cerr << "Not disposed: " << /*ST_mem_consump <<*/"? bytes from states"<< endl;)
  for (ERR_nbr_t i = 0; i!=gerr.count(); i++)
  { gerr.perror("Parse error",i); }
  gerr.clear();
  DEB(cerr << "Further not disposed: " << error_string_t::allocated_strings() <<) DEB(        " from error_string_t objects." << endl;)
  if (write_to_cout)
    cout << "The end of the program "<<argv[0] << endl;
  return 0;
}
