#include <divine/dios.h>
#include <tuple>
#include <utility>
#include <cstring>

#include <cstdarg>
#include <cstdio>

void vm_trace( const char *fmt, ... )
{
    va_list ap;
    char buffer[1024];
    va_start( ap, fmt );
    vsnprintf( buffer, 1024, fmt, ap );
    va_end( ap );
    __vm_trace( buffer );
}

namespace dios {

using ThreadId = _Sys_ThreadId;
using FunPtr   = _Sys_FunPtr;
struct Scheduler;

struct Syscall {
	Syscall() : _syscall( Type::INACTIVE ) {
		__vm_trace( "Syscall constructor" );
	}

	static ThreadId start_thread( FunPtr routine, void *arg, FunPtr cleanup ) {
		auto& inter = get();
		dios_assert( inter._syscall == Type::INACTIVE );
		dios_assert( routine );
		inter._syscall = Type::START_THREAD;
		inter._args.start_thread = Args::StartThread{ routine, arg, cleanup };
		// ToDo: Interrupt
		return inter._result.thread_id;
	}

	static ThreadId get_thread_id() {
		auto& inter = get();
		dios_assert( inter._syscall == Type::INACTIVE );
		inter._syscall = Type::GET_THREAD_ID;
		// ToDo: Interrupt
		return inter._result.thread_id;
	}

	static void kill_thread( ThreadId t_id, int reason) {
		auto& inter = get();
		dios_assert( inter._syscall == Type::INACTIVE );
		inter._syscall = Type::KILL_THREAD;
		inter._args.kill_thread = Args::KillThread{ t_id, reason };
		// ToDo: Interrupt
		return;
	}

	static Syscall& get() {
		__vm_trace( "Syscall get: entry" );
		if ( !instance ) {
			instance = static_cast< Syscall *> ( __vm_make_object( sizeof( Syscall ) ) );
			memset( instance, 0, sizeof( Syscall ) );
		}
		__vm_trace( "Syscall get: exit" );
		return *instance;
	}

private:

	enum class Type { INACTIVE, START_THREAD, KILL_THREAD, GET_THREAD_ID };
	
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

struct MainFrame : _VM_Frame {
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
		vm_trace( "Thread constuctor: %p, %p", _frame, _frame->pc );
	}

	Thread( Thread& o) = delete;

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
			__vm_fault( static_cast< _VM_Fault >( _Sys_F_Threading ) );
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
		dios_assert( cf );
		__vm_trace( "Scheduler constructor" );
	}

	Thread* get_threads() const noexcept {
		return &( _cf->main_thread );
	}

	void *get_cf() const noexcept {
		return _cf;
	}

	bool handle_syscall() noexcept {
		__vm_trace( "Syscall handle entry" );
		auto& inter = Syscall::get();

		if ( inter._syscall == Syscall::Type::INACTIVE ) {
			__vm_trace( "No syscall issued" );
			return false;
		}

		void *ret = this;
		switch( inter._syscall ) {
		case Syscall::Type::START_THREAD: {
			auto& args = inter._args.start_thread;
			inter._result.thread_id = start_thread(
				std::get< 0 >( args ), std::get< 1 >( args ), std::get< 2 >( args ) );
			break;
			}
		case Syscall::Type::KILL_THREAD: {
			auto& args = inter._args.kill_thread;
			kill_thread( std::get< 0 >( args ), std::get< 1 >( args ) );
			break;
			}
		case Syscall::Type::GET_THREAD_ID: {
			inter._result.thread_id = _cf->active_thread;
			break;
			}
		default:
			dios_assert( false );
		}

		inter._syscall = Syscall::Type::INACTIVE;
		return true;
	}

	_VM_Frame *run_threads() noexcept {
		_cf->active_thread = __vm_choose( _cf->thread_count );
		vm_trace( "Active thread: %d", _cf->active_thread );
		Thread& thread = get_threads()[ _cf->active_thread ];
		__vm_trace( "Thread obtained" );
		thread.update_state();
		__vm_trace( "Thread updated" );
		vm_trace( "Thread frame: %p, thread state: %d", thread._frame, thread._state );
		if ( !thread.zombie() ) {
			__vm_trace( "Before setting ifl" );
			__vm_set_ifl( &( thread._frame ) );
			__vm_trace( "ifl set" );
			return thread._frame;
		}
		else {
			__vm_trace( "Thread exit" );
			_cf = nullptr;
			return nullptr;
		}
	}

	void start_main_thread( FunPtr main, int argc, char** argv, char** envp ) noexcept {
		dios_assert( main );

		new ( &( _cf->main_thread ) ) Thread( main );
		_cf->active_thread = 0;
		_cf->thread_count = 1;
		
		MainFrame *frame = reinterpret_cast< MainFrame * >( _cf->main_thread._frame );

		if (main->arg_count >= 1)
			frame->argc = argc;
		if (main->arg_count >= 2)
			frame->argv = argv;
		if (main->arg_count >= 3)
			frame->envp = envp;
	}

	ThreadId start_thread( FunPtr routine, void *arg, FunPtr cleanup ) {
		dios_assert( routine );

		int cur_size = __vm_query_object_size( _cf );
		void *new_cf = __vm_make_object( cur_size + sizeof( Thread ) );
		__vm_memcpy( new_cf, _cf, cur_size );
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
		dios_assert( t_id );
		dios_assert( t_id <  _cf->thread_count );
		get_threads()[ t_id ].stop_thread( reason );
	}
private:
	ControlFlow* _cf;
};

} // namespace dios

enum _VM_FaultAction __sys_fault( enum _VM_Fault what ) noexcept {
	/* ToDo: Handle errors */
	__vm_trace( "VM Fault" );
	return _VM_FA_Resume;
}

void *__sys_sched( int, void *state ) noexcept {
	__vm_trace("Scheduler entry");
	dios::Scheduler scheduler( state );
	if ( !scheduler.handle_syscall() ) {
		_VM_Frame *jmp = scheduler.run_threads();
		if ( jmp ) {
			vm_trace( "Scheduler pre-jump: %p", jmp );
			__vm_jump( jmp, 1 );
			__vm_trace( "Scheduler after jump" );
		}
	}
	vm_trace( "Scheduler exit, %p", scheduler.get_cf() );
	return scheduler.get_cf();
}


void *__sys_init( void *env[] ) {
	__vm_trace( "__sys_init called" );
	__vm_set_sched( __sys_sched );
	__vm_set_fault( __sys_fault );

	void *cf = __vm_make_object( sizeof( dios::ControlFlow ) );
	dios::Scheduler scheduler( cf );

	_Sys_FunPtr main = __sys_get_fun_ptr( "main" );
	if (!main) {
		__vm_trace( "No main function" );
		__vm_fault( static_cast< _VM_Fault >( _Sys_F_MissingFunction ), "main" );
		return nullptr;
	}

	/* ToDo: Parse and forward main arguments */
	scheduler.start_main_thread( main, 0, nullptr, nullptr );
	__vm_trace( "Main thread started" );
	
	return scheduler.get_cf();
}

_Sys_FunPtr __sys_get_fun_ptr( const char *name ) noexcept {
	return __vm_query_function( name );
}

_Sys_ThreadId __sys_start_thread( _Sys_FunPtr routine, void *arg,
	_Sys_FunPtr cleanup ) noexcept
{
	return dios::Syscall::get().start_thread( routine, arg, cleanup );
}

_Sys_ThreadId __sys_get_thread_id() noexcept {
	return dios::Syscall::get().get_thread_id();
}

void __sys_kill_thread( _Sys_ThreadId id, int reason ) noexcept {
	return dios::Syscall::get().kill_thread( id, reason ); 
}
