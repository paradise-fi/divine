#include <sys/types.h>
#include <sys/cdefs.h>

struct group {
               char   *gr_name;        /* group name */
               char   *gr_passwd;      /* group password */
               gid_t   gr_gid;         /* group ID */
               char  **gr_mem;         /* NULL-terminated array of pointers
                                          to names of group members */
           };

__BEGIN_DECLS
struct group *getgrnam(const char *name);
struct group *getgrgid(gid_t gid);
__END_DECLS
