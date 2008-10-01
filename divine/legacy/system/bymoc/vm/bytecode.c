/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bytecode.h"
#include "tools.h"


#ifdef EPROTO
#  define E_BC_INV_FMT EPROTO
#else
#  define E_BC_INV_FMT EINVAL
#endif


// load bytecode module from file - check module name
// returns -1 of error, 0 if not equal, 1 if equal
static int bytecode_load_from_file_check_module( FILE *file, const char *module )
{
  // read size of module name
  uint16_t mod_name_sz;
  if( fread( &mod_name_sz, 1, sizeof( mod_name_sz ), file ) != sizeof( mod_name_sz ) )
  {
    errno = E_BC_INV_FMT;
    return -1;
  }
  mod_name_sz = be2h_16( mod_name_sz );
  if( mod_name_sz == 0 )
  {
    errno = E_BC_INV_FMT;
    return -1;
  }

  // read module name
  char mod_name[mod_name_sz];
  if( fread( mod_name, 1, mod_name_sz, file ) != mod_name_sz )
  {
    errno = E_BC_INV_FMT;
    return -1;
  }
  mod_name[mod_name_sz - 1] = 0; // terminate string (should already be terminated there)

  // compare module name (NULL matches any name)
  return module == NULL || strcmp( mod_name, module ) == 0;
}


// load bytecode module from file - read a string
static char * bytecode_load_from_file_string( FILE *file )
{
  // read size of string
  uint16_t str_sz;
  if( fread( &str_sz, 1, sizeof( str_sz ), file ) != sizeof( str_sz ) )
  {
    errno = E_BC_INV_FMT;
    return NULL;
  }
  str_sz = be2h_16( str_sz );
  if( str_sz == 0 )
  {
    errno = E_BC_INV_FMT;
    return NULL;
  }

  // allocate memory
  char *str = (char *)malloc( str_sz );
  if( str == NULL )
  {
    errno = ENOMEM;
    return NULL;
  }

  // read string
  if( fread( str, 1, str_sz, file ) != str_sz )
  {
    free( str );
    errno = E_BC_INV_FMT;
    return NULL;
  }
  str[str_sz - 1] = 0; // terminate string (should already be terminated there)

  return str;
}


// load bytecode module from file - process module
static st_bytecode * bytecode_load_from_file_module( FILE *file )
{
  // read number of parts
  uint16_t part_cnt;
  if( fread( &part_cnt, 1, sizeof( part_cnt ), file ) != sizeof( part_cnt ) )
  {
    errno = E_BC_INV_FMT;
    return NULL;
  }
  part_cnt = be2h_16( part_cnt );

  // allocate structure
  st_bytecode *bytecode = (st_bytecode *)malloc( sizeof( st_bytecode ) );
  if( bytecode == NULL )
  {
    errno = ENOMEM;
    return NULL;
  }
  // initialize structure
  bytecode->modflags = 0;
  bytecode->size = 0;
  bytecode->ptr = NULL;
  bytecode->flag_cnt = 0;
  bytecode->flags = NULL;
  bytecode->string_cnt = 0;
  bytecode->strings = NULL;
  bytecode->src_loc_cnt = 0;
  bytecode->src_locs = NULL;
  bytecode->str_inf_cnt = 0;
  bytecode->str_infs = NULL;

  uint16_t part_no;
  for( part_no = 0; part_no < part_cnt; part_no++ )
  {
    // read part type and part size
    char part_type[4];
    uint32_t part_sz;
    if( fread( &part_type, 1, sizeof( part_type ), file ) != sizeof( part_type ) ||
        fread( &part_sz, 1, sizeof( part_sz ), file ) != sizeof( part_sz ) )
    {
      errno = E_BC_INV_FMT;
      break;
    }
    part_sz = be2h_32( part_sz );

    // module flags
    if( memcmp( part_type, "modf", 4 ) == 0 )
    {
      uint32_t modflags;
      if( fread( &modflags, 1, sizeof( modflags ), file ) != sizeof( modflags ) )
      {
        errno = E_BC_INV_FMT;
        break;
      }
      bytecode->modflags |= be2h_32( modflags );
    }

    // bytecode
    else if( memcmp( part_type, "bc  ", 4 ) == 0 )
    {
      // two bytecodes in one module are invalid
      if( bytecode->ptr != NULL )
      {
        errno = E_BC_INV_FMT;
        break;
      }
      // allocate memory
      bytecode->size = part_sz;
      bytecode->ptr = (uint8_t *)malloc( bytecode->size == 0 ? 1 : bytecode->size );
      if( bytecode->ptr == NULL )
      {
        errno = ENOMEM;
        break;
      }
      // read bytecode
      if( fread( bytecode->ptr, 1, bytecode->size, file ) != bytecode->size )
      {
        errno = E_BC_INV_FMT;
        break;
      }
    }

    // flag table
    else if( memcmp( part_type, "flag", 4 ) == 0 )
    {
      // two flag tables in one module are invalid      
      if( bytecode->flags != NULL )
      {
        errno = E_BC_INV_FMT;
        break;
      }
      // read flag count
      uint16_t flag_cnt;
      if( fread( &flag_cnt, 1, sizeof( flag_cnt ), file ) != sizeof( flag_cnt ) )
      {
        errno = E_BC_INV_FMT;
        break;
      }
      bytecode->flag_cnt = be2h_16( flag_cnt );
      // allocate memory
      bytecode->flags = (st_bc_flag *)malloc( bytecode->flag_cnt == 0 ? 1 : bytecode->flag_cnt * sizeof( st_bc_flag ) );
      if( bytecode->flags == NULL )
      {
        errno = ENOMEM;
        break;
      }

      // read flag entries
      uint16_t i;
      for( i = 0; i < bytecode->flag_cnt; i++ )
      {
        uint32_t addr, flags;
        if( fread( &addr, 1, sizeof( addr ), file ) != sizeof( addr ) ||
            fread( &flags, 1, sizeof( flags ), file ) != sizeof( flags ) )
        {
          errno = E_BC_INV_FMT;
          break;
        }
        bytecode->flags[i].addr = be2h_32( addr );
        bytecode->flags[i].flags = be2h_32( flags );
      }
      if( i < bytecode->flag_cnt )
        break;
    }

    // string table
    else if( memcmp( part_type, "str ", 4 ) == 0 )
    {
      // two string tables in one module are invalid      
      if( bytecode->strings != NULL )
      {
        errno = E_BC_INV_FMT;
        break;
      }
      // read string count
      uint16_t string_cnt;
      if( fread( &string_cnt, 1, sizeof( string_cnt ), file ) != sizeof( string_cnt ) )
      {
        errno = E_BC_INV_FMT;
        break;
      }
      bytecode->string_cnt = be2h_16( string_cnt );
      // allocate memory
      bytecode->strings = (char **)malloc( bytecode->string_cnt == 0 ? 1 : bytecode->string_cnt * sizeof( char * ) );
      if( bytecode->strings == NULL )
      {
        errno = ENOMEM;
        break;
      }
      uint16_t i;
      for( i = 0; i < bytecode->string_cnt; i++ )
        bytecode->strings[i] = NULL;

      // read strings
      for( i = 0; i < bytecode->string_cnt; i++ )
      {
        bytecode->strings[i] = bytecode_load_from_file_string( file );
        if( bytecode->strings[i] == NULL )
          break;
      }
      if( i < bytecode->string_cnt )
        break;
    }

    // source location table
    else if( memcmp( part_type, "sloc", 4 ) == 0 )
    {
      // two source location tables in one module are invalid      
      if( bytecode->src_locs != NULL )
      {
        errno = E_BC_INV_FMT;
        break;
      }
      // read source location count
      uint16_t src_loc_cnt;
      if( fread( &src_loc_cnt, 1, sizeof( src_loc_cnt ), file ) != sizeof( src_loc_cnt ) )
      {
        errno = E_BC_INV_FMT;
        break;
      }
      bytecode->src_loc_cnt = be2h_16( src_loc_cnt );
      // allocate memory
      bytecode->src_locs = (st_src_loc *)malloc( bytecode->src_loc_cnt == 0 ? 1 : bytecode->src_loc_cnt * sizeof( st_src_loc ) );
      if( bytecode->src_locs == NULL )
      {
        errno = ENOMEM;
        break;
      }

      // read source locations
      uint16_t i;
      for( i = 0; i < bytecode->src_loc_cnt; i++ )
      {
        uint32_t addr, line, col;
        if( fread( &addr, 1, sizeof( addr ), file ) != sizeof( addr ) ||
            fread( &line, 1, sizeof( line ), file ) != sizeof( line ) ||
            fread( &col, 1, sizeof( col ), file ) != sizeof( col ) )
        {
          errno = E_BC_INV_FMT;
          break;
        }
        bytecode->src_locs[i].addr = be2h_32( addr );
        bytecode->src_locs[i].line = be2h_32( line );
        bytecode->src_locs[i].col = be2h_32( col );
      }
      if( i < bytecode->src_loc_cnt )
        break;
    }

    // structure information table
    else if( memcmp( part_type, "stin", 4 ) == 0 )
    {
      // two source location tables in one module are invalid      
      if( bytecode->str_infs != NULL )
      {
        errno = E_BC_INV_FMT;
        break;
      }

      // read structure information count
      uint16_t str_inf_cnt;
      if( fread( &str_inf_cnt, 1, sizeof( str_inf_cnt ), file ) != sizeof( str_inf_cnt ) )
      {
        errno = E_BC_INV_FMT;
        break;
      }
      bytecode->str_inf_cnt = be2h_16( str_inf_cnt );

      // allocate memory
      bytecode->str_infs = (st_str_inf *)malloc( bytecode->str_inf_cnt == 0 ? 1 : bytecode->str_inf_cnt * sizeof( st_str_inf ) );
      if( bytecode->src_locs == NULL )
      {
        errno = ENOMEM;
        break;
      }
      
      // read structure information
      uint16_t i;
      for( i = 0; i < bytecode->str_inf_cnt; i++ )
      {
        uint32_t addr;
        uint8_t code;
        if ( fread( &addr, 1, sizeof( addr ), file ) != sizeof( addr ) ||
        	   fread( &code, 1, sizeof( code ), file ) != sizeof( code ))
        {
          errno = E_BC_INV_FMT;
          break;
        }
        
        // read type
        char *type = bytecode_load_from_file_string( file );
        if( type == NULL )
        {
          errno = E_BC_INV_FMT;
          break;
        }

        // read name        
        char *name = bytecode_load_from_file_string( file );
        if( name == NULL )
        {
          errno = E_BC_INV_FMT;
          break;
        }
        
        bytecode->str_infs[i].addr = be2h_32( addr );
        bytecode->str_infs[i].code = code;
        bytecode->str_infs[i].type = type;
        bytecode->str_infs[i].name = name;
      }
      if( i < bytecode->str_inf_cnt )
        break;
    }      
    	
    // SCC mapping table
    else if( memcmp( part_type, "scc ", 4 ) == 0 ) {
      // two SCC mapping tables in one module are invalid      
      if (bytecode->scc_info.types != NULL) {
          errno = E_BC_INV_FMT;
          break;
      }
      // read scc type count
      uint16_t scc_types_cnt;
      if( fread( &scc_types_cnt, 1, sizeof scc_types_cnt, file )
          != sizeof scc_types_cnt) {
          errno = E_BC_INV_FMT;
          break;
      }
      bytecode->scc_info.types_cnt = be2h_16 (scc_types_cnt);
      // allocate memory
      bytecode->scc_info.types = malloc (min(1,bytecode->scc_info.types_cnt) * sizeof (t_scc_type));
      if( bytecode->scc_info.types == NULL ) {
          errno = ENOMEM;
          break;
      }

      // read scc types
      int weak_graph = 1;
      uint16_t i;
      for( i = 0; i < bytecode->scc_info.types_cnt; i++ ) {
          t_scc_type type;
          if (fread (&type, 1, sizeof type, file) != sizeof type) {
              errno = E_BC_INV_FMT;
              break;
          }
          if (type == NIPS_SCC_PARTIALLY_ACCEPTING) weak_graph = 0;
          bytecode->scc_info.types[i] = type;
      }
      if (i < bytecode->scc_info.types_cnt) {
          errno = E_BC_INV_FMT;
          break;
      }
      bytecode->scc_info.weak = weak_graph;

      // read scc map count
      uint16_t scc_map_cnt;
      if( fread( &scc_map_cnt, 1, sizeof scc_map_cnt, file )
          != sizeof scc_map_cnt) {
          errno = E_BC_INV_FMT;
          break;
      }
      bytecode->scc_info.map_cnt = be2h_16 (scc_map_cnt);
      // allocate memory
      bytecode->scc_info.map =
          malloc (min(1, bytecode->scc_info.map_cnt) * sizeof (st_scc_map));
      if( bytecode->scc_info.map == NULL ) {
          errno = ENOMEM;
          break;
      }
      // read scc map
      uint16_t j;
      for (j = 0; j < bytecode->scc_info.map_cnt; j++) {
          uint32_t addr, scc_id;
          if (fread (&addr, 1, sizeof addr, file) != sizeof addr ||
              fread (&scc_id, 1, sizeof scc_id, file) != sizeof scc_id) {
              errno = E_BC_INV_FMT;
              break;
          }
          bytecode->scc_info.map[j].addr = be2h_32(addr);
          bytecode->scc_info.map[j].id   = be2h_32(scc_id);
      }
      if (j < bytecode->scc_info.map_cnt) {
          errno = E_BC_INV_FMT;
          break;
      }
    }

    // unknown part
    else
      fseek( file, part_sz, SEEK_CUR );

  } // for( part_no ...

  // no bytecode found
  if( bytecode->ptr == NULL )
  {
    errno = ESRCH;
    // mark as error
    part_no = 0;
    part_cnt = 0;
  }

  // some error occured in loop
  if( part_no < part_cnt || part_cnt == 0 )
  {
    // cleanup and return NULL (errno is already set)
    if( bytecode->ptr != NULL )
      free( bytecode->ptr );
    if( bytecode->flags != NULL )
      free( bytecode->flags );
    if( bytecode->strings != NULL )
    {
      uint16_t i;
      for( i = 0; i < bytecode->string_cnt; i++ )
        if( bytecode->strings[i] != NULL )
          free( bytecode->strings[i] );
      free( bytecode->strings );
    }
    if( bytecode->src_locs != NULL )
      free( bytecode->src_locs );
    free( bytecode );
    return NULL;
  }

  return bytecode;
}


// load bytecode module from file - process sections
static st_bytecode * bytecode_load_from_file_sections( FILE *file, const char *module )
{
  // get number of sections in file
  uint16_t sec_cnt;
  if( fread( &sec_cnt, 1, sizeof( sec_cnt ), file ) != sizeof( sec_cnt ) )
  {
    errno = E_BC_INV_FMT;
    return NULL;
  }
  sec_cnt = be2h_16( sec_cnt );

  // read sections
  uint16_t sec_no;
  for( sec_no = 0; sec_no < sec_cnt; sec_no++ )
  {

    // read section type and section size
    char sec_type[4];
    uint32_t sec_sz;
    if( fread( &sec_type, 1, sizeof( sec_type ), file ) != sizeof( sec_type ) ||
        fread( &sec_sz, 1, sizeof( sec_sz ), file ) != sizeof( sec_sz ) )
    {
      errno = E_BC_INV_FMT;
      return NULL;
    }
    sec_sz = be2h_32( sec_sz );

    // module section
    if( memcmp( sec_type, "mod ", 4 ) == 0 )
    {
      // remember position in file
      long pos = ftell( file );

      // check if module name matches
      int result = bytecode_load_from_file_check_module( file, module );
      if( result < 0 ) // error
        return NULL;
      if( result > 0 ) // module name matches
        return bytecode_load_from_file_module( file );

      // skip module
      fseek( file, pos + sec_sz, SEEK_SET );
    }

    // unknown section
    else
      fseek( file, sec_sz, SEEK_CUR );

  } // for( sec_no ...

  errno = ESRCH;
  return NULL;
}

#ifdef NIPS_XXX_GLOBAL_BYTECODE
st_bytecode *global_bytecode = NULL;
#endif

// load bytecode module from file
// if module == NULL the first module is loaded
// returns NULL on error (errno ist set in this case)
st_bytecode * bytecode_load_from_file( const char *filename, const char *module ) // extern
{
  // open bytecode file for reading
  FILE *file = fopen( filename, "rb" );
  if( file == NULL )
    return NULL;

  // magic to check at beginning of bytecode
  static char bc_magic[8] = "NIPS v2 ";

  // read magic
  char magic[sizeof( bc_magic )];
  if( fread( magic, 1, sizeof( magic ), file ) != sizeof( magic ) )
  {
    fclose( file );
    return NULL;
  }

  // check magic
  if( memcmp( bc_magic, magic, sizeof( bc_magic ) ) != 0 )
  {
    fclose( file );
    errno = E_BC_INV_FMT;
    return NULL;
  }

  // process sections
  st_bytecode *bytecode = bytecode_load_from_file_sections( file, module );
  fclose( file );
#ifdef NIPS_XXX_GLOBAL_BYTECODE  
  global_bytecode = bytecode;
#endif
  return bytecode;
}


// unload bytecode
void bytecode_unload( st_bytecode *bytecode ) // extern
{
  free( bytecode->ptr );
  uint16_t i;
  if( bytecode->flags != NULL )
    free( bytecode->flags );
  if( bytecode->strings != NULL )
  {
    for( i = 0; i < bytecode->string_cnt; i++ )
      free( bytecode->strings[i] );
    free( bytecode->strings );
  }
  if( bytecode->src_locs != NULL )
    free( bytecode->src_locs );
  free( bytecode );
}


// check if a monitor process exists in bytecode
int bytecode_monitor_present( st_bytecode *bytecode ) // extern
{
  return (bytecode->modflags & BC_MODFLAG_MONITOR) != 0;
}


// get flags at address (e.g. program counter)
uint32_t bytecode_flags( st_bytecode *bytecode, uint32_t addr ) // extern
{
  int start, end, i;

  start = 0;
  end = bytecode->flag_cnt - 1;
  while( start <= end )
  {
    i = (start + end) / 2;
    if( addr < bytecode->flags[i].addr )
      end = i - 1;
    else if( addr > bytecode->flags[i].addr )
      start = i + 1;
    else
      return bytecode->flags[i].flags;
  }
  return 0;
}


// get source location from address (e.g. program counter)
// TODO: change to same interface as bytecode_str_inf
st_src_loc bytecode_src_loc( st_bytecode *bytecode, uint32_t addr ) // extern
{
  int start, end, i;
  
  if( bytecode->src_loc_cnt < 1 )
    return (st_src_loc){ .addr = addr, .line = -1, .col = -1 };

  start = 0;
  end = bytecode->src_loc_cnt - 1;
  for( ; ; )
  {
    i = (start + end) / 2;
    if( addr < bytecode->src_locs[i].addr )
    {
      if( i - 1 < start )
        return bytecode->src_locs[i > 0 ? i - 1 : 0];
      end = i - 1;
    }
    else if( addr > bytecode->src_locs[i].addr )
    {
      if( i + 1 > end )
        return bytecode->src_locs[end];
      start = i + 1;
    }
    else
      return bytecode->src_locs[i];
  }
}

// get scc map from address (e.g. program counter)
st_scc_map *
bytecode_scc_map (st_bytecode *bytecode, uint32_t addr) // extern
{
    int start, end;
  
    start = 0;
    end = bytecode->scc_info.map_cnt - 1;
    for ( ; start < end; ) {
        int i = (start + end) / 2;
        if (addr > bytecode->scc_info.map[i].addr) {
            start = i + 1;
        } else if (addr < bytecode->scc_info.map[i].addr) {
            end = i - 1;
        } else {
            return &bytecode->scc_info.map[i];
        }
    }
    return NULL;
}

t_scc_type
bytecode_scc_type (st_bytecode *bytecode, uint32_t id)
{
    if (id < bytecode->scc_info.types_cnt) {
        return bytecode->scc_info.types[id];
    } else {
        return -1;
    }
}

int
bytecode_scc_weak_graph(st_bytecode *bytecode)
{
    return bytecode->scc_info.weak;
}

// get structure information from address (e.g. program counter), NULL if not found

// *start denotes where to start search, and if found, is modified to
// the next valid start position.  Multiple records can be retrieved like this:
//   unsigned int seq = 0;
//   while ((s_inf = bytecode_str_inf (bc, addr, &seq)) != NULL) {
//     /* stuff with *s_inf */
//   }

// ASSUMES str_infs sorted by addr field
// TODO: replace linear search with binary search
extern st_str_inf *
bytecode_str_inf( st_bytecode *bytecode, uint32_t addr, unsigned int *start)
{
  int i;
  
  for(i = *start; i < bytecode->str_inf_cnt; ++i)
  {
      if (bytecode->str_infs[i].addr == addr) {
          *start = i+1;
          return &bytecode->str_infs[i];
      } else if (bytecode->str_infs[i].addr > addr) {
          break;
      }
  }

  return NULL;
}
