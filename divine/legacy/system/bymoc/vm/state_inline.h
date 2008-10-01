/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */


// DO NOT INCLUDE THIS FILE - IT IS FOR INTERNAL USE ONLY
#ifdef STATE_INLINE


// size of a process
STATE_INLINE unsigned int process_size( st_process_header *p_proc )
{
  // active process
  if( p_proc->flags & PROCESS_FLAGS_ACTIVE )
  {
    return sizeof( st_process_active_header ) // header
         + ((st_process_active_header *)p_proc)->proc.lvar_sz // variables
         + ((st_process_active_header *)p_proc)->stack_max * sizeof( t_val ); // stack
  }

  // normal process
  return sizeof( st_process_header ) // header
       + p_proc->lvar_sz; // variables
}


// get pointer to variables
STATE_INLINE char * process_get_variables( st_process_header *p_proc )
{
  // active process
  if( p_proc->flags & PROCESS_FLAGS_ACTIVE )
  {
    return (char *)p_proc
         + sizeof( st_process_active_header ); // header
  }

  // normal process
  return (char *)p_proc
       + sizeof( st_process_header ); // header
}


// get pointer to stack
STATE_INLINE ua_t_val * process_active_get_stack( st_process_active_header *p_proc )
{
  return (ua_t_val *)((char *)p_proc
                      + sizeof( st_process_active_header ) // header
                      + p_proc->proc.lvar_sz); // variables
}


// size of a channel
STATE_INLINE unsigned int channel_size( st_channel_header *p_chan )
{
  return sizeof( st_channel_header ) // header
       + p_chan->type_len // type specification
       + max( 1, p_chan->max_len ) * p_chan->msg_len; // messages
}


// get pointer to type
STATE_INLINE int8_t * channel_get_type( st_channel_header *p_chan )
{
  return (int8_t *)((char *)p_chan + sizeof( st_channel_header )); // header
}


// get pointer to message
STATE_INLINE char * channel_get_msg( st_channel_header *p_chan, uint8_t msg_no )
{
  return (char *)p_chan + sizeof( st_channel_header ) // header
                        + p_chan->type_len // type specification
                        + msg_no * p_chan->msg_len; // messages before msg_no
}


// size of a global state without processes and channels
STATE_INLINE unsigned int global_state_size_noproc_nochan( st_global_state_header *p_glob )
{
  return sizeof( st_global_state_header ) // header
       + be2h_16( p_glob->gvar_sz ); // variables
}

// size of a global state
STATE_INLINE size_t global_state_size( st_global_state_header *p_glob )
{
  unsigned int sz, i;

  sz = sizeof( st_global_state_header ) // header
     + be2h_16( p_glob->gvar_sz ); // variables
  for( i = 0; i < p_glob->proc_cnt; i++ ) // processes
    sz += process_size( (st_process_header *)((char *)p_glob + sz) );
  for( i = 0; i < p_glob->chan_cnt; i++ ) // channels
    sz += channel_size( (st_channel_header *)((char *)p_glob + sz) );

  return sz;
}


// get pointer to variables
STATE_INLINE char * global_state_get_variables( st_global_state_header *p_glob )
{
  return (char *)p_glob
       + sizeof( st_global_state_header ); // header
}


// get pointer to processes
STATE_INLINE char * global_state_get_processes( st_global_state_header *p_glob )
{
  char *ptr;

  ptr = (char *)p_glob
      + sizeof( st_global_state_header ) // header
      + be2h_16( p_glob->gvar_sz ); // variables
  return ptr;
}


// get pointer to active process
STATE_INLINE st_process_active_header * global_state_get_active_process( st_global_state_header *p_glob )
{
  char *ptr;
  unsigned int i;

  ptr = (char *)p_glob
      + sizeof( st_global_state_header ) // header
      + be2h_16( p_glob->gvar_sz ); // variables
  for( i = 0; i < p_glob->proc_cnt; i++ ) // search process
  {
    if( ((st_process_header *)ptr)->flags & PROCESS_FLAGS_ACTIVE ) // active process found
      return (st_process_active_header *)ptr;
    ptr += process_size( (st_process_header *)ptr ); // next process
  }
  return NULL; // no active process found
}


// get pointer to process
STATE_INLINE st_process_header * global_state_get_process( st_global_state_header *p_glob, t_pid pid )
{
  char *ptr;
  unsigned int i;

  ptr = (char *)p_glob
      + sizeof( st_global_state_header ) // header
      + be2h_16( p_glob->gvar_sz ); // variables
  for( i = 0; i < p_glob->proc_cnt; i++ ) // search process
  {
    if( ((st_process_header *)ptr)->pid == h2be_pid( pid ) ) // found
      return (st_process_header *)ptr;
    ptr += process_size( (st_process_header *)ptr ); // next process
  }
  return NULL; // not found
}


// get maximum process id
STATE_INLINE t_pid global_state_get_max_pid( st_global_state_header *p_glob )
{
  char *ptr;
  unsigned int i;
  t_pid max_pid;

  ptr = (char *)p_glob
      + sizeof( st_global_state_header ) // header
      + be2h_16( p_glob->gvar_sz ); // variables
  max_pid = 0;
  for( i = 0; i < p_glob->proc_cnt; i++ ) // processes
  {
    max_pid = max( max_pid, be2h_pid( ((st_process_header *)ptr)->pid ) );
    ptr += process_size( (st_process_header *)ptr );
  }
  return max_pid;
}


// get pointer to channels
STATE_INLINE char * global_state_get_channels( st_global_state_header *p_glob )
{
  char *ptr;
  unsigned int i;

  ptr = (char *)p_glob
      + sizeof( st_global_state_header ) // header
      + be2h_16( p_glob->gvar_sz ); // variables
  for( i = 0; i < p_glob->proc_cnt; i++ ) // processes
    ptr += process_size( (st_process_header *)ptr );
  return ptr;
}


// get pointer to channel
STATE_INLINE st_channel_header * global_state_get_channel( st_global_state_header *p_glob, t_chid chid )
{
  char *ptr;
  unsigned int i;

  ptr = (char *)p_glob
      + sizeof( st_global_state_header ) // header
      + be2h_16( p_glob->gvar_sz ); // variables
  for( i = 0; i < p_glob->proc_cnt; i++ ) // processes
    ptr += process_size( (st_process_header *)ptr );
  for( i = 0; i < p_glob->chan_cnt; i++ ) // search channel
  {
    if( ((st_channel_header *)ptr)->chid == h2be_chid( chid ) ) // found
      return (st_channel_header *)ptr;
    ptr += channel_size( (st_channel_header *)ptr ); // next channel
  }
  return NULL; // not found
}


// get new channel id
STATE_INLINE t_chid global_state_get_new_chid( st_global_state_header *p_glob, t_pid pid )
{
  char *ptr;
  unsigned int i;
  t_chid chid_begin, chid, chid_end_max, chid_end;

  chid_begin = (t_chid)pid << 8;

  ptr = (char *)p_glob
      + sizeof( st_global_state_header ) // header
      + be2h_16( p_glob->gvar_sz ); // variables
  for( i = 0; i < p_glob->proc_cnt; i++ ) // processes
    ptr += process_size( (st_process_header *)ptr );
  chid_end_max = 0;
  for( i = 0; i < p_glob->chan_cnt; i++ ) // channels
  {
    chid = be2h_chid( ((st_channel_header *)ptr)->chid ); // get chid
    if( (chid & 0xFF00) == chid_begin ) // compare begin of chid (pid)
      chid_end_max = max( chid_end_max, chid & 0x00FF ); // save maximum end part
    ptr += channel_size( (st_channel_header *)ptr );
  }

  chid_end = chid_end_max + 1; // generate new chid
  if( chid_end > 0x00FF ) // all end parts used
    return 0; // no chid
  return chid_begin | chid_end; // return new chid
}


// check if synchronous communication is occuring in at least one channel
STATE_INLINE int global_state_sync_comm( st_global_state_header *p_glob )
{
  char *ptr;
  unsigned int i;

  ptr = (char *)p_glob
      + sizeof( st_global_state_header ) // header
      + be2h_16( p_glob->gvar_sz ); // variables
  for( i = 0; i < p_glob->proc_cnt; i++ ) // processes
    ptr += process_size( (st_process_header *)ptr );
  for( i = 0; i < p_glob->chan_cnt; i++ ) // channels
  {
    if( ((st_channel_header *)ptr)->cur_len > ((st_channel_header *)ptr)->max_len )
      return 1; // found a channel that contains more messages than max_len
                // ---> synchronous communication is occuring
    ptr += channel_size( (st_channel_header *)ptr );
  }
  return 0; // all channels do not contain more than max_len messages
            // ---> no synchronous communication
}


/* deprecated */ STATE_INLINE int
global_state_compare (void *p_glob1, void *p_glob2)
{
	// see nipsvm_state_compare
	return memcmp (p_glob1, p_glob2, global_state_size ((st_global_state_header *)p_glob1));
}

#endif // #ifdef STATE_INLINE

