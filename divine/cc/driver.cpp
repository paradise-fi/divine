// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016-2017 Vladimír Štill
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <divine/cc/driver.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>

#include <brick-string>
#include <brick-types>

namespace divine::cc
{
    using namespace std::literals;

    // TODO: Unused
    static std::string getWrappedMDS( llvm::NamedMDNode *meta, int i = 0, int j = 0 )
    {
        auto *op = llvm::cast< llvm::MDTuple >( meta->getOperand( i ) );
        auto *str = llvm::cast< llvm::MDString >( op->getOperand( j ).get() );
        return str->getString().str();
    }

    // TODO: Clever but unused
    struct MergeFlags_
    { // hide in class so they can be mutually recursive
        void operator()( std::vector< std::string > & ) { }

        template< typename ... Xs >
        void operator()( std::vector< std::string > &out, std::string first, Xs &&... xs )
        {
            out.emplace_back( std::move( first ) );
            (*this)( out, std::forward< Xs >( xs )... );
        }

        template< typename C, typename ... Xs >
        auto operator()( std::vector< std::string > &out, C &&c, Xs &&... xs )
            -> decltype( void( (c.begin(), c.end()) ) )
        {
            out.insert( out.end(), c.begin(), c.end() );
            (*this)( out, std::forward< Xs >( xs )... );
        }
    };

    template< typename ... Xs >
    static std::vector< std::string > mergeFlags( Xs &&... xs )
    {
        std::vector< std::string > out;
        MergeFlags_()( out, std::forward< Xs >( xs )... );
        return out;
    }

    // Return all possible jobs for a Clang compilation with the provided args
    std::vector< Command > Driver::getJobs( llvm::ArrayRef< const char * > args )
    {
        using clang::driver::Compilation;
        ClangDriver drv;

        std::unique_ptr< Compilation > c( drv.BuildCompilation( args ) );
        if ( drv.diag.engine.hasErrorOccurred() )
            throw cc::CompileError( "failed to get linker arguments, aborting" );
        std::vector< Command > clangJobs;

        for( auto job : c->getJobs() )
        {
            Command cmd( job.getExecutable() );
            for( auto arg : job.getArguments() )
                cmd.addArg( arg );
            clangJobs.push_back( cmd );
        }
        return clangJobs;
    }

    Driver::Driver( Options opts, std::shared_ptr< llvm::LLVMContext > ctx ) :
        opts( opts ), compiler( ctx ), linker( new brick::llvm::Linker( compiler.context() ) )
        {
            commonFlags = { "-debug-info-kind=standalone" };
        }

    Driver::~Driver() { }

    std::unique_ptr< llvm::Module > Driver::compile( std::string path,
                                        std::vector< std::string > flags )
    {
        return compile( path, typeFromFile( path ), flags );
    }

    // Append the necessary and provided flags and defer to the compiler
    std::unique_ptr< llvm::Module > Driver::compile( std::string path,
                                        FileType type, std::vector< std::string > flags )
    {
        using FT = FileType;

        if ( type == FT::Unknown )
            throw std::runtime_error( "cannot detect file format for file '"
                                      + path + "', please supply -x option for it" );

        std::vector< std::string > allFlags;
        std::copy( commonFlags.begin(), commonFlags.end(), std::back_inserter( allFlags ) );
        std::copy( flags.begin(), flags.end(), std::back_inserter( allFlags ) );
        if ( opts.verbose )
            std::cerr << "compiling " << path << std::endl;

        compiler.allowIncludePath( "." ); /* clang 4.0 requires that cwd is always accessible */
        compiler.allowIncludePath( brq::dirname( path ) );
        auto mod = compiler.compile( path, type, allFlags );

        return mod;
    }

    // Compile all the files and link them together, including necessary libraries
    void Driver::build( ParsedOpts po )
    {
        for ( auto path : po.allowedPaths )
            compiler.allowIncludePath( path );

        for ( auto &f : po.files )
        {
            f.match(
                [&]( const File &f )
                {
                    if ( auto m = compile( f.name, f.type, po.opts ) )
                        linker->link( std::move( m ) );
                },
                [&]( const Lib &l )
                {
                    linkLib( l.name, po.libSearchPath );
                } );
        }
    }

    std::unique_ptr< llvm::Module > Driver::takeLinked()
    {
        brick::llvm::verifyModule( linker->get() );
        return linker->take();
    }

    // Write the linked bitcode into a file
    void Driver::writeToFile( std::string filename )
    {
        writeToFile( filename, linker->get() );
    }

    void Driver::writeToFile( std::string filename, llvm::Module *mod )
    {
        brick::llvm::writeModule( mod, filename );
    }

    // Return the linked bitcode serialized into a string
    std::string Driver::serialize()
    {
        return compiler.serializeModule( *linker->get() );
    }

    void Driver::addDirectory( std::string path )
    {
        compiler.allowIncludePath( path );
    }

    void Driver::addFlags( std::vector< std::string > flags )
    {
        std::copy( flags.begin(), flags.end(), std::back_inserter( commonFlags ) );
    }

    std::shared_ptr< llvm::LLVMContext > Driver::context() { return compiler.context(); }

    void Driver::linkLibs( std::vector< std::string > ls, std::vector< std::string > searchPaths )
    {
        TRACE( "link libs", ls, searchPaths );
        for ( auto lib : ls )
            linkLib( lib, searchPaths );
    }

    void Driver::link( ModulePtr mod )
    {
       linker->link( std::move( mod ) );
    }

    // Find a library called 'lib' in 'dirs' and return it as an archive.
    // Given a name "foo", these file names are tried: "foo.a", "foo.bc", "libfoo.a", "libfoo.bc"
    brick::llvm::ArchiveReader Driver::find_archive( std::string lib, string_vec dirs )
    {
        return read_archive( find_library( lib, { ".a", ".bc", "" }, dirs ) );
    }

    brick::llvm::ArchiveReader Driver::read_archive( std::string path )
    {
        auto buf = compiler.getFileBuffer( path );
        if ( !buf )
            throw std::runtime_error( "Cannot open library file: " + path );

        return brick::llvm::ArchiveReader( std::move( buf ), context() );
    }

    std::string Driver::find_library( std::string lib, string_vec suffixes, string_vec dirs )
    {
        compiler.allowIncludePath( "." );  // TODO: necessary?
        compiler.allowIncludePath( brq::dirname( lib ) );

        dirs.insert( dirs.begin(), "/dios/lib" );
        for ( auto p : dirs )
            for ( auto suf : suffixes )
                for ( auto pref : { "lib", "" } )
                {
                    auto n = brq::join_path( p, pref + lib + suf );
                    if ( compiler.fileExists( n ) )
                        return n;
                }
        throw std::runtime_error( "Library not found: " + lib );
    }

    std::string Driver::find_object( std::string name )
    {
        return brq::join_path( "/dios/lib", name + ".bc"s );
    }

    Driver::ModulePtr Driver::load_object( std::string path )
    {
        using IRO = llvm::object::IRObjectFile;

        compiler.allowIncludePath( "." ); // TODO: necessary?
        compiler.allowIncludePath( brq::dirname( path ) );

        if ( !compiler.fileExists( path ) )
            throw std::runtime_error( "object not found: " + path );

        auto buf = compiler.getFileBuffer( path );
        if ( !buf )
            throw std::runtime_error( "cannot open object: " + path );

        auto bc_buf = IRO::findBitcodeInMemBuffer( buf.get()->getMemBufferRef() );

        if ( !bc_buf )
            throw std::runtime_error( "could not parse object: " + path );

        auto parsed = llvm::parseBitcodeFile( bc_buf.get(), *context() );
        if ( !parsed )
            throw std::runtime_error( "could not parse bitcode in " + path );

        return std::move( parsed.get() );
    }

    void Driver::linkLib( std::string lib, std::vector< std::string > dirs, bool shared /* = false */ )
    {
        TRACE( "link lib", lib, dirs );
        auto path = find_library( lib, { ".a", ".so", ".bc", "" }, dirs );
        TRACE( "using", path );

        if ( shared )
        {
            linker->link( load_object( path ) );
        }
        else
        {
            auto archive = find_archive( lib, std::move( dirs ) );
            auto modules = archive.modules();
            for ( auto it = modules.begin(); it != modules.end(); ++it )
            {
                if ( it.getName() == "_link_essentials"s ) // This contains the declarations
                {
                    linker->link_decls( it.take() );
                    break;
                }
            }

            linker->linkArchive( archive );
    	}
    }

    void Driver::linkArchive( std::unique_ptr< llvm::MemoryBuffer > buf, std::shared_ptr< llvm::LLVMContext > context )
    {
        auto archive = brick::llvm::ArchiveReader( std::move( buf ), context );
        linker->linkArchive( archive );
    }

    // Link the entire archive - i.e. every module, including the ones that are not needed to resolve undefined symbols
    void Driver::linkEntireArchive( std::string arch )
    {
        auto archive = find_archive( arch );
        auto modules = archive.modules();
        for ( auto it = modules.begin(); it != modules.end(); ++it )
            linker->link( it.take() );
    }

}
