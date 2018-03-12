#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H  1

#include <sys/uio.h>
#include <sys/cdefs.h>
#include <stddef.h>
#include <unistd.h>     /* For ssize_t. */

__BEGIN_DECLS

/* Types of sockets.  */
enum __socket_type
{
  SOCK_STREAM = 1,       /* Sequenced, reliable, connection-based byte streams.  */
  SOCK_DGRAM = 2,        /* Connectionless, unreliable datagrams
                            of fixed maximum length.  */
  SOCK_RAW = 3,          /* Raw protocol interface.  */
  SOCK_RDM = 4,          /* Reliably-delivered messages.  */
  SOCK_SEQPACKET = 5,    /* Sequenced, reliable, connection-based,
                            datagrams of fixed maximum length.  */
  SOCK_DCCP = 6,         /* Datagram Congestion Control Protocol.  */
  SOCK_PACKET = 10,      /* Linux specific way of getting packets
                            at the dev level.  For writing rarp and
                            other similar things on the user level. */

  /* Flags to be ORed into the type parameter of socket and socketpair and
     used for the flags parameter of paccept.  */

  SOCK_CLOEXEC = 02000000,  /* Atomically set close-on-exec flag for the
                               new descriptor(s).  */
  SOCK_NONBLOCK = 04000,    /* Atomically mark descriptor(s) as
                              non-blocking.  */
  __SOCK_TYPE_MASK = 0777
};

#define _SOCK_TYPE_BMASK    0xF
#define _SOCK_FLAGS_BMASK   (~_SOCK_TYPE_BMASK)

/* Protocol families.  */
#define PF_UNSPEC     0   /* Unspecified.  */
#define PF_LOCAL      1   /* Local to host (pipes and file-domain).  */
#define PF_UNIX       PF_LOCAL /* POSIX name for PF_LOCAL.  */
#define PF_FILE       PF_LOCAL /* Another non-standard name for PF_LOCAL.  */
#define PF_INET       2   /* IP protocol family.  */
#define PF_AX25       3   /* Amateur Radio AX.25.  */
#define PF_IPX        4   /* Novell Internet Protocol.  */
#define PF_APPLETALK  5   /* Appletalk DDP.  */
#define PF_NETROM     6   /* Amateur radio NetROM.  */
#define PF_BRIDGE     7   /* Multiprotocol bridge.  */
#define PF_ATMPVC     8   /* ATM PVCs.  */
#define PF_X25        9   /* Reserved for X.25 project.  */
#define PF_INET6      10  /* IP version 6.  */
#define PF_ROSE       11  /* Amateur Radio X.25 PLP.  */
#define PF_DECnet     12  /* Reserved for DECnet project.  */
#define PF_NETBEUI    13  /* Reserved for 802.2LLC project.  */
#define PF_SECURITY   14  /* Security callback pseudo AF.  */
#define PF_KEY        15  /* PF_KEY key management API.  */
#define PF_NETLINK    16
#define PF_ROUTE      PF_NETLINK /* Alias to emulate 4.4BSD.  */
#define PF_PACKET     17  /* Packet family.  */
#define PF_ASH        18  /* Ash.  */
#define PF_ECONET     19  /* Acorn Econet.  */
#define PF_ATMSVC     20  /* ATM SVCs.  */
#define PF_RDS        21  /* RDS sockets.  */
#define PF_SNA        22  /* Linux SNA Project */
#define PF_IRDA       23  /* IRDA sockets.  */
#define PF_PPPOX      24  /* PPPoX sockets.  */
#define PF_WANPIPE    25  /* Wanpipe API sockets.  */
#define PF_LLC        26  /* Linux LLC.  */
#define PF_CAN        29  /* Controller Area Network.  */
#define PF_TIPC       30  /* TIPC sockets.  */
#define PF_BLUETOOTH  31  /* Bluetooth sockets.  */
#define PF_IUCV       32  /* IUCV sockets.  */
#define PF_RXRPC      33  /* RxRPC sockets.  */
#define PF_ISDN       34  /* mISDN sockets.  */
#define PF_PHONET     35  /* Phonet sockets.  */
#define PF_IEEE802154 36  /* IEEE 802.15.4 sockets.  */
#define PF_CAIF       37  /* CAIF sockets.  */
#define PF_ALG        38  /* Algorithm sockets.  */
#define PF_NFC        39  /* NFC sockets.  */
#define PF_MAX        40  /* For now..  */

/* Address families.  */
#define AF_UNSPEC      PF_UNSPEC
#define AF_LOCAL       PF_LOCAL
#define AF_UNIX        PF_UNIX
#define AF_FILE        PF_FILE
#define AF_INET        PF_INET
#define AF_AX25        PF_AX25
#define AF_IPX         PF_IPX
#define AF_APPLETALK   PF_APPLETALK
#define AF_NETROM      PF_NETROM
#define AF_BRIDGE      PF_BRIDGE
#define AF_ATMPVC      PF_ATMPVC
#define AF_X25         PF_X25
#define AF_INET6       PF_INET6
#define AF_ROSE        PF_ROSE
#define AF_DECnet      PF_DECnet
#define AF_NETBEUI     PF_NETBEUI
#define AF_SECURITY    PF_SECURITY
#define AF_KEY         PF_KEY
#define AF_NETLINK     PF_NETLINK
#define AF_ROUTE       PF_ROUTE
#define AF_PACKET      PF_PACKET
#define AF_ASH         PF_ASH
#define AF_ECONET      PF_ECONET
#define AF_ATMSVC      PF_ATMSVC
#define AF_RDS         PF_RDS
#define AF_SNA         PF_SNA
#define AF_IRDA        PF_IRDA
#define AF_PPPOX       PF_PPPOX
#define AF_WANPIPE     PF_WANPIPE
#define AF_LLC         PF_LLC
#define AF_CAN         PF_CAN
#define AF_TIPC        PF_TIPC
#define AF_BLUETOOTH   PF_BLUETOOTH
#define AF_IUCV        PF_IUCV
#define AF_RXRPC       PF_RXRPC
#define AF_ISDN        PF_ISDN
#define AF_PHONET      PF_PHONET
#define AF_IEEE802154  PF_IEEE802154
#define AF_CAIF        PF_CAIF
#define AF_ALG         PF_ALG
#define AF_NFC         PF_NFC
#define AF_MAX         PF_MAX

/* Socket level values.  Others are defined in the appropriate headers.

   XXX These definitions also should go into the appropriate headers as
   far as they are available.  */
#define SOL_RAW     255
#define SOL_DECNET  261
#define SOL_X25     262
#define SOL_PACKET  263
#define SOL_ATM     264  /* ATM layer (cell level).  */
#define SOL_AAL     265  /* ATM Adaption Layer (packet level).  */
#define SOL_IRDA    266

/* Maximum queue length specifiable by listen.  */
#define SOMAXCONN  128

/* Get the definition of the macro to define the common sockaddr members.  */
#include <bits/sockaddr.h>

/* Structure describing a generic socket address.  */
struct sockaddr {
    __SOCKADDR_COMMON (sa_);  /* Common data: address family and length.  */
    char sa_data[14];         /* Address data.  */
};

/* Structure large enough to hold any socket address (with the historical
   exception of AF_UNIX).  We reserve 128 bytes.  */
#define __ss_aligntype  unsigned long int
#define _SS_SIZE  128
#define _SS_PADSIZE  (_SS_SIZE - (2 * sizeof (__ss_aligntype)))

struct sockaddr_storage  {
    __SOCKADDR_COMMON (ss_);    /* Address family, etc.  */
    __ss_aligntype __ss_align;  /* Force desired alignment.  */
    char __ss_padding[_SS_PADSIZE];
};

/* Bits in the FLAGS argument to `send', `recv', et al.  */
enum  {
    MSG_OOB          = 0x01,  /* Process out-of-band data.  */
    MSG_PEEK         = 0x02,  /* Peek at incoming messages.  */
    MSG_DONTROUTE    = 0x04,  /* Don't use local routing.  */
    MSG_TRYHARD      = MSG_DONTROUTE,  /* DECnet uses a different name.  */
    MSG_CTRUNC       = 0x08,  /* Control data lost before delivery.  */
    MSG_PROXY        = 0x10,  /* Supply or ask second address.  */
    MSG_TRUNC        = 0x20,
    MSG_DONTWAIT     = 0x40,  /* Nonblocking IO.  */
    MSG_EOR          = 0x80,  /* End of record.  */
    MSG_WAITALL      = 0x100, /* Wait for a full request.  */
    MSG_FIN          = 0x200,
    MSG_SYN          = 0x400,
    MSG_CONFIRM      = 0x800, /* Confirm path validity.  */
    MSG_RST          = 0x1000,
    MSG_ERRQUEUE     = 0x2000,  /* Fetch message from error queue.  */
    MSG_NOSIGNAL     = 0x4000,  /* Do not generate SIGPIPE.  */
    MSG_MORE         = 0x8000,  /* Sender will send more.  */
    MSG_WAITFORONE   = 0x10000, /* Wait for at least one packet to return.*/
    MSG_CMSG_CLOEXEC = 0x40000000  /* Set close_on_exit for file descriptor
                                      received through SCM_RIGHTS.  */
};

/* Structure describing messages sent by `sendmsg' and received by `recvmsg'.  */
struct msghdr {
    void *msg_name;         /* Address to send to/receive from.  */
    socklen_t msg_namelen;  /* Length of address data.  */

    struct iovec *msg_iov;  /* Vector of data to send/receive into.  */
    size_t msg_iovlen;      /* Number of elements in the vector.  */

    void *msg_control;      /* Ancillary data (eg BSD filedesc passing). */
    size_t msg_controllen;  /* Ancillary data buffer length. */
    int msg_flags;          /* Flags on received message.  */
};

/* Structure used for storage of ancillary data object information.  */
struct cmsghdr {
    size_t cmsg_len;    /* Length of data in cmsg_data plus length
                           of cmsghdr structure. */
    int cmsg_level;     /* Originating protocol.  */
    int cmsg_type;      /* Protocol specific type.  */
};

/* cmsg type */
enum {
#ifdef CVFS_ENABLE_SCM_RIGHTS
    SCM_RIGHTS = 0x01,    /* Transfer file descriptors.  */
#define SCM_RIGHTS SCM_RIGHTS
#endif
    SCM_CREDENTIALS = 0x02  /* Credentials passing.  */
};

/* User visible structure for SCM_CREDENTIALS message */
struct ucred {
  pid_t pid;      /* PID of sending process.  */
  uid_t uid;      /* UID of sending process.  */
  gid_t gid;      /* GID of sending process.  */
};

/* Protocol number used to manipulate socket-level options
   with `getsockopt' and `setsockopt'.  */
#define SOL_SOCKET  0xffff

/* Socket-level options for `getsockopt' and `setsockopt'.  */
enum
  {
    SO_DEBUG =       0x0001,  /* Record debugging information.  */
    SO_ACCEPTCONN =  0x0002,  /* Accept connections on socket.  */
    SO_REUSEADDR =   0x0004,  /* Allow reuse of local addresses.  */
    SO_KEEPALIVE =   0x0008,  /* Keep connections alive and send
                                 SIGPIPE when they die.  */
    SO_DONTROUTE =   0x0010,  /* Don't do local routing.  */
    SO_BROADCAST =   0x0020,  /* Allow transmission of
                                 broadcast messages.  */
    SO_USELOOPBACK = 0x0040,  /* Use the software loopback to avoid
                                 hardware use when possible.  */
    SO_LINGER =      0x0080,  /* Block on close of a reliable
                                 socket to transmit pending data.  */
    SO_OOBINLINE =   0x0100,  /* Receive out-of-band data in-band.  */
    SO_REUSEPORT =   0x0200,  /* Allow local address and port reuse.  */
    SO_SNDBUF =      0x1001,  /* Send buffer size.  */
    SO_RCVBUF =      0x1002,  /* Receive buffer.  */
    SO_SNDLOWAT =    0x1003,  /* Send low-water mark.  */
    SO_RCVLOWAT =    0x1004,  /* Receive low-water mark.  */
    SO_SNDTIMEO =    0x1005,  /* Send timeout.  */
    SO_RCVTIMEO =    0x1006,  /* Receive timeout.  */
    SO_ERROR =       0x1007,  /* Get and clear error status.  */
    SO_STYLE =       0x1008,  /* Get socket connection style.  */
    SO_TYPE =        SO_STYLE /* Compatible name for SO_STYLE.  */
  };

/* Structure used to manipulate the SO_LINGER option.  */
struct linger  {
    int l_onoff;     /* Nonzero to linger on close.  */
    int l_linger;    /* Time to linger.  */
};

/* The following constants should be used for the second parameter of
   `shutdown'.  */
enum {
    SHUT_RD = 0,    /* No more receptions.  */
    SHUT_WR,        /* No more transmissions.  */
    SHUT_RDWR       /* No more receptions or transmissions.  */
};

/* For `recvmmsg' and `sendmmsg'. */
struct mmsghdr {
    struct msghdr msg_hdr;  /* Actual message header.  */
    unsigned int msg_len;   /* Number of received or sent bytes for the
                               entry.  */
};


/* Create a new socket of type TYPE in domain DOMAIN, using
   protocol PROTOCOL.  If PROTOCOL is zero, one is chosen automatically.
   Returns a file descriptor for the new socket, or -1 for errors.  */
__noinline int socket( int domain, int type, int protocol ) __nothrow;

/* Create two new sockets, of type TYPE in domain DOMAIN and using
   protocol PROTOCOL, which are connected to each other, and put file
   descriptors for them in FDS[0] and FDS[1].  If PROTOCOL is zero,
   one will be chosen automatically.  Returns 0 on success, -1 for errors.  */
__noinline int socketpair( int domain, int type, int protocol,
                            int fds[2] ) __nothrow;

/* Put the local address of FD into *ADDR and its length in *LEN.  */
__noinline int getsockname ( int fd, struct sockaddr *addr, socklen_t *addrlen ) __nothrow;

/* Give the socket FD the local address ADDR (which is LEN bytes long).  */
__noinline int bind( int fd, const struct sockaddr *addr, socklen_t len ) __nothrow;

/* Open a connection on socket FD to peer at ADDR (which LEN bytes long).
   For connectionless socket types, just set the default address to send to
   and the only address from which to accept transmissions.
   Return 0 on success, -1 for errors.
*/
__noinline int connect( int fd, const struct sockaddr *addr, socklen_t len ) __nothrow;

/* Put the address of the peer connected to socket FD into *ADDR
   (which is *LEN bytes long), and its actual length into *LEN.  */
__noinline int getpeername( int fd, struct sockaddr *addr, socklen_t *len ) __nothrow;

/* Send N bytes of BUF to socket FD.  Returns the number sent or -1. */
__noinline ssize_t send( int fd, const void *buf, size_t n, int flags ) __nothrow;

/* Send N bytes of BUF on socket FD to peer at address ADDR (which is
   ADDRLEN bytes long).  Returns the number sent, or -1 for errors.
*/
__noinline ssize_t sendto( int fd, const void *buf, size_t n, int flags,
                            const struct sockaddr *addr, socklen_t addrlen ) __nothrow;

/* Send a message described by MESSAGE on socket FD.
   Returns the number of bytes sent, or -1 for errors.
*/
/*extern ssize_t sendmsg( int fd, const struct msghdr *message,
                        int flags ) CV__noinline;*/

/* Send a VLEN messages as described by VMESSAGES to socket FD.
   Returns the number of datagrams successfully written or -1 for errors. */
/*extern int sendmmsg( int fd, struct mmsghdr *vmessages, unsigned int vlen,
                     int flags ) CV__noinline;*/

/* Read N bytes into BUF from socket FD.
   Returns the number read or -1 for errors.
*/
__noinline ssize_t recv( int fd, void *buf, size_t n, int flags ) __nothrow;

/* Read N bytes into BUF through socket FD.
   If ADDR is not NULL, fill in *ADDRLEN bytes of it with tha address of
   the sender, and store the actual size of the address in *ADDR_LEN.
   Returns the number of bytes read or -1 for errors.
*/
__noinline ssize_t recvfrom( int fd, void *buf, size_t n, int flags,
                         struct sockaddr *addr, socklen_t *addrlen ) __nothrow;

/* Receive a message as described by MESSAGE from socket FD.
   Returns the number of bytes read or -1 for errors.
*/
/*extern ssize_t recvmsg( int fd, struct msghdr *message, int flags ) CV__noinline;*/

/* Receive up to VLEN messages as described by VMESSAGES from socket FD.
   Returns the number of bytes read or -1 for errors. */
/*extern int recvmmsg( int fd, struct mmsghdr *vmessages, unsigned int vlen,
                     int flags, const struct timespec *tmo ) CV__noinline;*/

/* Put the current value for socket FD's option OPTNAME at protocol level LEVEL
   into OPTVAL (which is *OPTLEN bytes long), and set *OPTLEN to the value's
   actual length.  Returns 0 on success, -1 for errors.  */
/*extern int getsockopt( int fd, int level, int optname,
                       void *optval, socklen_t *optlen )  CVFS_NOT_IMPLEMENTED;*/

/* Set socket FD's option OPTNAME at protocol level LEVEL
   to *OPTVAL (which is OPTLEN bytes long).
   Returns 0 on success, -1 for errors.  */
/*extern int setsockopt( int fd, int level, int optname,
                       const void *optval, socklen_t optlen )  CVFS_NOT_IMPLEMENTED;*/

/* Prepare to accept connections on socket FD.
   N connection requests will be queued before further requests are refused.
   Returns 0 on success, -1 for errors.  */
__noinline int listen( int fd, int n ) __nothrow;

/* Await a connection on socket FD.
   When a connection arrives, open a new socket to communicate with it,
   set *ADDR (which is *ADDR_LEN bytes long) to the address of the connecting
   peer and *ADDR_LEN to the address's actual length, and return the
   new socket's descriptor, or -1 for errors.
*/
__noinline int accept( int fd, struct sockaddr * addr,  socklen_t * addr_len ) __nothrow;

/* Similar to 'accept' but takes an additional parameter to specify flags. */
__noinline int accept4( int fd, struct sockaddr *addr, socklen_t * addr_len,
                         int flags ) __nothrow;

/* Shut down all or part of the connection open on socket FD.
   HOW determines what to shut down:
     SHUT_RD   = No more receptions;
     SHUT_WR   = No more transmissions;
     SHUT_RDWR = No more receptions or transmissions.
   Returns 0 on success, -1 for errors.  */
/*extern int shutdown( int fd, int how );*/

__END_DECLS

#endif /* sys/socket.h */
