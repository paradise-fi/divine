/*
 * This file is part of the program ltl2dstar (http://www.ltl2dstar.de/).
 * Copyright (C) 2005-2007 Joachim Klein <j.klein@ltl2dstar.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 *  published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */



#ifndef TEMPFILE
#define TEMPFILE

/** @file
 * Provide temporary file objects.
 */

#if (__WIN32__ || _WIN32)
 #include <windows.h>
 #include <io.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cstdio>
#include <cerrno>
#include <string>
#include <iostream>
#include "common/fdstream.hpp"
#include "common/Exceptions.hpp"

/**
 * Virtual base class for a temporary file.
 */
class TempFile {
public:
  /** Constructor */
  TempFile() : istr(0), ostr(0) {}

  /** Destructor */
  virtual ~TempFile() {delete istr;delete ostr;}

  /** Get a file descriptor for the file */
  virtual int getFileDescriptor() = 0;

  /** Seek to start of file */
  void reset() {
    lseek(getFileDescriptor(), 0, SEEK_SET);
  }

  /** Get an istream for this file */
  std::istream& getIStream() {
    if (istr!=0) {return *istr;}

    lseek(getFileDescriptor(), 0, SEEK_SET);
    istr=new boost::fdistream(getFileDescriptor());
    return *istr;
  }

  /** Get an ostream for this file */
  std::ostream& getOStream() {
    if (ostr!=0) {return *ostr;}

    lseek(getFileDescriptor(), 0, SEEK_SET);
    ostr=new boost::fdostream(getFileDescriptor());
    return *ostr;
  }

  /** Get a FILE* for reading from this file */
  virtual FILE *getInFILEStream() {
    // dup file descriptor so that the FILE* can be
    // closed independantly of the file descriptor
    int dup_fd=dup(getFileDescriptor());
    if (dup_fd<0) {throw Exception("Couldn't dup file descriptor! errno="
				   +boost::lexical_cast<std::string>(errno));}

    FILE *file=fdopen(dup_fd, "r");
    fseek(file, 0, SEEK_SET);

    return file;
  }

  /** Get a FILE* for writing to this file */
  virtual FILE *getOutFILEStream() {
    // dup file descriptor so that the FILE* can be
    // closed independantly of the file descriptor
    int dup_fd=dup(getFileDescriptor());
    if (dup_fd<0) {throw Exception("Couldn't dup file descriptor! errno="
				   +boost::lexical_cast<std::string>(errno));}

    FILE *file=fdopen(dup_fd, "a");
    fseek(file, 0, SEEK_SET);
    return file;
  }



#if (__WIN32__ || _WIN32)
  /** Get the win32 file handle for this file */
  HANDLE getFileHandle() {
    return (HANDLE)_get_osfhandle(getFileDescriptor());
  }
#endif

private:
  std::istream* istr;
  std::ostream* ostr;
};

/**
 * A class for an anonymous temporary file. Because this file
 * is anonymous, it can not be used for
 * sym-link attacks on the user, but it can also not be passed
 * by name to another process.
 */
class AnonymousTempFile : public TempFile {
public:
  /** Constructor */
  AnonymousTempFile() {
    _file=tmpfile();
    if (!_file) {
      THROW_EXCEPTION(Exception, "Could not open anonymous tmpfile!");
    }
  }

  /** Destructor */
  virtual ~AnonymousTempFile() {fclose(_file);}

  /** Get a file descriptor */
  virtual int getFileDescriptor() {
    #if (__WIN32__ || _WIN32)
    return _fileno(_file);
    #else
    return fileno(_file);
    #endif
  }

private:
  FILE *_file;
};


#endif
