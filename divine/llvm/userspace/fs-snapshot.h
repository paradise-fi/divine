#include <sys/types.h>

#ifndef _FS_SNAPSHOT_H_
#define _FS_SNAPSHOT_H_

namespace divine {
namespace fs {

enum class Type {
    Nothing,
    File,
    Directory,
    Pipe,
    SymLink,
    HardLink
};

struct SnapshotFS {
    const char *name;
    Type type;
    mode_t mode;
    const char *content;
    size_t length;
};

} // namespace fs
} // namespace divine

#endif
