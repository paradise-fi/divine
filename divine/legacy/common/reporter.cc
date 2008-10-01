
#include "common/reporter.hh"
#include <iomanip>
#include <fstream>
#include <cmath>

using namespace divine;
using namespace std;


void reporter_t::print(ostream& out) {

  out << "File:"<<file_name<<endl;
  out << "Algorithm:"<<alg_name<<endl;
  out << "Problem:"<<problem<<endl;
  out << "Workstations:1"<<endl;
  out << setiosflags (ios_base::fixed);
  out <<setprecision(6);
  out << "Time:"<<time<<endl;  
  vm_info.scan();
  out <<setprecision(0);
  out << "VMsize:"<< vm_info.vmsize/1024.0 << endl;  //vm_info.vmsize is in kB, we want MB, thus "/1024.0"
  //  out << "Memory (VM RSS):\t"<< vm_info.vmrss<<endl;
  //  out << "Memory (VM data):\t"<< vm_info.vmdata<<endl;
  for(map<string,double>::iterator i = specific_info.begin(); i!=specific_info.end(); i++) {
    if ((*i).second == floor((*i).second)) 
      out<< setprecision(0);
    else out <<setprecision(6);
    out << (*i).first<<":"<<(*i).second<<endl;
  }
}

void reporter_t::print() {
  ofstream out;
  string rep_file = file_name+".report";
  out.open(rep_file.c_str());
  print(out);
  out.close();
}
