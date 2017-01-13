// // -*- C++ -*- (c) 2016 Katarina Kejstova

#include <iostream>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <string>

#include <bits/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <divine.h>
#include <dios.h>
#include <dios/core/syscall.hpp>


#ifdef __divine__

# define FS_MALLOC( x ) __vm_obj_make( x )
# define FS_PROBLEM( msg ) __dios_fault( _VM_Fault::_VM_F_Assert, msg )
# define FS_FREE( x ) __dios::delete_object( x )

#endif

namespace __sc_passthru {

	void read( __dios::Context& , int* err, void* retval, va_list vl ) {
		auto fd = va_arg( vl, int );
        auto buf = va_arg( vl, void * );
        auto count = va_arg( vl, size_t );

        *err = __vm_syscall( __dios::_VM_SC::read, 
        	_VM_SC_Out | _VM_SC_Int64, retval,
        	_VM_SC_In | _VM_SC_Int32, fd,
        	_VM_SC_Out | _VM_SC_Mem, count, buf,
        	_VM_SC_In | _VM_SC_Int64, count );

	}

	void write( __dios::Context& , int* err, void* retval, va_list vl )
    {
        auto fd = va_arg( vl, int );
        auto buf = va_arg( vl, const void * );
        auto count = va_arg( vl, size_t );

         *err = __vm_syscall( __dios::_VM_SC::write,
                  _VM_SC_Out | _VM_SC_Int64, retval,
                  _VM_SC_In | _VM_SC_Int32, fd,
                  _VM_SC_In | _VM_SC_Mem, count, buf,
                  _VM_SC_In | _VM_SC_Int64, count );
    }

    void readlinkat( __dios::Context& , int* err, void* retval, va_list vl )
    {
        auto dirfd = va_arg( vl, int );
        auto path = va_arg( vl, const char* );
        auto buf = va_arg( vl, char* );
        auto count = va_arg( vl, size_t );
        va_end(vl);

        *err = __vm_syscall( __dios::_VM_SC::readlinkat, 
        	_VM_SC_Out | _VM_SC_Int64, retval,
        	_VM_SC_In | _VM_SC_Int32, dirfd,
        	_VM_SC_In | _VM_SC_Mem, strlen( path ), path,
        	_VM_SC_Out | _VM_SC_Mem, count, buf,
        	_VM_SC_In | _VM_SC_Int64, count );
     
    }


    void socketpair( __dios::Context&, int* err, void* retval, va_list vl )
    {
        auto domain = va_arg( vl, int );
        auto t = va_arg( vl, int );
        auto protocol = va_arg( vl, int );
        auto fds = va_arg( vl, int* );

        *err = __vm_syscall( __dios::_VM_SC::socketpair, 
        	_VM_SC_Out | _VM_SC_Int32, retval,
        	_VM_SC_In | _VM_SC_Int32, domain,
        	_VM_SC_In | _VM_SC_Int32, t,
        	_VM_SC_In | _VM_SC_Int32, protocol,
        	_VM_SC_Out | _VM_SC_Mem, sizeof( int ) * 2,  fds );
    }

    void creat( __dios::Context& , int* err, void* retval, va_list vl ){
        auto path = va_arg( vl, const char * );
        auto mode = va_arg( vl, mode_t );
        va_end( vl );

         *err = __vm_syscall( __dios::_VM_SC::creat, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_In | _VM_SC_Int32, mode );
    }

    void close( __dios::Context& , int* err, void* retval, va_list vl ){
        auto fd = va_arg( vl, int );

         *err = __vm_syscall( __dios::_VM_SC::close, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, fd );

    }

    void open( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto path = va_arg( vl, const char * );
        auto flags = va_arg(  vl, int );
        int mode = 0;
        va_end( vl );
        
        if ( flags & O_CREAT ) {
            if ( !vl )
                    FS_PROBLEM( "flag O_CREAT has been specified but mode was not set" );
            mode = va_arg(  vl, int );
        }

        *err = __vm_syscall( __dios::_VM_SC::open, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_In | _VM_SC_Int32, flags,
            _VM_SC_In | _VM_SC_Int32, mode );
        
    }

	void lseek( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto fd = va_arg( vl, int );
        auto offset = va_arg( vl, off_t );
        auto whence = va_arg( vl, int );
        va_end( vl );

         *err = __vm_syscall( __dios::_VM_SC::lseek, 
            _VM_SC_Out | _VM_SC_Int64, retval,
            _VM_SC_In | _VM_SC_Int32, fd,
            _VM_SC_In | _VM_SC_Int64, offset,
            _VM_SC_In | _VM_SC_Int32, whence );

    }

	void dup( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto fd = va_arg( vl, int );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::dup, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, fd );
        
    }

	void dup2( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto oldfd = va_arg( vl, int );
        auto newfd = va_arg( vl, int );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::dup2, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, oldfd,
            _VM_SC_In | _VM_SC_Int32, newfd );

    }

	void ftruncate( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto fd = va_arg( vl, int );
        auto length = va_arg( vl, off_t );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::ftruncate, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, fd,
            _VM_SC_In | _VM_SC_Int64, length );
    }

	void truncate( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto path = va_arg( vl, const char* );
        auto length = va_arg( vl, off_t );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::truncate, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_In | _VM_SC_Int64, length );        
    }

	void rmdir( __dios::Context& , int* err, void* retval, va_list vl )  {
        auto path = va_arg( vl, const char* );
        va_end( vl );
      
        *err = __vm_syscall( __dios::_VM_SC::rmdir, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path );   
    }

    void link( __dios::Context& , int* err, void* retval, va_list vl )  {
        auto target = va_arg( vl, const char* );
        auto linkpath = va_arg( vl, const char* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::link, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( target ), target,
            _VM_SC_In | _VM_SC_Mem, strlen( linkpath ), linkpath ); 
    }

	void unlink( __dios::Context& , int* err, void* retval, va_list vl )  {
        auto path = va_arg( vl, const char* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::unlink, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path ); 
    }

	void unlinkat( __dios::Context& , int* err, void* retval, va_list vl )  {
        auto dirfd = va_arg( vl, int );
        auto path = va_arg( vl, const char* );
        auto flags = va_arg( vl, int );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::unlinkat, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, dirfd,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_In | _VM_SC_Int32, flags ); 
    }

	void readlink( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto path = va_arg( vl, const char* );
        auto buf = va_arg( vl, char* );
        auto count = va_arg( vl, size_t );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::readlink, 
            _VM_SC_Out | _VM_SC_Int64, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_Out | _VM_SC_Mem, count, buf,
            _VM_SC_In | _VM_SC_Int64, count );
    }

	void linkat( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto olddirfd = va_arg( vl, int );
        auto target = va_arg( vl, const char* );
        auto newdirfd = va_arg( vl, int );
        auto linkpath = va_arg( vl, const char* );
        auto flags = va_arg( vl, int );
        va_end( vl );
        
        *err = __vm_syscall( __dios::_VM_SC::linkat, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, olddirfd,
            _VM_SC_In | _VM_SC_Mem, strlen( target ), target,
            _VM_SC_In | _VM_SC_Int32, newdirfd,
            _VM_SC_In | _VM_SC_Mem, strlen( linkpath ), linkpath,
            _VM_SC_In | _VM_SC_Int32, flags );
    }

	void symlink( __dios::Context& , int* err, void* retval, va_list vl ) {
         auto target = va_arg( vl, const char* );
        auto linkpath = va_arg( vl, const char* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::symlink, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( target ), target,
            _VM_SC_In | _VM_SC_Mem, strlen( linkpath ), linkpath );
    }

	void symlinkat( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto target = va_arg( vl, const char* );
        auto dirfd = va_arg( vl, int );
        auto linkpath = va_arg( vl, const char* );
        va_end( vl );
        
        *err = __vm_syscall( __dios::_VM_SC::symlinkat, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( target ), target,
            _VM_SC_In | _VM_SC_Int32, dirfd,
            _VM_SC_In | _VM_SC_Mem, strlen( linkpath ), linkpath );
    }

	void access( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto path = va_arg( vl, const char* );
        auto mode = va_arg( vl, int );
        va_end( vl );

         *err = __vm_syscall( __dios::_VM_SC::access, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_In | _VM_SC_Int32, mode );
    }

	void chdir( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto path = va_arg( vl, const char* );
        va_end( vl );


         *err = __vm_syscall( __dios::_VM_SC::chdir, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path );
    }

	void faccessat( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto dirfd = va_arg( vl, int );
        auto path = va_arg( vl, const char* );
        auto mode = va_arg( vl, int );
        auto flags = va_arg( vl, int );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::faccessat, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, dirfd,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_In | _VM_SC_Int32, mode,
            _VM_SC_In | _VM_SC_Int32, flags );
    }

	void fsync( __dios::Context& , int* err, void* retval, va_list vl ) {
         auto fd = va_arg( vl, int );
         va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::fsync, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, fd );
    }

	void fdatasync( __dios::Context& , int* err, void* retval, va_list vl ) {
         auto fd = va_arg( vl, int );
         va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::fdatasync, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, fd );
    }

	void fchdir( __dios::Context& , int* err, void* retval, va_list vl ) {
         auto dirfd = va_arg( vl, int );
         va_end( vl );

         *err = __vm_syscall( __dios::_VM_SC::fchdir, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, dirfd );
    }

	void syncfs( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto fd = va_arg( vl, int );
         va_end( vl );

         *err = __vm_syscall( __dios::_VM_SC::syncfs, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, fd );
    }

	void fstat( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto fd = va_arg( vl, int );
        auto buf = va_arg( vl, struct stat* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::fstat, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, fd,
            _VM_SC_Out | _VM_SC_Mem, sizeof( struct stat ), buf);
    }

	void fstatfs( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto fd = va_arg( vl, int );
        auto buf = va_arg( vl, struct stat* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::fstatfs, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, fd,
            _VM_SC_Out | _VM_SC_Mem, sizeof( struct stat ), buf);

    }

	void lstat( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto path = va_arg( vl, const char* );
        auto buf = va_arg( vl, struct stat* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::lstat, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_Out | _VM_SC_Mem, sizeof( struct stat ), buf);
    }

	void stat( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto path = va_arg( vl, const char* );
        auto buf = va_arg( vl, struct stat* );
        va_end( vl );

         *err = __vm_syscall( __dios::_VM_SC::stat, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_Out | _VM_SC_Mem, sizeof( struct stat ), buf);
    }

	void statfs( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto path = va_arg( vl, const char* );
        auto buf = va_arg( vl, struct stat* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::statfs, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_Out | _VM_SC_Mem, sizeof( struct stat ), buf);

    }

	void chmod( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto path = va_arg( vl, const char* );
        auto mode = va_arg( vl, mode_t );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::chmod, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_In | _VM_SC_Int32, mode );
    }

	void fchmod( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto fd = va_arg( vl, int );
        auto mode = va_arg( vl, mode_t );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::fchmod, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, fd,
            _VM_SC_In | _VM_SC_Int32, mode );
    }

	void fchmodat( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto dirfd = va_arg( vl, int );
        auto path = va_arg( vl, const char* );
        auto mode = va_arg( vl, mode_t );
        auto flags = va_arg( vl, int );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::fchmodat, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, dirfd,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_In | _VM_SC_Int32, mode,
            _VM_SC_In | _VM_SC_Int32, flags );
    }

	void umask( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto mask = va_arg( vl, mode_t );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::umask, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, mask );
    }

	void mkdir( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto dirfd = va_arg( vl, int );
        auto path = va_arg( vl, const char* );
        auto mode = va_arg( vl, mode_t );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::mkdir, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, dirfd,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_In | _VM_SC_Int32, mode );
    }

	void mkdirat( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto dirfd = va_arg( vl, int );
        auto path = va_arg( vl, const char* );
        auto mode = va_arg( vl, mode_t );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::mkdirat, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, dirfd,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_In | _VM_SC_Int32, mode );
    }

	void mknod( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto path = va_arg( vl, const char* );
        auto mode = va_arg( vl, mode_t );
        auto dev = va_arg( vl, dev_t );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::mknod, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_In | _VM_SC_Int32, mode,
            _VM_SC_In | _VM_SC_Int32, dev );
    }

	void mknodat( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto dirfd = va_arg( vl, int );
        auto path = va_arg( vl, const char* );
        auto mode = va_arg( vl, mode_t );
        auto dev = va_arg( vl, dev_t );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::mknodat, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, dirfd,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_In | _VM_SC_Int32, mode,
            _VM_SC_In | _VM_SC_Int32, dev );
    }

	void sync( __dios::Context& , int* err, void* retval, va_list  ) {
        *err = __vm_syscall( __dios::_VM_SC::sync,
            _VM_SC_Out | _VM_SC_Int32, retval );
    }

	void openat( __dios::Context& , int* err, void* retval, va_list vl ) {
        mode_t mode = 0;
        auto dirfd = va_arg(  vl, int );
        auto path = va_arg( vl, const char * );
        auto flags = va_arg(  vl, int );

        if ( flags & O_CREAT ) {
            if ( !vl )
                    FS_PROBLEM( "flag O_CREAT has been specified but mode was not set" );
            mode = va_arg(vl, mode_t );
            va_end(  vl );
        }

        *err = __vm_syscall( __dios::_VM_SC::openat, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, dirfd,
            _VM_SC_In | _VM_SC_Mem, strlen( path ), path,
            _VM_SC_In | _VM_SC_Int32, flags,
            _VM_SC_In | _VM_SC_Int32, mode );
    }

	void fcntl( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto fd = va_arg( vl, int );
        auto cmd = va_arg( vl, int );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::fcntl, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, fd,
            _VM_SC_In | _VM_SC_Int32, cmd );
    }

	void pipe( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto pipefd = va_arg( vl, int* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::pipe, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_Out | _VM_SC_Int32, pipefd );
    }

	void socket( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto domain = va_arg( vl, int );
        auto type = va_arg( vl, int );
        auto protocol = va_arg( vl, int );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::pipe, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, domain,
            _VM_SC_In | _VM_SC_Int32, type,
            _VM_SC_In | _VM_SC_Int32, protocol);
    }

	void getsockname( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto sockfd = va_arg( vl, int );
        auto addr = va_arg( vl,  struct sockaddr* );
        auto len = va_arg( vl, socklen_t* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::pipe, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, sockfd,
            _VM_SC_Out | _VM_SC_Mem, sizeof( struct sockaddr ), addr,
            _VM_SC_Out | _VM_SC_Int32, len );
    }

	void bind( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto sockfd = va_arg( vl, int );
        auto addr = va_arg( vl,  const struct sockaddr* );
        auto len = va_arg( vl, socklen_t );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::bind, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, sockfd,
            _VM_SC_In | _VM_SC_Mem, sizeof( struct sockaddr ), addr,
            _VM_SC_In | _VM_SC_Int32, len );
    }

	void connect( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto sockfd = va_arg( vl, int );
        auto addr = va_arg( vl,  const struct sockaddr* );
        auto len = va_arg( vl, socklen_t );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::connect, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, sockfd,
            _VM_SC_In | _VM_SC_Mem, sizeof( struct sockaddr ), addr,
            _VM_SC_In | _VM_SC_Int32, len );
    }

	void getpeername( __dios::Context& , int* err, void* retval, va_list vl ) {
         auto sockfd = va_arg( vl, int );
        auto addr = va_arg( vl, struct sockaddr* );
        auto len = va_arg( vl, socklen_t* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::getpeername, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, sockfd,
            _VM_SC_Out | _VM_SC_Mem, sizeof( struct sockaddr ), addr,
            _VM_SC_Out | _VM_SC_Int32, len );
    }

	void sendto( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto sockfd = va_arg( vl, int );
        auto buf = va_arg( vl,  const void* );
        auto n = va_arg( vl, size_t );
        auto flags = va_arg( vl, int );
        auto addr = va_arg( vl,  const struct sockaddr* );
        auto len = va_arg( vl, socklen_t );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::sendto, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, sockfd,
            _VM_SC_In | _VM_SC_Mem, len, buf,
            _VM_SC_In | _VM_SC_Int64, n,
            _VM_SC_In | _VM_SC_Int32, flags,
            _VM_SC_In | _VM_SC_Mem, sizeof( struct sockaddr ), addr,
            _VM_SC_In | _VM_SC_Int32, len );
    }

	void listen( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto sockfd = va_arg( vl, int );
        auto n = va_arg( vl, int );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::listen, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, sockfd,
            _VM_SC_In | _VM_SC_Int32, n );
    }

	void accept( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto sockfd = va_arg( vl, int );
        auto addr = va_arg( vl,  struct sockaddr* );
        auto len = va_arg( vl, socklen_t* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::accept, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, sockfd,
            _VM_SC_Out | _VM_SC_Mem, sizeof( struct sockaddr ), addr,
            _VM_SC_Out | _VM_SC_Int32, len );
    }

	void recv( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto sockfd = va_arg( vl, int );
        auto buf = va_arg( vl,  void* );
        auto n = va_arg( vl, size_t );
        auto flags = va_arg( vl, int );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::accept, 
            _VM_SC_Out | _VM_SC_Int64, retval,
            _VM_SC_In | _VM_SC_Int32, sockfd,
            _VM_SC_Out | _VM_SC_Mem, n, buf,
            _VM_SC_In | _VM_SC_Int64, n,
            _VM_SC_In | _VM_SC_Int32, flags );
    }

	void accept4( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto sockfd = va_arg( vl, int );
        auto addr = va_arg( vl,  struct sockaddr* );
        auto len = va_arg( vl, socklen_t* );
        auto flags = va_arg( vl, int );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::accept4, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, sockfd,
            _VM_SC_Out | _VM_SC_Mem, sizeof( struct sockaddr ), addr,
            _VM_SC_Out | _VM_SC_Int32, len,
            _VM_SC_In | _VM_SC_Int32, flags );

    }

	void recvfrom( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto sockfd = va_arg( vl, int );
        auto buf = va_arg( vl,  void* );
        auto n = va_arg( vl, size_t );
        auto flags = va_arg( vl, int );
        auto addr = va_arg( vl,  struct sockaddr* );
        auto len = va_arg( vl, socklen_t* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::recvfrom, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, sockfd,
            _VM_SC_Out | _VM_SC_Mem, n, buf,
            _VM_SC_In | _VM_SC_Int64, n,
            _VM_SC_In | _VM_SC_Int32, flags,
            _VM_SC_Out | _VM_SC_Mem, sizeof( struct sockaddr ), addr,
            _VM_SC_Out | _VM_SC_Int32, len );
    }

    void renameat( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto olddirfd = va_arg( vl, int );
        auto oldpath = va_arg( vl, const char* );
        auto newdirfd = va_arg( vl, int );
        auto newpath = va_arg( vl, const char* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::renameat, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Int32, olddirfd,
            _VM_SC_In | _VM_SC_Mem, strlen( oldpath ), oldpath,
            _VM_SC_In | _VM_SC_Int32, newdirfd,
            _VM_SC_In | _VM_SC_Mem, strlen( newpath ), newpath );
    }

    void rename( __dios::Context& , int* err, void* retval, va_list vl ) {
        auto oldpath = va_arg( vl, const char* );
        auto newpath = va_arg( vl, const char* );
        va_end( vl );

        *err = __vm_syscall( __dios::_VM_SC::rename, 
            _VM_SC_Out | _VM_SC_Int32, retval,
            _VM_SC_In | _VM_SC_Mem, strlen( oldpath ), oldpath,
            _VM_SC_In | _VM_SC_Mem, strlen( newpath ), newpath );
    }
	
} // eo namespace __sc_passthru

