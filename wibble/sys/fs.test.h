/* -*- C++ -*- (c) 2007--2011 Petr Rockai <me@mornfall.net>
               (c) 2007--2011 Enrico Zini <enrico@enricozini.org>
               (c) 2014 Vladimír Štill <xstill@fi.muni.cz> */
#include <wibble/sys/fs.h>
#include <wibble/exception.h>
#include <cstdlib>
#include <set>
#include <cstdlib>
#include <unistd.h>

#include <wibble/test.h>

using namespace std;
using namespace wibble::sys::fs;

struct TestFs {

// Test directory iteration
    Test directoryIterate() {
#ifdef POSIX
        Directory dir("/");

        set<string> files;
        for (Directory::const_iterator i = dir.begin(); i != dir.end(); ++i)
            files.insert(*i);

        assert(files.size() > 0);
        assert(files.find(".") != files.end());
        assert(files.find("..") != files.end());
        assert(files.find("etc") != files.end());
        assert(files.find("bin") != files.end());
        assert(files.find("tmp") != files.end());
#endif
    }

    Test directoryIsdir()
    {
#ifdef POSIX
        {
            Directory dir("/");
            for (Directory::const_iterator i = dir.begin(); i != dir.end(); ++i)
                if (*i == "etc")
                {
                    assert(i.isdir());
                    assert(!i.isreg());
                }
        }
        {
            Directory dir("/etc");
            for (Directory::const_iterator i = dir.begin(); i != dir.end(); ++i)
                if (*i == "passwd")
                {
                    assert(i.isreg());
                    assert(!i.isdir());
                }
        }
        /* {  TODO: d_type on bind-mounted files is wrong; i.e. /dev/null gives
              d_type of DT_REG in nix-created build chroots
            Directory dir("/dev");
            for (Directory::const_iterator i = dir.begin(); i != dir.end(); ++i)
            {
                if (*i == "null")
                {
                    assert(i.ischr());
                    assert(!i.isblk());
                }
                else if (*i == "sda")
                {
                    assert(i.isblk());
                    assert(!i.ischr());
                }
            }
        } */
#endif
    }

    // Ensure that nonexisting directories and files are reported as not valid
    Test invalidDirectories() {
#ifdef POSIX
        try {
            Directory dir1("/antaniblindalasupercazzola123456");
            assert(false);
        } catch (wibble::exception::System& e) {
        }

        try {
            Directory dir2("/etc/passwd");
            assert(false);
        } catch (wibble::exception::System& e) {
        }
#endif
    }

    Test _mkPath() {
        // Mkpath should succeed on existing directory
        mkpath(".");

        // Mkpath should succeed on existing directory
        mkpath("./.");

        // Mkpath should succeed on existing directory
#ifdef POSIX
        mkpath("/");
#endif
    }

    Test _mkPath2() {
        // Try creating a path with mkpath
#ifdef POSIX
        system("rm -rf test-mkpath");
#endif
#ifdef WIN32
        system( "rmdir /Q /S test-mkpath" );
#endif
        mkpath("test-mkpath/test-mkpath");
        assert(wibble::sys::fs::access("test-mkpath", F_OK));
        assert(wibble::sys::fs::access("test-mkpath/test-mkpath", F_OK));
    }

    Test _mkFilePath() {
        // Try creating a path with mkFilePath
#ifdef POSIX
        system("rm -rf test-mkpath");
#endif
#ifdef WIN32
        system( "rmdir /Q /S test-mkpath" );
#endif
        mkFilePath("test-mkpath/test-mkpath/file");
        assert(wibble::sys::fs::access("test-mkpath", F_OK));
        assert(wibble::sys::fs::access("test-mkpath/test-mkpath", F_OK));
        assert(!wibble::sys::fs::access("test-mkpath/test-mkpath/file", F_OK));
    }

    Test _mkdirIfMissing() {
#ifdef POSIX
        system("rm -rf test-mkpath");
#endif
#ifdef WIN32
        system( "rmdir /Q /S test-mkpath" );
#endif
        // Creating works and is idempotent
        {
            assert(!wibble::sys::fs::access("test-mkpath", F_OK));
            wibble::sys::fs::mkdirIfMissing("test-mkpath");
            assert(wibble::sys::fs::access("test-mkpath", F_OK));
            wibble::sys::fs::mkdirIfMissing("test-mkpath");
        }

        // Creating fails if it exists and it is a file
        {
#ifdef POSIX
            system("rm -rf test-mkpath; touch test-mkpath");
#endif
#ifdef WIN32
            system( "rmdir /Q /S test-mkpath" );
            system( "type NUL > test-mkpath" );
#endif
            try {
                wibble::sys::fs::mkdirIfMissing("test-mkpath");
                assert(false);
            } catch (wibble::exception::Consistency& e) {
                assert(string(e.what()).find("exists but it is not a directory") != string::npos);
            }
        }

#ifdef POSIX // hm
        // Deal with dangling symlinks
        {
            system("rm -rf test-mkpath; ln -s ./tmp/tmp/tmp/DOESNOTEXISTS test-mkpath");
            try {
                wibble::sys::fs::mkdirIfMissing("test-mkpath");
                assert(false);
            } catch (wibble::exception::Consistency& e) {
                assert(string(e.what()).find("looks like a dangling symlink") != string::npos);
            }
        }
#endif // POSIX
    }

    Test _deleteIfExists() {
#ifdef POSIX
        system("rm -f does-not-exist");
        assert(!deleteIfExists("does-not-exist"));
        system("touch does-exist");
        assert(deleteIfExists("does-exist"));
#endif
#ifdef WIN32
        system( "del /Q DoesNotExit.txt" );
        assert( !deleteIfExists( "DoesNotExit.txt" ) );
        system( "type NUL > DoesExist.txt" );
        assert( deleteIfExists( "DoesExist.txt" ) );
#endif
    }

    Test _isdir() {
#ifdef POSIX
        system("rm -rf testdir");
        assert(!isdir("testdir"));
        system("touch testdir");
        assert(!isdir("testdir"));
        system("rm testdir; mkdir testdir");
        assert(isdir("testdir"));
#endif
        assert( isdir( "CMakeFiles" ) );
        assert( !isdir( "_ThisDoesNotExist_" ) );
        assert( !isdir( "wibble-test-generated-main.cpp" ) );
    }

    Test _stat() {
        std::auto_ptr< struct stat64 > pstat;

        pstat = stat( "CMakeFiles" );
        assert( pstat.get() );
        assert( S_ISDIR( pstat->st_mode ) );

        pstat = stat( "CMakeFiles/" );
        assert( pstat.get() );
        assert( S_ISDIR( pstat->st_mode ) );

        pstat = stat( "_ThisDoesNotExist_" );
        assert( !pstat.get() );

        pstat = stat( "wibble-test-generated-main.cpp" );
        assert( pstat.get() );
        assert( S_ISREG( pstat->st_mode ) );
    }
};

// vim:set ts=4 sw=4:
