#ifndef WIBBLE_NET_HTTP_H
#define WIBBLE_NET_HTTP_H

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

#include <string>
#include <map>
#include <wibble/regexp.h>
#include <wibble/net/mime.h>
#include <iosfwd>

namespace wibble {
namespace net {
namespace http {

struct Request;

struct error
{
    int code;
    std::string desc;
    std::string msg;

    error(int code, const std::string& desc)
        : code(code), desc(desc) {}
    error(int code, const std::string& desc, const std::string& msg)
        : code(code), desc(desc), msg(msg) {}

    virtual void send(Request& req);

};

struct error400 : public error
{
    error400() : error(400, "Bad request") {}
    error400(const std::string& msg) : error(400, "Bad request", msg) {}
};

struct error404 : public error
{
    error404() : error(404, "Not found") {}
    error404(const std::string& msg) : error(404, "Not found", msg) {}

    virtual void send(Request& req);
};

struct Request
{
    // Request does not take ownership of the socket: it is up to the caller to
    // close it
    int sock;
    std::string peer_hostname;
    std::string peer_hostaddr;
    std::string peer_port;
    std::string server_name;
    std::string server_port;
    std::string script_name;
    std::string path_info;

    std::string method;
    std::string url;
    std::string version;
    std::map<std::string, std::string> headers;
    wibble::Splitter space_splitter;

    wibble::net::mime::Reader mime_reader;

    Request();

    /**
     * Read request method and headers from sock
     *
     * Sock will be positioned at the beginning of the request body, after the
     * empty line that follows the header.
     *
     * @returns true if the request has been read, false if EOF was found
     * before the end of the headers.
     */
    bool read_request();

    /**
     * Read a fixed amount of data from the file descriptor
     *
     * @returns true if all the data were read, false if EOF was encountered
     * before the end of the buffer
     */
    bool read_buf(std::string& res, size_t size);

    // Read HTTP method and its following empty line
    bool read_method();

    /**
     * Read HTTP headers
     *
     * @return true if there still data to read and headers are terminated
     * by an empty line, false if headers are terminated by EOF
     */
    bool read_headers();

    // Set the CGI environment variables for the current process using this
    // request
    void set_cgi_env();

    // Send the content of buf, verbatim, to the client
    void send(const std::string& buf);

    // Send the HTTP status line
    void send_status_line(int code, const std::string& msg, const std::string& version = "HTTP/1.0");

    // Send the HTTP server header
    void send_server_header();

    // Send the HTTP date header
    void send_date_header();

    // Send a string as result
    void send_result(const std::string& content, const std::string& content_type="text/html; charset=utf-8", const std::string& filename=std::string());

    // Discard all input from the socket
    void discard_input();
};

}
}
}

// vim:set ts=4 sw=4:
#endif
