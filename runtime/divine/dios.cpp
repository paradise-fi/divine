// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <divine/dios.h>
#include <tuple>
#include <utility>
#include <cstring>
#include <cstdarg>
#include <cstdio>

namespace dios {

using ThreadId = _DiOS_ThreadId;
using FunPtr   = _DiOS_FunPtr;
struct Scheduler;

struct Syscall {
	Syscall() : _syscall( Type::INACTIVE ) {
		__vm_trace( "Syscall constructor" );
	}

	static ThreadId start_thread( FunPtr routine, void *arg, FunPtr cleanup ) {
		InterruptMask mask;
		__vm_trace( "Start thread issued" );
		auto& inter = get();
		__dios_assert_v( inter._syscall == Type::INACTIVE, "Overlapped syscall" );
		__dios_assert_v( routine, "Invalid thread routine" );
		inter._syscall = Type::START_THREAD;
		inter._args.start_thread = Args::StartThread{ routine, arg, cleanup };
		__vm_trace( "Before interrupt" );
		__dios_interrupt();
		__vm_trace( "After interrupt" );
		return inter._result.thread_id;
	}

	static ThreadId get_thread_id() {
		InterruptMask mask;
		__vm_trace( "Get id thread issued" );
		auto& inter = get();
		__dios_assert( inter._syscall == Type::INACTIVE );
		inter._syscall = Type::GET_THREAD_ID;
		__vm_trace( "Before interrupt" );
		__dios_interrupt();
		__vm_trace( "After interrupt" );
		return inter._result.thread_id;
	}

	static void kill_thread( ThreadId t_id, int reason) {
		InterruptMask mask;
		__vm_trace( "Kill thread issued" );
		auto& inter = get();
		__dios_assert( inter._syscall == Type::INACTIVE );
		inter._syscall = Type::KILL_THREAD;
		inter._args.kill_thread = Args::KillThread{ t_id, reason };
		__vm_trace( "Before interrupt" );
		__dios_interrupt();
		__vm_trace( "After interrupt" );
		return;
	}

	static void dummy() {
		InterruptMask mask;
		__vm_trace( "Dummy syscall" );
		auto& inter = get();
		__dios_assert( inter._syscall == Type::INACTIVE );
		inter._syscall = Type::DUMMY;

		__vm_trace( "Before interrupt" );
		__dios_interrupt();
		__vm_trace( "After interrupt" );
	}

	static Syscall& get() {
		if ( !instance ) {
			instance = static_cast< Syscall *> ( __vm_make_object( sizeof( Syscall ) ) );
			memset( instance, 0, sizeof( Syscall ) );
		}
		return *instance;
	}

private:

	enum class Type { INACTIVE, START_THREAD, KILL_THREAD, GET_THREAD_ID, DUMMY };
	
	Type _syscall;
	union RetValue {
		ThreadId thread_id;

		RetValue() {}
	} _result;

	union Args {
		using StartThread = std::tuple< FunPtr, void *, FunPtr >;
		using KillThread  = std::tuple< ThreadId, int >;
		
		StartThread start_thread;
		KillThread  kill_thread;

		Args() {}
	} _args;

	static Syscall *instance;

	friend struct Scheduler;
};

Syscall *Syscall::instance;

struct DiosMainFrame : _VM_Frame {
	int l;
	int argc;
	char** argv;
	char** envp;
};

struct ThreadRoutineFrame : _VM_Frame {
	void* arg;
};

struct CleanupFrame : _VM_Frame {
	int reason;
};

struct Thread {
	enum class State { RUNNING, CLEANING_UP, ZOMBIE };
	_VM_Frame *_frame;
	FunPtr _cleanup_handler;
	State _state;

	Thread( FunPtr fun, FunPtr cleanup = nullptr )
		: _frame( static_cast< _VM_Frame * >( __vm_make_object( fun->frame_size ) ) ),
		  _cleanup_handler( cleanup ),
		  _state( State::RUNNING )
	{
		_frame->pc = fun->entry_point;
		_frame->parent = nullptr;
		__dios_trace( 0, "Thread constuctor: %p, %p", _frame, _frame->pc );
	}

	Thread( const Thread& o ) = delete;
	Thread& operator=( const Thread& o ) = delete;

	Thread( Thread&& o ) :
		_frame( o._frame ), _cleanup_handler( o._cleanup_handler ),
		_state( o._state )
	{
		o._frame = 0;
		o._state = State::ZOMBIE;
	}

	Thread& operator=( Thread&& o ) {
		std::swap( _frame, o._frame );
		std::swap( _cleanup_handler, o._cleanup_handler );
		std::swap( _state, o._state );
		return *this;
	}

	~Thread() {
		clear();
	}

	bool active() const { return _state == State::RUNNING; }
	bool cleaning_up() const { return _state == State::CLEANING_UP; }
	bool zombie() const { return _state == State::ZOMBIE; }

	void update_state() {
		if ( !_frame )
			_state = State::ZOMBIE;
	}

	void stop_thread( int reason ) {
		if ( !active() ) {
			__vm_fault( static_cast< _VM_Fault >( _DiOS_F_Threading ) );
			return;
		}

		clear();
		auto* frame = reinterpret_cast< CleanupFrame * >( _frame );
		frame->pc = _cleanup_handler->entry_point;
		frame->parent = nullptr;
		frame->reason = reason;
		_state = State::CLEANING_UP;
	}

private:
	void clear() {
		while ( _frame ) {
			_VM_Frame *f = _frame->parent;
			__vm_free_object( _frame );
			_frame = f;
		}
	}
};

struct ControlFlow {
	ThreadId active_thread;
	int      thread_count;
	Thread   main_thread;
};

struct Scheduler {
	Scheduler( void *cf ) : _cf( static_cast< ControlFlow * >( cf ) ) {
		__dios_assert( cf );
	}

	Thread* get_threads() const noexcept {
		return &( _cf->main_thread );
	}

	void *get_cf() const noexcept {
		return _cf;
	}

	bool handle_syscall() noexcept {
		auto& inter = Syscall::get();

		if ( inter._syscall == Syscall::Type::INACTIVE ) {
			return false;
		}

		switch( inter._syscall ) {
		case Syscall::Type::START_THREAD: {
			__dios_trace( 0, "Syscall issued: start_thread" );
			auto& args = inter._args.start_thread;
			inter._result.thread_id = start_thread(
				std::get< 0 >( args ), std::get< 1 >( args ), std::get< 2 >( args ) );
			break;
			}
		case Syscall::Type::KILL_THREAD: {
			__dios_trace( 0, "Syscall issued: kill_thread" );
			auto& args = inter._args.kill_thread;
			kill_thread( std::get< 0 >( args ), std::get< 1 >( args ) );
			break;
			}
		case Syscall::Type::GET_THREAD_ID: {
			__dios_trace( 0, "Syscall issued: get_thread_id" );
			inter._result.thread_id = _cf->active_thread;
			break;
			}
		case Syscall::Type::DUMMY:
			__dios_trace( 0, "Syscall issued: dummy");
			break;
		default:
			__dios_assert( false );
		}

		inter._syscall = Syscall::Type::INACTIVE;
		return true;
	}

	_VM_Frame *run_thread( int idx = -1 ) noexcept {
		if ( idx < 0 )
			idx = _cf->active_thread;
		else
			_cf->active_thread = idx;

		Thread &thread = get_threads()[ idx ];
		thread.update_state();
		__dios_trace( 0, "Thread: %d, frame: %p, state: %d", _cf->active_thread, thread._frame, thread._state );
		
		if ( !thread.zombie() ) {
			__vm_set_ifl( &( thread._frame) );
			return thread._frame;
		}

		__dios_trace( 0, "Thread exit" );
		// ToDo: Move main thread. Neccessary only for divine run
		if ( idx != 0 ) {
			return run_thread(0);
		}
		_cf = nullptr;
		return nullptr;
	}

	_VM_Frame *run_threads() noexcept {
		__dios_trace( 0, "Number of threads: %d", _cf->thread_count );
		return run_thread( __vm_choose( _cf->thread_count ) );
	}

	void start_main_thread( FunPtr main, int argc, char** argv, char** envp ) noexcept {
		__dios_assert_v( main, "Invalid main function!" );

		_DiOS_FunPtr dios_main = __dios_get_fun_ptr( "__dios_main" );
		__dios_assert_v( dios_main, "Invalid DiOS main function" );

		new ( &( _cf->main_thread ) ) Thread( dios_main );
		_cf->active_thread = 0;
		_cf->thread_count = 1;
		
		DiosMainFrame *frame = reinterpret_cast< DiosMainFrame * >( _cf->main_thread._frame );
		frame->l = main->arg_count;

		if (main->arg_count >= 1)
			frame->argc = argc;
		if (main->arg_count >= 2)
			frame->argv = argv;
		if (main->arg_count >= 3)
			frame->envp = envp;
	}

	ThreadId start_thread( FunPtr routine, void *arg, FunPtr cleanup ) {
		__dios_assert( routine );

		int cur_size = __vm_query_object_size( _cf );
		void *new_cf = __vm_make_object( cur_size + sizeof( Thread ) );
		memcpy( new_cf, _cf, cur_size );
		__vm_free_object( _cf );
		_cf = static_cast< ControlFlow * >( new_cf );

		Thread &t = get_threads()[ _cf->thread_count++ ];
		new ( &t ) Thread( routine, cleanup );
		ThreadRoutineFrame *frame = reinterpret_cast< ThreadRoutineFrame * >(
			t._frame );
		frame->arg = arg;

		return _cf->thread_count - 1;
	}

	void kill_thread( ThreadId t_id, int reason ) {
		__dios_assert( t_id );
		__dios_assert( int( t_id ) < _cf->thread_count );
		get_threads()[ t_id ].stop_thread( reason );
	}
private:
	ControlFlow* _cf;
};

} // namespace dios


void *__dios_sched( int st_size, void *_state ) noexcept;
enum _VM_FaultAction __dios_fault( enum _VM_Fault what ) noexcept;
int main(...);

enum _VM_FaultAction __dios_fault( enum _VM_Fault what ) noexcept {
	/* ToDo: Handle errors */
	__vm_trace( "VM Fault" );
	switch ( what ) {
	case _VM_F_NoFault:
		__dios_trace( 0, "FAULT: No fault" );
		break;
	case _VM_F_Assert:
		__dios_trace( 0, "FAULT: Assert" );
		break;
	case _VM_F_Arithmetic:
		__dios_trace( 0, "FAULT: Arithmetic" );
		break;
	case _VM_F_Memory:
		__dios_trace( 0, "FAULT: Memory" );
		break;
	case _VM_F_Control:
		__dios_trace( 0, "FAULT: Control" );
		break;
	case _VM_F_Locking:
		__dios_trace( 0, "FAULT: Locking" );
		break;
	case _VM_F_Hypercall:
		__dios_trace( 0, "FAULT: Hypercall" );
		break;
	case _VM_F_NotImplemented:
		__dios_trace( 0, "FAULT: Not Implemented" );
		break;
	case _DiOS_F_MainReturnValue:
		__dios_trace( 0, "FAULT: Main exited with non-zero value" );
		break;
	default:
		__dios_trace( 0, "Unknown fault ");
	}
	return _VM_FA_Resume;
}

extern "C" void __dios_main( int l, int argc, char **argv, char **envp ) {
	__dios_trace( 0, "Dios started!" );
	int res;
	switch (l) {
	case 0:
		res = main();
		break;
	case 2:
		res = main( argc, argv );
		break;
	case 3:
		res = main( argc, argv, envp );
		break;
	default:
		__dios_assert_v( false, "Unexpected prototype of main" );
        res = 256;
	}

	if ( res != 0 )
		__vm_fault( ( _VM_Fault ) _DiOS_F_MainReturnValue );

	__dios_trace( 0, "DiOS out!" );
}

void *__dios_sched( int, void *state ) noexcept {
	dios::Scheduler scheduler( state );
	if ( scheduler.handle_syscall() ) {
		__vm_jump( scheduler.run_thread(), 1 );
		__dios_trace( 0, "Syscall should be handled\n" );
		return scheduler.get_cf();
	}

	_VM_Frame *jmp = scheduler.run_threads();
	if ( jmp ) {
		__vm_jump( jmp, 1 );
	}

	__dios_trace( 0, "\n" );
	return scheduler.get_cf();
}

extern "C" void *__dios_init( const _VM_Env *env ) {
	__vm_trace( "__sys_init called" );
	__vm_set_sched( __dios_sched );
	__vm_set_fault( __dios_fault );

	void *cf = __vm_make_object( sizeof( dios::ControlFlow ) );
	dios::Scheduler scheduler( cf );

	_DiOS_FunPtr main = __dios_get_fun_ptr( "main" );
	if ( !main ) {
		__vm_trace( "No main function" );
		__vm_fault( static_cast< _VM_Fault >( _DiOS_F_MissingFunction ), "main" );
		return nullptr;
	}

	/* ToDo: Parse and forward main arguments */
	scheduler.start_main_thread( main, 0, nullptr, nullptr );
	__vm_trace( "Main thread started" );
	
	return scheduler.get_cf();
}

extern "C" void *__sys_init( const _VM_Env *env ) __attribute__((weak)) {
	return __dios_init( env );
}

_DiOS_FunPtr __dios_get_fun_ptr( const char *name ) noexcept {
	return __md_get_function_meta( name );
}

_DiOS_ThreadId __dios_start_thread( _DiOS_FunPtr routine, void *arg,
	_DiOS_FunPtr cleanup ) noexcept
{
	return dios::Syscall::get().start_thread( routine, arg, cleanup );
}

_DiOS_ThreadId __dios_get_thread_id() noexcept {
	return dios::Syscall::get().get_thread_id();
}

void __dios_kill_thread( _DiOS_ThreadId id, int reason ) noexcept {
	return dios::Syscall::get().kill_thread( id, reason ); 
}

void __dios_dummy() noexcept {
	return dios::Syscall::get().dummy();
}

void __dios_interrupt() noexcept {
	int mask = __vm_mask( 1 );
	int interrupt = __vm_interrupt( 1 );
	__vm_mask( 0 );
	__vm_mask( 1 );
	//__vm_interrupt( interrupt );
	__vm_mask( mask );
}

void __dios_trace( int indent, const char *fmt, ... ) noexcept
{
    static bool inTrace = false;
    static int fmtIndent = 0;

	int mask = __vm_mask(1);

    if ( inTrace )
        goto unmask;
    inTrace = true;

	va_list ap;
	char buffer[1024];
    for ( int i = 0; i < fmtIndent; ++i )
        buffer[ i ] = ' ';

	va_start( ap, fmt );
	vsnprintf( buffer + fmtIndent, 1024, fmt, ap );
	va_end( ap );
	__vm_trace( buffer );

    fmtIndent += indent * 4;
    inTrace = false;
unmask:
	__vm_mask(mask);
}

