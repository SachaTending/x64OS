#include <vfs.hpp>
#include <frg/std_compat.hpp>
#include <frg/vector.hpp>
#include <new>
#include <sched.hpp>
#include <libc.h>
#include <logging.hpp>
#include <mmap.h>

static Logger log("Process manager");

struct proc_handle {
    size_t id;
    const char *path;
    vfs_node_t *node;
};

typedef frg::vector<proc_handle *, frg::stl_allocator> proc_handles_t;

struct proc_info {
    size_t pid;
    proc_handles_t handles;
    size_t next_fd;
};

typedef frg::vector<proc_info *, frg::stl_allocator> proc_infos_t;

proc_infos_t proc_infos = proc_infos_t();

proc_info *mgmt_find_proc(size_t pid) {
    for (size_t i=0;i<proc_infos.size();i++) {
        if (proc_infos[i]->pid == pid) {
            return proc_infos[i];
        }
    }
    return NULL;
}

proc_info *mgmt_get_current_proc() {
    return mgmt_find_proc(getpid());
}

proc_handle *mgmt_find_handle(size_t handle) {
    proc_info *proc = mgmt_get_current_proc();
    if (proc == NULL) return NULL;
    for (size_t i=0;i<(proc->handles.size());i++) {
        if (proc->handles[i]->id == handle) {
            return proc->handles[i];
        }
    }
    return NULL;
}

void mgmt_syscall_write(size_t handle, void *buf, size_t count) {
    proc_handle *handle2 = mgmt_find_handle(handle);
    if (handle2 == NULL) return;
    handle2->node->write(handle2->node, buf, count);
}

void mgmt_syscall_read(size_t handle, void *buf, size_t count) {
    proc_handle *handle2 = mgmt_find_handle(handle);
    if (handle2 == NULL) return;
    handle2->node->read(handle2->node, buf, count);
}

// Returns fd for file
size_t mgmt_syscall_open(const char *path) {
    log.debug("%lu: open(%s);\n", getpid(), path);
    proc_info *proc = mgmt_get_current_proc();
    if (proc == NULL) {
        log.debug("%lu: process is not registered.\n", getpid());
        return -1;
    }// Check if file already opened
    for (size_t i=0;i<(proc->handles.size());i++) {
        if (!strcmp(proc->handles[i]->path, path)) {
            log.debug("%lu: file already opened\n", getpid());
            return proc->handles[i]->id;
        }
    }
    vfs_node_t *node = vfs_get_node(path);
    if (node == NULL) {
        return VFS_FILE_NOT_FOUND;
    }
    proc_handle *handle = new proc_handle;
    if (handle == NULL) {
        // Failed to allocate proc_handle structure, bruh.
        node->close(node);
        return -1;
    }
    handle->node = node;
    handle->path = strdup(path);
    handle->id = proc->next_fd;
    proc->next_fd++;
    proc->handles.push(handle);
    log.debug("%lu: file opened, fd: %lu\n", getpid(), handle->id);
    return handle->id;
}

void *mgmt_mmap(uintptr_t hint, size_t length, uint64_t flags, int fdnum, size_t offset) {
    vfs_node_t *node;
    //log.debug("mmap: hint: 0x%lx, length: %lu, flags: %lu, fdnum: %d, offset: %lu, prot: %lu, flags(real): %lu\n", hint, length, flags, fdnum, offset,
    //    (int)flags >> 32, flags & 0xffffffff);
    if (fdnum != -1) {
        node = mgmt_find_handle(fdnum)->node;
    }
    return mmap(get_current_task()->pgm, hint, length, (int)(flags >> 32), flags, node, offset);
}

void mgmt_on_new_program(size_t pid) {
    proc_info *proc = new proc_info;
    proc->pid = pid;
    proc->handles = proc_handles_t();
    proc->next_fd = 0;
    log.debug("Registered process %lu\n", pid);
    proc_infos.push(proc);
    return;
}