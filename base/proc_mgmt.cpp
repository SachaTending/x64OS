#include <vfs.hpp>
#include <frg/std_compat.hpp>
#include <frg/vector.hpp>
#include <new>
#include <sched.hpp>
#include <libc.h>
#include <logging.hpp>
#include <mmap.h>
#include <cpu.h>

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
    char *cwd;
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

proc_handle *mgmt_find_handle(size_t handle, size_t pid=getpid()) {
    proc_info *proc = mgmt_find_proc(pid);
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
    if (handle2 == NULL) {
        log.debug("write(%u, 0x%lx, %u): unknown handle\n", handle, buf, count);
        return;
    }
    handle2->node->write(handle2->node, buf, count);
    //log.debug("write(%u, 0x%lx, %u);\n", handle, buf, count);
}

void mgmt_syscall_ioctl(size_t handle, uint64_t req, void *s) {
    proc_handle *handle2 = mgmt_find_handle(handle);
    if (handle2 == NULL) {
        log.debug("ioctl(%u, %lu, 0x%lx): unknown handle\n", handle, req, s);
        return;
    }
    handle2->node->ioctl(handle2->node, (int)req, s);
}

size_t mgmt_syscall_read(size_t handle, void *buf, size_t count) {
    //log.debug("%lu: read(%lu, 0x%lx, %lu);\n", getpid(), handle, buf, count);
    proc_handle *handle2 = mgmt_find_handle(handle);
    if (handle2 == NULL) return;
    return handle2->node->read(handle2->node, buf, count);
}

size_t mgmt_syscall_seek(size_t handle, size_t seek_pos, int whence) {
    proc_handle *handle2 = mgmt_find_handle(handle);
    if (handle2 == NULL) return;
    size_t o = handle2->node->seek(handle2->node, seek_pos, whence);
    //log.debug("%lu: seek(%lu, %lu, %d) = %lu\n", getpid(), handle, seek_pos, whence, o);
    return o;
}

void mgmt_syscall_getcwd(char *buffer, size_t len) {
    proc_info *proc = mgmt_get_current_proc();
    if (proc == NULL) return;
    size_t sl = strlen(proc->cwd);
    if (len > sl) {
        len = sl;
    }
    memcpy(buffer, proc->cwd, len);
    log.debug("%lu: getcwd(0x%lx, %lu);\n", getpid(), buffer, len);
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
    //log.debug("%lu: file opened, fd: %lu\n", getpid(), handle->id);
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

int mgmt_set_tcb(void *ptr) {
    task_t *task = get_current_task();
    task->tls = (uint64_t)ptr;
    //log.info("%lu: set fs base to 0x%lx\n", getpid(), task->tls);
    wrmsr(0xc0000100, task->tls);
    return 0;
}

static int mgmt_force_open(size_t pid, const char *path) {
    log.debug("%lu: open(%s);\n", pid, path);
    proc_info *proc = mgmt_find_proc(pid);
    if (proc == NULL) {
        log.debug("%lu: process is not registered.\n", pid);
        return -1;
    }// Check if file already opened
    for (size_t i=0;i<(proc->handles.size());i++) {
        if (!strcmp(proc->handles[i]->path, path)) {
            log.debug("%lu: file already opened\n", pid);
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
    log.debug("%lu: file opened, fd: %lu\n", pid, handle->id);
    return handle->id;
}

void mgmt_dup(size_t pid, int fd) {
    log.debug("%lu: dup(%d);\n", pid, fd);
    proc_handle *h = mgmt_find_handle(fd, pid);
    if (h == NULL) {
        log.debug("%lu: dup(%d): file not found.\n", pid, fd);
        return;
    }
    proc_handle *clone = new proc_handle;
    proc_info *proc = mgmt_find_proc(pid);
    clone->id = proc->next_fd;
    clone->path = strdup(h->path);
    clone->node = h->node;
    proc->next_fd++;
    proc->handles.push_back(clone);
}
int exec(const char *path, int argc, char *argv[], char *envp[]);
size_t mgmt_syscall_exec(const char *path, char **argv, char **env) {
    int argc = 0;
    while (argv[argc] != NULL) {
        //log.debug("argv[%d]=0x%lx\n", argc, argv[argc]);
        argc++;
    }
    char **argv2 = new char*[argc];
    memset(argv2, 0, sizeof(char **)*(argc));
    for (int i=0;i<argc;i++) {
        argv2[i] = (char *)strdup(argv[i]);
    }
    int envc = 0;
    for (int i=0;env[i]!=NULL;i++) {
        envc = i;
    }
    char **env2 = new char *[envc];
    memset(env2, 0, sizeof(char **)*(envc));
    for (int i=0;i<envc;i++) {
        env2[i] = (char *)strdup(env[i]);
    }
    //log.debug("argc: %d\n", argc);
    size_t ret = exec(path, argc, argv2, env2);
    // TODO: Free argv2 and env2
    return ret;
}

void mgmt_waitpid(int pid) {
    
}

void mgmt_on_new_program(size_t pid) {
    proc_info *proc = new proc_info;
    proc->pid = pid;
    proc->handles = proc_handles_t();
    proc->next_fd = 0;
    proc->cwd = (const char *)strdup("/");
    log.debug("Registered process %lu\n", pid);
    proc_infos.push(proc);
    int ret = mgmt_force_open(pid, "/dev/console");
    if (ret >= 0) {
        mgmt_dup(pid, 0);
        mgmt_dup(pid, 1);
    }
    return;
}