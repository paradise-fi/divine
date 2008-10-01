/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#ifndef INC_bytecode
#define INC_bytecode


#ifdef __cplusplus
extern "C"
{
#endif


#include <stdint.h>


// module flags
#define BC_MODFLAG_MONITOR 0x00000001 // if a monitor process exists in bytecode


// type for flags at some address
#define BC_FLAG_PROGRESS 0x00000001
#define BC_FLAG_ACCEPT 0x00000002
typedef struct t_bc_flag
{
  uint32_t addr, flags;
} st_bc_flag;

// type for a source location (mapping of address to line and column in source)
typedef struct t_src_loc
{
  uint32_t addr, line, col;
} st_src_loc;

// type for a structure inforamtion
typedef struct t_str_inf
{
  uint32_t addr;
  uint8_t code;
  char *type, *name;	
} st_str_inf;

// SCC information
typedef int8_t t_scc_type;
enum {
    NIPS_SCC_NOT_ACCEPTING = 0,
    NIPS_SCC_PARTIALLY_ACCEPTING = 1,
    NIPS_SCC_FULLY_ACCEPTING = 2,
};

typedef struct {
    uint32_t addr;
    int32_t id;
} st_scc_map;

// type for pointer to bytecode and its size
typedef struct t_bytecode
{
  uint32_t modflags; // module flags
  uint32_t size; // size of bytecode
  uint8_t *ptr; // pointer to bytecode
  uint16_t flag_cnt; // number of flag entries in table
  st_bc_flag *flags; // flag table
  uint16_t string_cnt; // number of strings in string table
  char **strings; // string table
  uint16_t src_loc_cnt; // number of source locations in table
  st_src_loc *src_locs; // source location table
  uint16_t str_inf_cnt; // number of structure information in table
  st_str_inf *str_infs; // structure information table
  struct {
      int weak;             // control flow graph is weak
                            // if no SCCs are partially accepting
      uint16_t types_cnt;   // number of scc type entries in table
      t_scc_type *types;
      uint16_t map_cnt;     // number of scc map entries in table
      st_scc_map *map;
  } scc_info;
} st_bytecode;


// load bytecode module from file
// if module == NULL the first module is loaded
// returns NULL on error (errno ist set in this case)
extern st_bytecode * bytecode_load_from_file( const char *filename, const char *module );


// unload bytecode
extern void bytecode_unload( st_bytecode *bytecode );


// check if a monitor process exists in bytecode
extern int bytecode_monitor_present( st_bytecode *bytecode );


// get flags at address (e.g. program counter)
extern uint32_t bytecode_flags( st_bytecode *bytecode, uint32_t addr );


// get source location from address (e.g. program counter)
extern st_src_loc bytecode_src_loc( st_bytecode *bytecode, uint32_t addr );

// get structure information from address (e.g. program counter)
extern st_str_inf *bytecode_str_inf( st_bytecode *bytecode, uint32_t addr, unsigned int *start );

// get SCC information from address (e.g. program counter)
extern st_scc_map *bytecode_scc_map (st_bytecode *bytecode, uint32_t addr);

extern t_scc_type bytecode_scc_type (st_bytecode *bytecode, uint32_t id);

#ifdef __cplusplus
} // extern "C"
#endif


#endif // #ifndef INC_bytecode
