// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-

/* tracer - Utility for printing UPPAAL XTR trace files.
   Copyright (C) 2006 Uppsala University and Aalborg University.
   
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

#include <cstdio>
#include <climits>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <map>

using namespace std;

/* This utility takes an UPPAAL model in the UPPAAL intermediate
 * format and a UPPAAL XTR trace file and prints trace to stdout in a
 * human readable format.
 *
 * The utility basically contains two parsers: One for the
 * intermediate format and one for the XTR format. You may want to use
 * them a starting point for writing analysis tools.
 *
 * Notice that the intermediate format uses a global numbering of
 * clocks, variables, locations, etc. This is in contrast to the XTR
 * format, which makes a clear distinction between e.g. clocks and
 * variables and uses process local number of locations and
 * edges. Care must be taken to convert between these two numbering
 * schemes.
 */

enum type_t { CONST, CLOCK, VAR, META, COST, LOCATION, FIXED };
enum flags_t { NONE, COMMITTED, URGENT };

/* Representation of a memory cell.
 */
struct cell_t
{
    /** The type of the cell. */
    type_t type;

    /** Name of cell. Not all types have names. */
    string name;

    union 
    {
        int value;
        struct 
        {
            int nr;
        } clock;
        struct
        {
            int min;
            int max;
            int init;
            int nr;
        } var;
        struct
        {
            int min;
            int max;
            int init;
            int nr;
        } meta;
        struct
        {
            int flags;
            int process;
            int invariant;
        } location;
        struct 
        {
            int min;
            int max;
        } fixed;
    };
};

/* Representation of a process.
 */
struct process_t
{
    int initial;
    string name;
    vector<int> locations;
    vector<int> edges;
};

/* Representation of an edge.
 */
struct edge_t
{
    int process;
    int source;
    int target;
    int guard;
    int sync;
    int update;
};

/* The UPPAAL model in intermediate format.
 */
vector<cell_t> layout;
vector<int> instructions;
vector<process_t> processes;
vector<edge_t> edges;
map<int,string> expressions;

/* For convenience we keep the size of the system here.
 */
static size_t processCount = 0;
static size_t variableCount = 0;
static size_t clockCount = 0;

/* These are mappings from variable and clock indicies to
 * the names of these variables and clocks.
 */
static vector<string> clocks;
static vector<string> variables;

/* Thrown by parser upon parse errors.
 */
class invalid_format : public std::runtime_error
{
public:
    explicit invalid_format(const string&  arg);
};

invalid_format::invalid_format(const string&  arg) : runtime_error(arg)
{

}

/* Reads one line from file. Skips comments.
 */
bool read(FILE *file, char *str, size_t n)
{
    do 
    {
        if (fgets(str, n, file) == NULL)
        {
            return false;
        }
    } while (str[0] == '#');
    return true;
}

/* Parser for intermediate format.
 */
void loadIF(FILE *file)
{
    char str[255];
    char section[16];
    char name[32];
    int index;

    while (fscanf(file, "%15s\n", section) == 1)
    {
        if (strcmp(section, "layout") == 0)
        {
            while (read(file, str, 255) && !isspace(str[0]))
            {
                char s[5];
                cell_t cell;
                
                if (sscanf(str, "%d:clock:%d:%31s", &index, 
                           &cell.clock.nr, name) == 3)
                {
                    cell.type = CLOCK;
                    cell.name = name;
                    clocks.push_back(name);
                    clockCount++;
                }
                else if (sscanf(str, "%d:const:%d", &index, 
                                &cell.value) == 2)
                {
                    cell.type = CONST;
                }
                else if (sscanf(str, "%d:var:%d:%d:%d:%d:%31s", &index, 
                                &cell.var.min, &cell.var.max, &cell.var.init, 
                                &cell.var.nr, name) == 6)
                {
                    cell.type = VAR;
                    cell.name = name;
                    variables.push_back(name);
                    variableCount++;
                }
                else if (sscanf(str, "%d:meta:%d:%d:%d:%d:%31s", &index,
                                &cell.meta.min, &cell.meta.max, &cell.meta.init,
                                &cell.meta.nr, name) == 6)
                {
                    cell.type = META;
                    cell.name = name;
                    variables.push_back(name);
                    variableCount++;
                }
                else if (sscanf(str, "%d:location::%31s", &index, name) == 2)
                {
                    cell.type = LOCATION;
                    cell.location.flags = NONE;
                    cell.name = name;
                }
                else if (sscanf(str, "%d:location:committed:%31s", &index, name) == 2)
                {
                    cell.type = LOCATION;
                    cell.location.flags = COMMITTED;
                    cell.name = name;
                }
                else if (sscanf(str, "%d:location:urgent:%31s", &index, name) == 2)
                {
                    cell.type = LOCATION;
                    cell.location.flags = URGENT;
                    cell.name = name;
                }
                else if (sscanf(str, "%d:static:%d:%d:%31s", &index,
                                &cell.fixed.min, &cell.fixed.max, 
                                name) == 4)
                {
                    cell.type = FIXED;
                    cell.name = name;
                }
                else if (sscanf(str, "%d:%5s", &index, s) == 2
                         && strcmp(s, "cost") == 0)
                {
                    cell.type = COST;
                }
                else 
                {
                    throw invalid_format(str);
                }

                layout.push_back(cell);
            }
        }
        else if (strcmp(section, "instructions") == 0)
        {
            while (read(file, str, 255) && !isspace(str[0]))
            {
                int address;
                int values[4];
                int cnt = sscanf(
                    str, "%d:%d%d%d%d", &address, 
                    values + 0, values + 1, values + 2, values + 4);
                if (cnt < 2)
                {
                    throw invalid_format("In instruction section");
                }

                for (int i = 0; i < cnt; i++)
                {
                    instructions.push_back(values[i]);
                }
            }
        }
        else if (strcmp(section, "processes") == 0)
        {
            while (read(file, str, 255) && !isspace(str[0]))
            {
                process_t process;
                if (sscanf(str, "%d:%d:%31s", 
                           &index, &process.initial, name) != 3)
                {
                    throw invalid_format("In process section");
                }
                process.name = name;
                processes.push_back(process);
                processCount++;
            }
        }
        else if (strcmp(section, "locations") == 0)
        {
            while (read(file, str, 255) && !isspace(str[0]))
            {
                int index;
                int process;
                int invariant;

                if (sscanf(str, "%d:%d:%d", &index, &process, &invariant) != 3)
                {
                    throw invalid_format("In location section");
                }

                layout[index].location.process = process;
                layout[index].location.invariant = invariant;
                processes[process].locations.push_back(index);
            }
        }
        else if (strcmp(section, "edges") == 0)
        {
            while (read(file, str, 255) && !isspace(str[0]))
            {
                edge_t edge;

                if (sscanf(str, "%d:%d:%d:%d:%d:%d", &edge.process, 
                           &edge.source, &edge.target,
                           &edge.guard, &edge.sync, &edge.update) != 6)
                {
                    throw invalid_format("In edge section");
                }                

                processes[edge.process].edges.push_back(edges.size());
                edges.push_back(edge);
            }
        }
        else if (strcmp(section, "expressions") == 0)
        {
            while (read(file, str, 255) && !isspace(str[0]))
            {
                if (sscanf(str, "%d", &index) != 1)
                {
                    throw invalid_format("In expression section");
                }                    
                
                /* Find expression string (after the third colon).
                 */
                char *s = str;
                int cnt = 3;
                while (cnt && *s)
                {
                    cnt -= (*s == ':');
                    s++;
                }
                if (cnt)
                {
                    throw invalid_format("In expression section");
                }

                /* Trim white space. 
                 */
                while (*s && isspace(*s)) 
                {
                    s++;
                }
                char *t = s + strlen(s) - 1;
                while (t >= s && isspace(*t)) 
                {
                    t--;
                }
                t[1] = '\0';

                expressions[index] = s;
            }
        }
        else 
        {
            throw invalid_format("Unknown section");
        }
    }  
};

/* A bound for a clock constraint. A bound consists of a value and a
 * bit indicating whether the bound is strict or not.
 */
struct bound_t
{
    int value   : 31; // The value of the bound
    bool strict : 1;  // True if the bound is strict
};

/* The bound (infinity, <).
 */
static bound_t infinity = { INT_MAX >> 1, 1 };

/* The bound (0, <=).
 */
static bound_t zero = { 0, 0 };

/* A symbolic state. A symbolic state consists of a location vector, a
 * variable vector and a zone describing the possible values of the
 * clocks in a symbolic manner.
 */
class State
{
public:
    State();
    State(FILE *);
    ~State();
    
    int &getLocation(int i)              { return locations[i]; }
    int &getVariable(int i)              { return integers[i]; }
    bound_t &getConstraint(int i, int j) { return dbm[i * clockCount + j]; }

    int getLocation(int i) const              { return locations[i]; }
    int getVariable(int i) const              { return integers[i]; }
    bound_t getConstraint(int i, int j) const { return dbm[i * clockCount + j]; }
private:
    int *locations;
    int *integers;
    bound_t *dbm;
    void allocate();
};

State::~State()
{
    delete[] dbm;
    delete[] integers;
    delete[] locations;
}

State::State(FILE *file)
{
    allocate();

    /* Read locations.
     */
    for (size_t i = 0; i < processCount; i++)
    {
        fscanf(file, "%d\n", &getLocation(i));
    }
    fscanf(file, ".\n");

    /* Read DBM.
     */
    int i, j, bnd;
    while (fscanf(file, "%d\n%d\n%d\n.\n", &i, &j, &bnd) == 3)
    {
        getConstraint(i, j).value = bnd >> 1;
        getConstraint(i, j).strict = bnd & 1;
    }
    fscanf(file, ".\n");    

    /* Read integers.
     */
    for (size_t i = 0; i < variableCount; i++) 
    {
        fscanf(file, "%d\n", &getVariable(i));
    }
    fscanf(file, ".\n");
}

void State::allocate()
{
    /* Allocate.
     */
    locations = new int[processCount];
    integers = new int[variableCount];
    dbm = new bound_t[clockCount * clockCount];
    
    /* Fill with default values.
     */
    fill(locations, locations + processCount, 0);
    fill(integers, integers + variableCount, 0);
    fill(dbm, dbm + clockCount * clockCount, infinity);

    /* Set diagonal and lower bounds to zero.
     */
    for (size_t i = 0; i < clockCount; i++) 
    {
        getConstraint(0, i) = zero;
        getConstraint(i, i) = zero;
    }
}

/* A transition consists of one or more edges. Edges are indexes from
 * 0 in the order they appear in the input file.
 */
class Transition
{
public:
    Transition(FILE *);
    ~Transition();

    int getEdge(int32_t process) const { return edges[process]; }
private:
    int *edges;
};

Transition::Transition(FILE *file)
{
    edges = new int[processCount];
    fill(edges, edges + processCount, -1);

    int process, edge;
    while (fscanf(file, "%d %d.\n", &process, &edge) == 2) 
    {
        edges[process] = edge - 1;
    }
    fscanf(file, ".\n");
}

Transition::~Transition()
{
    delete[] edges;
}

/* Output operator for a symbolic state. Prints the location vector,
 * the variables and the zone of the symbolic state.
 */
ostream &operator << (ostream &o, const State &state)
{
    /* Print location vector.
     */
    for (size_t p = 0; p < processCount; p++)
    {
        int idx = processes[p].locations[state.getLocation(p)];
        cout << processes[p].name << '.' << layout[idx].name << " ";
    }

    /* Print variables.
     */
    for (size_t v = 0; v < variableCount; v++) 
    {
        cout << variables[v] << " = " << state.getVariable(v) << ' ';
    }
  
    /* Print clocks.
     */
    for (size_t i = 0; i < clockCount; i++) 
    {
        for (size_t j = 0; j < clockCount; j++) 
        {
            if (i != j) 
            {
                bound_t bnd = state.getConstraint(i, j);

                if (bnd.value != infinity.value) 
                {
                    cout << clocks[i] << "-" << clocks[j] 
                         << (bnd.strict ? "<" : "<=") << bnd.value << " ";
                }
            }
        }
    }
  
    return o;
}

/* Output operator for a transition. Prints all edges in the
 * transition including the source, destination, guard,
 * synchronisation and assignment.
 */
ostream &operator << (ostream &o, const Transition &t)
{
    for (size_t p = 0; p < processCount; p++) 
    {
        int idx = t.getEdge(p);
        if (idx > -1) 
        {
            int edge = processes[p].edges[idx];
            int src = edges[edge].source;
            int dst = edges[edge].target;
            int guard = edges[edge].guard;
            int sync = edges[edge].sync;
            int update = edges[edge].update;

            cout << processes[p].name << '.' << layout[src].name 
                 << " -> "
                 << processes[p].name << '.' << layout[dst].name 
                 << " {"
                 << expressions[guard] << "; " << expressions[sync] << "; " << expressions[update]
                 << ";} ";
        }
    }

    return o;
}

/* Read and print a trace file.
 */
void loadTrace(FILE *file)
{
    /* Read and print trace.
     */
    cout << "State: " << State(file) << endl;
    for (;;) 
    {
        int c;
        /* Skip white space.
         */
        do {
            c = fgetc(file);
        } while (isspace(c));

        /* A dot terminates the trace.
         */
        if (c == '.') 
        {
            break;
        }

        /* Put the character back into the stream.
         */
        ungetc(c, file);

        /* Read a state and a transition.
         */
        State state(file);
        Transition transition(file);

        /* Print transition and state.
         */
        cout << endl << "Transition: " << transition << endl
             << endl << "State: " << state << endl;
    }
}


int main(int argc, char *argv[])
{
    FILE *file;
    try 
    {
        if (argc < 3) 
        {
            printf("Synopsis: %s <if> <trace>\n", argv[0]);
            exit(1);
        }

        /* Load model in intermediate format.
         */
        if (strcmp(argv[1], "-") == 0)
        {
            loadIF(stdin);
        }
        else
        {
            file = fopen(argv[1], "r");
            if (file == NULL)
            {
                perror(argv[0]);
                exit(1);
            }
            loadIF(file);
            fclose(file);
        }

        /* Load trace.
         */
        file = fopen(argv[2], "r");
        if (file == NULL)
        {
            perror(argv[0]);
            exit(1);
        }
        loadTrace(file);
        fclose(file);
    }
    catch (exception &e)
    {
        cerr << "Catched exception: " << e.what() << endl;
    }
}
