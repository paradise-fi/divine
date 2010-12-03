/*
 * net/http - HTTP server utilities
 *
 * Copyright (C) 2010  Enrico Zini <enrico@enricozini.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#include <wibble/net/http.h>
#include <wibble/exception.h>
#include <wibble/string.h>
#include <wibble/stream/posix.h>
#include <sstream>
#include <ctime>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

namespace wibble {
namespace net {
namespace http {

const char* error::what() const throw ()
{
    if (!msg.empty()) return msg.c_str();
    return desc.c_str();
}

void error::send(Request& req)
{
    stringstream body;
    body << "<html>" << endl;
    body << "<head>" << endl;
    body << "  <title>" << desc << "</title>" << endl;
    body << "</head>" << endl;
    body << "<body>" << endl;
    if (msg.empty())
        body << "<p>" + desc + "</p>" << endl;
    else
        body << "<p>" + msg + "</p>" << endl;
    body << "</body>" << endl;
    body << "</html>" << endl;

    req.send_status_line(code, desc);
    req.send_date_header();
    req.send_server_header();
    req.send_extra_response_headers();
    req.send("Content-Type: text/html; charset=utf-8\r\n");
    req.send(str::fmtf("Content-Length: %d\r\n", body.str().size()));
    req.send("\r\n");
    req.send(body.str());
}

void error404::send(Request& req)
{
    if (msg.empty())
        msg = "Resource " + req.script_name + " not found.";
    error::send(req);
}

Request::Request()
    : server_software("wibble"),
      response_started(false),
      space_splitter("[[:blank:]]+", REG_EXTENDED)
{
}

bool Request::read_request()
{
    // Set all structures to default values
    method = "GET";
    url = "/";
    version = "HTTP/1.0";
    headers.clear();

    // Read request line
    if (!read_method())
        return false;

    // Read request headers
    read_headers();

    // Parse the url
    script_name = "/";
    size_t pos = url.find('?');
    if (pos == string::npos)
    {
        path_info = url;
        query_string.clear();
    }
    else
    {
        path_info = url.substr(0, pos);
        query_string = url.substr(pos);
    }

    // Message body is not read here
    return true;
}

std::string Request::path_info_head()
{
    size_t beg = path_info.find_first_not_of('/');
    if (beg == string::npos)
        return string();
    else
    {
        size_t end = path_info.find('/', beg);
        if (end == string::npos)
            return path_info.substr(beg);
        else
            return path_info.substr(beg, end-beg);
    }
}

std::string Request::pop_path_info()
{
    // Strip leading /
    size_t beg = path_info.find_first_not_of('/');
    if (beg == string::npos)
    {
        path_info.clear();
        return string();
    }
    else
    {
        size_t end = path_info.find('/', beg);
        string first;
        if (end == string::npos)
        {
            first = path_info.substr(beg);
            path_info.clear();
        }
        else
        {
            first = path_info.substr(beg, end-beg);
            path_info = path_info.substr(end);
        }
        script_name = str::joinpath(script_name, first);
        return first;
    }
}

bool Request::read_buf(std::string& res, size_t size)
{
    res.clear();
    res.resize(size);
    size_t pos = 0;
    while (true)
    {
        ssize_t r = read( sock, const_cast< char* >( res.data() ) + pos, size - pos );
        if (r < 0)
            throw wibble::exception::System("reading data from socket");
        if (static_cast<size_t>(r) == size - pos)
            break;
        else if (r == 0)
        {
            res.resize(pos);
            return false;
        }
        pos += r;
    }
    return true;
}


// Read HTTP method and its following empty line
bool Request::read_method()
{
    // Request line, such as GET /images/logo.png HTTP/1.1, which
    // requests a resource called /images/logo.png from server
    string cmdline;
    if (!mime_reader.read_line(sock, cmdline)) return false;

    // If we cannot fill some of method, url or version we just let
    // them be, as they have previously been filled with defaults
    // by read_request()
    Splitter::const_iterator i = space_splitter.begin(cmdline);
    if (i != space_splitter.end())
    {
        method = str::toupper(*i);
        ++i;
        if (i != space_splitter.end())
        {
            url = *i;
            ++i;
            if (i != space_splitter.end())
                version = *i;
        }
    }

    // An empty line
    return mime_reader.read_line(sock, cmdline);
}

/**
 * Read HTTP headers
 *
 * @return true if there still data to read and headers are terminated
 * by an empty line, false if headers are terminated by EOF
 */
bool Request::read_headers()
{
    return mime_reader.read_headers(sock, headers);
}

// Set the CGI environment variables for the current process using this
// request
void Request::set_cgi_env()
{
    // Set CGI server-specific variables

#ifdef POSIX // FIXME
    // SERVER_SOFTWARE — name/version of HTTP server.
    setenv("SERVER_SOFTWARE", server_software.c_str(), 1);
    // SERVER_NAME — host name of the server, may be dot-decimal IP address.
    setenv("SERVER_NAME", server_name.c_str(), 1);
    // GATEWAY_INTERFACE — CGI/version.
    setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);

    // Set request-specific variables

    // SCRIPT_NAME — relative path to the program, like /cgi-bin/script.cgi.
    setenv("SCRIPT_NAME", script_name.c_str(), 1);
    // PATH_INFO — path suffix, if appended to URL after program name and a slash.
    setenv("PATH_INFO", path_info.c_str(), 1);
    // TODO: is it really needed?
    // PATH_TRANSLATED — corresponding full path as supposed by server, if PATH_INFO is present.
    unsetenv("PATH_TRANSLATED");
    // QUERY_STRING — the part of URL after ? character. Must be composed of name=value pairs separated with ampersands (such as var1=val1&var2=val2…) and used when form data are transferred via GET method.
    setenv("QUERY_STRING", query_string.c_str(), 1);
    // SERVER_PORT — TCP port (decimal).
    setenv("SERVER_PORT", server_port.c_str(), 1);
    // REMOTE_HOST — host name of the client, unset if server did not perform such lookup.
    setenv("REMOTE_HOST", peer_hostname.c_str(), 1);
    // REMOTE_ADDR — IP address of the client (dot-decimal).
    setenv("REMOTE_ADDR", peer_hostaddr.c_str(), 1);
    // SERVER_PROTOCOL — HTTP/version.
    setenv("SERVER_PROTOCOL", version.c_str(), 1);
    // REQUEST_METHOD — name of HTTP method (see above).
    setenv("REQUEST_METHOD", method.c_str(), 1);
    // AUTH_TYPE — identification type, if applicable.
    unsetenv("AUTH_TYPE");
    // REMOTE_USER used for certain AUTH_TYPEs.
    unsetenv("REMOTE_USER");
    // REMOTE_IDENT — see ident, only if server performed such lookup.
    unsetenv("REMOTE_IDENT");
    // CONTENT_TYPE — MIME type of input data if PUT or POST method are used, as provided via HTTP header.
    map<string, string>::const_iterator i = headers.find("content-type");
    if (i != headers.end())
        setenv("CONTENT_TYPE", i->second.c_str(), 1);
    else
        unsetenv("CONTENT_TYPE");
    // CONTENT_LENGTH — size of input data (decimal, in octets) if provided via HTTP header.
    i = headers.find("content-length");
    if (i != headers.end())
        setenv("CONTENT_LENGTH", i->second.c_str(), 1);
    else
        unsetenv("CONTENT_LENGTH");

    // Variables passed by user agent (HTTP_ACCEPT, HTTP_ACCEPT_LANGUAGE, HTTP_USER_AGENT, HTTP_COOKIE and possibly others) contain values of corresponding HTTP headers and therefore have the same sense.
    for (map<string, string>::const_iterator i = headers.begin();
            i != headers.end(); ++i)
    {
        if (i->first == "content-type" or i->first == "content-length")
            continue;

        string name = "HTTP_";
        for (string::const_iterator j = i->first.begin();
                j != i->first.end(); ++j)
            if (isalnum(*j))
                name.append(1, toupper(*j));
            else
                name.append(1, '_');
        setenv(name.c_str(), i->second.c_str(), 1);
    }
#endif
}

void Request::send(const std::string& buf)
{
    size_t pos = 0;
    while (pos < buf.size())
    {
        ssize_t sent = write(sock, buf.data() + pos, buf.size() - pos);
        if (sent < 0)
            throw wibble::exception::System("writing data to client");
        pos += sent;
    }
}

void Request::send_status_line(int code, const std::string& msg, const std::string& version)
{
    stringstream buf;
    buf << version << " " << code << " " << msg << "\r\n";
    send(buf.str());
    response_started = true;
}

void Request::send_server_header()
{
    stringstream buf;
    buf << "Server: " << server_software << "\r\n";
    send(buf.str());
}

void Request::send_date_header()
{
    time_t now = time(NULL);
    struct tm t;
#ifdef POSIX
    gmtime_r(&now, &t);
#else
    t = *gmtime(&now);
#endif
    char tbuf[256];
    size_t size = strftime(tbuf, 256, "%a, %d %b %Y %H:%M:%S GMT", &t);
    if (size == 0)
        throw wibble::exception::Consistency("internal buffer too small to store date header");

    stringstream buf;
    buf << "Date: " << tbuf << "\r\n";
    send(buf.str());
}

void Request::send_extra_response_headers()
{
    stringstream buf;
    for (map<string, string>::const_iterator i = extra_response_headers.begin();
            i != extra_response_headers.end(); ++i)
    {
        // Truncate the body at the first newline
        // FIXME: mangle in a better way
        buf << i->first << ": ";
        size_t pos = i->second.find('\n');
        if (pos == string::npos)
            buf << i->second;
        else
            buf << i->second.substr(0, pos);
        buf << "\r\n";
    }
    if (!buf.str().empty())
        send(buf.str());
}

void Request::send_result(const std::string& content, const std::string& content_type, const std::string& filename)
{
    send_status_line(200, "OK");
    send_date_header();
    send_server_header();
    send_extra_response_headers();
    send("Content-Type: " + content_type + "\r\n");
    if (!filename.empty())
        send("Content-Disposition: attachment; filename=" + filename + "\r\n");
    send(str::fmtf("Content-Length: %d\r\n", content.size()));
    send("\r\n");
    send(content);
}

void Request::discard_input()
{
    // First try seeking at end
    int res = lseek(sock, 0, SEEK_END);
    if (res == -1 && errno == ESPIPE)
    {
        // If it fails, we'll have to read our way
        char buf[4096];
        while (true)
        {
            int res = read(sock, buf, 4096);
            if (res < 0) throw wibble::exception::System("reading data from input socket");
            if (res == 0) break;
        }
    }
}

Param::~Param() {}

void ParamSingle::parse(const std::string& str)
{
    assign(str);
}

void ParamMulti::parse(const std::string& str)
{
    push_back(str);
}

FileParam::~FileParam() {}

bool FileParam::FileInfo::read(
        net::mime::Reader& mime_reader,
        map<string, string> headers,
        const std::string& outdir,
        const std::string& fname_blacklist,
        const std::string& client_fname,
        int sock,
        const std::string& boundary,
        size_t inputsize)
{
    int openflags = O_CREAT | O_WRONLY;

    // Store the client provided pathname
    this->client_fname = client_fname;

    // Generate the output file name
    if (fname.empty())
    {
        fname = client_fname;
        openflags |= O_EXCL;
    }
    string preferred_fname = str::basename(fname);

    // Replace blacklisted chars
    if (!fname_blacklist.empty())
        for (string::iterator i = preferred_fname.begin(); i != preferred_fname.end(); ++i)
            if (fname_blacklist.find(*i) != string::npos)
                *i = '_';

    preferred_fname = str::joinpath(outdir, preferred_fname);

    fname = preferred_fname;

    // Create the file
    int outfd;
    for (unsigned i = 1; ; ++i)
    {
        outfd = open(fname.c_str(), openflags, 0600);
        if (outfd >= 0) break;
        if (errno != EEXIST)
            throw wibble::exception::File(fname, "creating file");
        // Alter the file name and try again
        fname = preferred_fname + str::fmtf(".%u", i);
    }

    // Wrap output FD into a stream, which will take care of
    // closing it
    wibble::stream::PosixBuf posixBuf(outfd);
    ostream out(&posixBuf);

    // Read until boundary, sending data to temp file

    bool has_part = mime_reader.read_until_boundary(sock, boundary, out, inputsize);

    return has_part;
}

FileParamSingle::FileParamSingle(const std::string& fname)
{
    info.fname = fname;
}

bool FileParamSingle::read(
        net::mime::Reader& mime_reader,
        map<string, string> headers,
        const std::string& outdir,
        const std::string& fname_blacklist,
        const std::string& client_fname,
        int sock,
        const std::string& boundary,
        size_t inputsize)
{
    return info.read(mime_reader, headers, outdir, fname_blacklist, client_fname, sock, boundary, inputsize);
}

bool FileParamMulti::read(
        net::mime::Reader& mime_reader,
        map<string, string> headers,
        const std::string& outdir,
        const std::string& fname_blacklist,
        const std::string& client_fname,
        int sock,
        const std::string& boundary,
        size_t inputsize)
{
    files.push_back(FileInfo());
    return files.back().read(mime_reader, headers, outdir, fname_blacklist, client_fname, sock, boundary, inputsize);
}

Params::Params()
{
    conf_max_input_size = 20 * 1024 * 1024;
    conf_max_field_size = 1024 * 1024;
    conf_accept_unknown_fields = false;
    conf_accept_unknown_file_fields = false;
}

Params::~Params()
{
    for (iterator i = begin(); i != end(); ++i)
        delete i->second;
    for (std::map<std::string, FileParam*>::iterator i = files.begin(); i != files.end(); ++i)
        delete i->second;
}

void Params::add(const std::string& name, Param* param)
{
    iterator i = find(name);
    if (i != end())
    {
        delete i->second;
        i->second = param;
    } else
        insert(make_pair(name, param));
}

void Params::add(const std::string& name, FileParam* param)
{
    std::map<std::string, FileParam*>::iterator i = files.find(name);
    if (i != files.end())
    {
        delete i->second;
        i->second = param;
    } else
        files.insert(make_pair(name, param));
}

Param* Params::obtain_field(const std::string& name)
{
    iterator i = find(name);
    if (i != end())
        return i->second;
    if (!conf_accept_unknown_fields)
        return NULL;
    pair<iterator, bool> res = insert(make_pair(name, new ParamMulti));
    return res.first->second;
}

FileParam* Params::obtain_file_field(const std::string& name)
{
    std::map<std::string, FileParam*>::iterator i = files.find(name);
    if (i != files.end())
        return i->second;
    if (!conf_accept_unknown_file_fields)
        return NULL;
    pair<std::map<std::string, FileParam*>::iterator, bool> res =
        files.insert(make_pair(name, new FileParamMulti));
    return res.first->second;
}

Param* Params::field(const std::string& name)
{
    iterator i = find(name);
    if (i != end())
        return i->second;
    return NULL;
}

FileParam* Params::file_field(const std::string& name)
{
    std::map<std::string, FileParam*>::iterator i = files.find(name);
    if (i != files.end())
        return i->second;
    return NULL;
}

void Params::parse_get_or_post(net::http::Request& req)
{
    if (req.method == "GET")
    {
        size_t pos = req.url.find('?');
        if (pos != string::npos)
            parse_urlencoded(req.url.substr(pos + 1));
    }
    else if (req.method == "POST")
        parse_post(req);
    else
        throw wibble::exception::Consistency("cannot parse parameters from \"" + req.method + "\" request");
}

void Params::parse_urlencoded(const std::string& qstring)
{
    // Split on &
    str::Split splitter("&", qstring);
    for (str::Split::const_iterator i = splitter.begin();
            i != splitter.end(); ++i)
    {
        if (i->empty()) continue;

        // Split on =
        size_t pos = i->find('=');
        if (pos == string::npos)
        {
            // foo=
            Param* p = obtain_field(str::urldecode(*i));
            if (p != NULL)
                p->parse(string());
        }
        else
        {
            // foo=bar
            Param* p = obtain_field(str::urldecode(i->substr(0, pos)));
            if (p != NULL)
                p->parse(str::urldecode(i->substr(pos+1)));
        }
    }
}

void Params::parse_multipart(net::http::Request& req, size_t inputsize, const std::string& content_type)
{
    net::mime::Reader mime_reader;

    // Get the mime boundary
    size_t pos = content_type.find("boundary=");
    if (pos == string::npos)
        throw net::http::error400("no boundary in content-type");
    // TODO: strip boundary of leading and trailing "
    string boundary = "--" + content_type.substr(pos + 9);

    // Read until first boundary, discarding data
    mime_reader.discard_until_boundary(req.sock, boundary);

    boundary = "\r\n" + boundary;

    // Content-Disposition: form-data; name="submit-name"
    //wibble::ERegexp re_disposition(";%s*([^%s=]+)=\"(.-)\"", 2);
    wibble::Splitter cd_splitter("[[:blank:]]*;[[:blank:]]*", REG_EXTENDED);
    wibble::ERegexp cd_parm("([^=]+)=\"([^\"]+)\"", 3);

    bool has_part = true;
    while (has_part)
    {
        // Read mime headers for this part
        map<string, string> headers;
        if (!mime_reader.read_headers(req.sock, headers))
            throw net::http::error400("request truncated at MIME headers");

        // Get name and (optional) filename from content-disposition
        map<string, string>::const_iterator i = headers.find("content-disposition");
        if (i == headers.end())
            throw net::http::error400("no Content-disposition in MIME headers");
        wibble::Splitter::const_iterator j = cd_splitter.begin(i->second);
        if (j == cd_splitter.end())
            throw net::http::error400("incomplete content-disposition header");
        if (*j != "form-data")
            throw net::http::error400("Content-disposition is not \"form-data\"");
        string name;
        string filename;
        for (++j; j != cd_splitter.end(); ++j)
        {
            if (!cd_parm.match(*j)) continue;
            string key = cd_parm[1];
            if (key == "name")
            {
                name = cd_parm[2];
            } else if (key == "filename") {
                filename = cd_parm[2];
            }
        }

        if (!filename.empty())
        {
            // Get a file param
            FileParam* p = conf_outdir.empty() ? NULL : obtain_file_field(name);
            if (p != NULL)
                has_part = p->read(mime_reader, headers, conf_outdir, conf_fname_blacklist, filename, req.sock, boundary, inputsize);
            else
                has_part = mime_reader.discard_until_boundary(req.sock, boundary);
        } else {
            // Read until boundary, storing data in string
            stringstream value;
            has_part = mime_reader.read_until_boundary(req.sock, boundary, value, conf_max_field_size);

            // Store the field value
            Param* p = obtain_field(name);
            if (p != NULL)
                p->parse(value.str());
        }
    }
}

void Params::parse_post(net::http::Request& req)
{
    // Get the supposed size of incoming data
    map<string, string>::const_iterator i = req.headers.find("content-length");
    if (i == req.headers.end())
        throw wibble::exception::Consistency("no Content-Length: found in request header");
    // Validate the post size
    size_t inputsize = strtoul(i->second.c_str(), 0, 10);
    if (inputsize > conf_max_input_size)
    {
        // Discard all input
        req.discard_input();
        throw wibble::exception::Consistency(str::fmtf(
            "Total size of incoming data (%zdb) exceeds configured maximum (%zdb)",
            inputsize, conf_max_input_size));
    }

    // Get the content type
    i = req.headers.find("content-type");
    if (i == req.headers.end())
        throw wibble::exception::Consistency("no Content-Type: found in request header");
    if (i->second.find("x-www-form-urlencoded") != string::npos)
    {
        string line;
        req.read_buf(line, inputsize);
        parse_urlencoded(line);
    }
    else if (i->second.find("multipart/form-data") != string::npos)
    {
        parse_multipart(req, inputsize, i->second);
    }
    else
        throw wibble::exception::Consistency("unsupported Content-Type: " + i->second);
}

}
}
}
// vim:set ts=4 sw=4:
