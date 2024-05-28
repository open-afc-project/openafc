/* Copyright (C) 2023 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
 * that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy of which is
 * included with this software program. */

#define __DEFINED_struct__IO_FILE
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <semaphore.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <map>

#define AEP_PATH_MAX PATH_MAX
#define AEP_FILENAME_MAX FILENAME_MAX

#define HASH_SIZE USHRT_MAX

/* debug flags */
#define DBG_LOG 1 /* statistic log */
#define DBG_DBG 2 /* debug messages */
#define DBG_CACHED 8 /* cached file IO */
#define DBG_ANY 4 /* any file IO */

#define aep_assert(cond, format, ...) \
    if (!(cond)) { \
        fprintf(stderr, format " Abort!\n", ##__VA_ARGS__); \
        if (aep_debug) { \
            dprintf(logfile, format " Abort!\n", ##__VA_ARGS__); \
        } \
        abort(); \
    }
#define dbg(format, ...) \
    if (aep_debug & DBG_DBG) { \
        dprintf(logfile, "%d: " format "\n", getpid(), ##__VA_ARGS__); \
    }
#define dbge(format, ...) \
    fprintf(stderr, format " Error!\n", ##__VA_ARGS__); \
    if (aep_debug) { \
        dprintf(logfile, format " Error!\n", ##__VA_ARGS__); \
    }
#define dbgd(format, ...) \
    if (aep_debug & DBG_CACHED) { \
        dprintf(logfile, "data " format "\n", ##__VA_ARGS__); \
    }
#define dbgo(format, ...) \
    if (aep_debug & DBG_ANY) { \
        dprintf(logfile, "orig " format "\n", ##__VA_ARGS__); \
    }
#define dbgl(format, ...) \
    if (aep_debug & DBG_LOG) { \
        dprintf(logfile, format "\n", ##__VA_ARGS__); \
    }

typedef int (*orig_fcntl_t)(int, int, ...);

/* from musl-1.2.3/src/dirent/__dirent.h */
struct __dirstream {
        off_t tell;
        int fd;
        int buf_pos;
        int buf_end;
        volatile int lock[1];
        /* Any changes to this struct must preserve the property:
         * offsetof(struct __dirent, buf) % sizeof(off_t) == 0 */
        char buf[2048];
};

#ifndef __GLIBC__
/* from musl-1.2.3/src/stat/fstatat.c */
struct statx {
        uint32_t stx_mask;
        uint32_t stx_blksize;
        uint64_t stx_attributes;
        uint32_t stx_nlink;
        uint32_t stx_uid;
        uint32_t stx_gid;
        uint16_t stx_mode;
        uint16_t pad1;
        uint64_t stx_ino;
        uint64_t stx_size;
        uint64_t stx_blocks;
        uint64_t stx_attributes_mask;
        struct {
                int64_t tv_sec;
                uint32_t tv_nsec;
                int32_t pad;
        } stx_atime, stx_btime, stx_ctime, stx_mtime;
        uint32_t stx_rdev_major;
        uint32_t stx_rdev_minor;
        uint32_t stx_dev_major;
        uint32_t stx_dev_minor;
        uint64_t spare[14];
};

/* from musl-1.2.3/src/internal/stdio_impl.h */
struct _IO_FILE {
        unsigned flags;
        unsigned char *rpos, *rend;
        int (*close)(FILE *);
        unsigned char *wend, *wpos;
        unsigned char *mustbezero_1;
        unsigned char *wbase;
        size_t (*read)(FILE *, unsigned char *, size_t);
        size_t (*write)(FILE *, const unsigned char *, size_t);
        off_t (*seek)(FILE *, off_t, int);
        unsigned char *buf;
        size_t buf_size;
        FILE *prev, *next;
        int fd;
        int pipe_pid;
        long lockcount;
        int mode;
        volatile int lock;
        int lbf;
        void *cookie;
        off_t off;
        char *getln_buf;
        void *mustbezero_2;
        unsigned char *shend;
        off_t shlim, shcnt;
        FILE *prev_locked, *next_locked;
        struct __locale_struct *locale;
};
#endif /* __GLIBC__ */

typedef struct {
        unsigned int read_remote_size;
        unsigned int read_remote;
        unsigned int read_remote_time;
        unsigned int read_cached_size;
        unsigned int read_cached;
        unsigned int read_cached_time;
        unsigned int read_write_size;
        unsigned int read_write;
        unsigned int read_write_time;
} aep_statistic_t;

static struct stat def_stat = {.st_dev = 0x72,
    .st_ino = 0x6ea7ca04,
    .st_nlink = 0x1,
    .st_mode = 0, /*  S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO - directory,
          S_IFREG | S_IRUSR | S_IRGRP | S_IROTH - file */
    .st_uid = 0x4466,
    .st_gid = 0x592,
    .st_rdev = 0,
    .st_size = 0,
    .st_blksize = 0x80000,
    .st_blocks = 0, /* st_size / 512 rounded up */
    .st_atim = {0x63b45b04, 0},
    .st_mtim = {0x63b45b04, 0},
    .st_ctim = {0x63b45b04, 0}};

static struct statx def_statx = {.stx_mask = 0x17ff,
    .stx_blksize = 0x80000,
    .stx_attributes = 0,
    .stx_nlink = 1,
    .stx_uid = 0x4466,
    .stx_gid = 0x592,
    .stx_mode = 0, /*  S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO - directory,
               S_IFREG | S_IRUSR | S_IRGRP | S_IROTH - file */
    .stx_ino = 0x6ea7ca04,
    .stx_size = 0,
    .stx_blocks = 0, /* stx_size/512 rounded up */
    .stx_attributes_mask = 0x203000,
    .stx_atime = {0x63b45b04, 0},
    .stx_btime = {0x63b45b04, 0},
    .stx_ctime = {0x63b45b04, 0},
    .stx_mtime = {0x63b45b04, 0},
    .stx_rdev_major = 0,
    .stx_rdev_minor = 0,
    .stx_dev_major = 0,
    .stx_dev_minor = 0x72};

typedef struct fe_t fe_t;

typedef struct fe_t {
        fe_t *next, *down;
        char *name;
        int64_t size;
} fe_t;

typedef struct {
        fe_t *fe;
        FILE file;
        DIR dir;
        off_t off;
        char *tpath;
        struct dirent dirent;
        fe_t *readdir_p;
} data_fd_t;

/* the root "/" entry of file tree */
static fe_t tree = {};
/* open files array */
static std::map<int, data_fd_t> data_fds;

/* Dynamically allocated buffers. Sometime in the future I'll free them */
static uint8_t *filelist; /* row file tree buffer */
static fe_t *fes; /* file tree */

static aep_statistic_t aepst = {};

/* configuration from getenv() */
static char *cache_path;
static uint64_t max_cached_file_size;
static uint64_t max_cached_size;
static uint32_t aep_debug = 0;
static char *ae_mountpoint = NULL; /* The path in which afc-engine seeking for static data */
static size_t strlen_ae_mountpoint;
static char *real_mountpoint = NULL; /* The path in which static data really is */
static bool aep_use_gs = false;
static int logfile = -1;
static volatile int64_t *cache_size;
static volatile int8_t *open_files;
static sem_t *shmem_sem;
static int64_t claimed_size;

static data_fd_t *fd_get_data_fd(const int fd);
static char *fd_get_dbg_name(const int fd);
static void fd_set_dbg_name(const int fd, const char *tpath);
static void fd_rm(const int fd, bool closeit = false);

static int download_file_nfs(data_fd_t *data_fd, char *dest);
static ssize_t read_remote_data_nfs(void *destv, size_t size, char *tpath, off_t off);
static int download_file_gs(data_fd_t *data_fd, char *dest);
static ssize_t read_remote_data_gs(void *destv, size_t size, char *tpath, off_t off);
static int init_gs();
static void reduce_cache(uint64_t size);
static bool fd_is_remote(int fd);

/* free string allocated by realpath() in is_remote_file() */
static inline void free_tpath(char *tpath)
{
    if (tpath) {
        free(tpath - strlen_ae_mountpoint);
    }
}

static inline void prn_statistic()
{
    if (!(aep_debug & (DBG_LOG | DBG_DBG))) {
        return;
    }
    dprintf(logfile, "statistics: remoteIO %u/%u/%u cachedIO %u/%u/%u dl %u/%u/%u cs %lu\n", aepst.read_remote, aepst.read_remote_size,
        aepst.read_remote_time, aepst.read_cached, aepst.read_cached_size, aepst.read_cached_time, aepst.read_write, aepst.read_write_size,
        aepst.read_write_time, *cache_size);
}

static void starttime(struct timeval *tv)
{
    gettimeofday(tv, NULL);
}

static unsigned int stoptime(struct timeval *tv)
{
    struct timeval tv1;
    int us;

    gettimeofday(&tv1, NULL);
    us = (tv1.tv_sec - tv->tv_sec) * 1000000 + tv1.tv_usec - tv->tv_usec;
    return (unsigned int)us;
}

static sem_t *semopen(const char *fname)
{
    int i;
    sem_t *sem;
    char *tmp = strdup(fname);

    for (i = 1; tmp[i]; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '_';
        }
    }
    sem = sem_open(tmp, O_CREAT, 0666, 1);
    free(tmp);
    aep_assert(sem, "sem_open");
    return sem;
}

static inline FILE *orig_fopen(const char *path, const char *mode)
{
    typedef FILE *(*orig_fopen_t)(const char *, const char *);
    orig_fopen_t orig = (orig_fopen_t)dlsym(RTLD_NEXT, "fopen");
    FILE *ret;

    ret = (*orig)(path, mode);
    if (ret) {
        fd_set_dbg_name(fileno(ret), path);
    }
    return ret;
}

static inline size_t orig_fread(void *destv, size_t size, size_t nmemb, FILE *f)
{
    typedef size_t (*orig_fread_t)(void *, size_t, size_t, FILE *);
    orig_fread_t orig = (orig_fread_t)dlsym(RTLD_NEXT, "fread");
    size_t ret;

    ret = (*orig)(destv, size, nmemb, f);
    return ret;
}

static inline int orig_fclose(FILE *f)
{
    typedef int (*orig_fclose_t)(FILE *);
    orig_fclose_t orig = (orig_fclose_t)dlsym(RTLD_NEXT, "fclose");

    return (*orig)(f);
}

static inline int orig_open(const char *pathname, int flags, ...)
{
    typedef int (*orig_open_t)(const char *, int, ...);
    int fd;
    orig_open_t orig = (orig_open_t)dlsym(RTLD_NEXT, "open");

    fd = (*orig)(pathname, flags, 0666);
    fd_set_dbg_name(fd, pathname);
    return fd;
}

static inline int orig_openat(int dirfd, const char *pathname, int flags, ...)
{
    typedef int (*orig_openat_t)(int, const char *, int);
    orig_openat_t orig = (orig_openat_t)dlsym(RTLD_NEXT, "openat");
    int fd;

    fd = (*orig)(dirfd, pathname, flags);
    fd_set_dbg_name(fd, pathname);
    return fd;
}

static inline int orig_close(int fd)
{
    typedef int (*orig_close_t)(int);
    orig_close_t orig = (orig_close_t)dlsym(RTLD_NEXT, "close");

    return (*orig)(fd);
}

static inline int orig_stat(const char *pathname, struct stat *statbuf)
{
    typedef int (*orig_stat_t)(const char *, struct stat *);
    orig_stat_t orig = (orig_stat_t)dlsym(RTLD_NEXT, "stat");

    return (*orig)(pathname, statbuf);
}

static inline ssize_t orig_read(int fd, void *buf, size_t count)
{
    typedef ssize_t (*orig_read_t)(int, void *, size_t);
    ssize_t ret;
    orig_read_t orig = (orig_read_t)dlsym(RTLD_NEXT, "read");

    ret = (*orig)(fd, buf, count);
    // dbgo("orig_read(%d, %zu) %zd", fd, count, ret);
    return ret;
}

static inline off_t orig_lseek(int fd, off_t offset, int whence)
{
    typedef off_t (*orig_lseek_t)(int, off_t, int);
    orig_lseek_t orig = (orig_lseek_t)dlsym(RTLD_NEXT, "lseek");

    return (*orig)(fd, offset, whence);
}

static inline struct dirent *orig_readdir(DIR *dir)
{
    typedef struct dirent *(*orig_readdir_t)(DIR * dir);
    orig_readdir_t orig = (orig_readdir_t)dlsym(RTLD_NEXT, "readdir");

    return (*orig)(dir);
}

static inline int orig_fstat(int fd, struct stat *statbuf)
{
    typedef int (*orig_fstat_t)(int, struct stat *);
    orig_fstat_t orig = (orig_fstat_t)dlsym(RTLD_NEXT, "fstat");

    return (*orig)(fd, statbuf);
}

static inline int orig_lstat(const char *pathname, struct stat *statbuf)
{
    typedef int (*orig_lstat_t)(const char *, struct stat *);
    orig_lstat_t orig = (orig_lstat_t)dlsym(RTLD_NEXT, "lstat");

    return (*orig)(pathname, statbuf);
}

static inline int orig_access(const char *pathname, int mode)
{
    typedef int (*orig_access_t)(const char *, int);
    orig_access_t orig = (orig_access_t)dlsym(RTLD_NEXT, "access");
    int ret;

    ret = (*orig)(pathname, mode);
    return ret;
}

static inline void orig_rewind(FILE *stream)
{
    typedef int (*orig_rewind_t)(FILE *);
    orig_rewind_t orig = (orig_rewind_t)dlsym(RTLD_NEXT, "rewind");

    (*orig)(stream);
}

static inline DIR *orig_opendir(const char *name)
{
    typedef DIR *(*orig_opendir_t)(const char *name);
    orig_opendir_t orig = (orig_opendir_t)dlsym(RTLD_NEXT, "opendir");
    DIR *ret;

    ret = (*orig)(name);
    if (ret) {
        fd_set_dbg_name(dirfd(ret), name);
    }
    return ret;
}

static inline int orig_closedir(DIR *dirp)
{
    typedef int (*orig_closedir_t)(DIR *);
    orig_closedir_t orig = (orig_closedir_t)dlsym(RTLD_NEXT, "closedir");

    return (*orig)(dirp);
}

static inline DIR *orig_fdopendir(int fd)
{
    typedef DIR *(*orig_fdopendir_t)(int);
    orig_fdopendir_t orig = (orig_fdopendir_t)dlsym(RTLD_NEXT, "fdopendir");

    return (*orig)(fd);
}

static inline int orig_fgetc(FILE *stream)
{
    typedef int (*orig_fgetc_t)(FILE *);
    orig_fgetc_t orig = (orig_fgetc_t)dlsym(RTLD_NEXT, "fgetc");

    return (*orig)(stream);
}

/* find file in file tree by name. Return file entry pointer or NULL  */
static fe_t *find_fe(char *tpath)
{
    char *c = tpath + 1; /* skip first / */
    fe_t *cfe = &tree;

    while (c < tpath + strlen(tpath)) {
        char name[AEP_FILENAME_MAX] = {};
        int name_off = 0;

        while (*c && *c != '/') {
            name[name_off] = *c;
            name_off++;
            c++;
        }
        c++; /* skip final / */
        name[name_off] = 0;
        cfe = cfe->down;
        while (cfe) {
            if (strcmp(cfe->name, name)) {
                cfe = cfe->next;
                if (!cfe) {
                    return NULL;
                }
            } else {
                break;
            }
        }
    }
    return cfe;
}

/* FILE function pointers stubs */
#ifndef __GLIBC__
/* never used */
static size_t f_read(FILE *f, unsigned char *buf, size_t size)
{
    dbgo("FILE->read(%d(%s), %zu)", fileno(f), fd_get_dbg_name(fileno(f)), size);
    return 0;
}
/* never used */
static size_t f_write(FILE *f, const unsigned char *buff, size_t size)
{
    dbgo("FILE->write(%d(%s), %zu)", fileno(f), fd_get_dbg_name(fileno(f)), size);
    f->wpos = 0;
    return 0;
}

static off_t f_seek(FILE *f, off_t off, int whence)
{
    data_fd_t *data_fd = fd_get_data_fd(fileno(f));
    dbgd("FILE->seek(%d(%s), %jd, %d)", fileno(f), fd_get_dbg_name(fileno(f)), off, whence);
    switch (whence) {
        case SEEK_SET: /* 0 */
            data_fd->off = off;
            break;
        case SEEK_CUR: /* 0 */
            data_fd->off += off;
            break;
        case SEEK_END:
            data_fd->off = data_fd->fe->size + off;
            break;
    }
    dbgd("FILE->seek(%d(%s), %jd, %d) %jd", fileno(f), fd_get_dbg_name(fileno(f)), off, whence, data_fd->off);
    return data_fd->off;
}

/* never used */
static int f_close(FILE *f)
{
    dbgd("FILE->close(%d(%s))", fileno(f), fd_get_dbg_name(fileno(f)));
    fd_rm(fileno(f));
    return 0;
}
#endif

static inline void cache_size_set(int64_t size)
{
    sem_wait(shmem_sem);
    *cache_size += size;
    sem_post(shmem_sem);
}

static inline int64_t cache_size_get()
{
    int64_t tmp;

    sem_wait(shmem_sem);
    tmp = *cache_size;
    sem_post(shmem_sem);
    return tmp;
}

/* 16 bits hash */
static uint16_t hash_fname(const char *str)
{
    uint8_t cor = 0; /* to do USGS_1_n32w099 and USGS_1_n33w098 differ */

    uint16_t hash = 0x5555;
    str++; /* skip '/' */
    while (*str) {
        hash ^= *((uint16_t *)str) + cor;
        str += 2;
        cor++;
    }
    return hash;
}

static uint8_t files_open_set(const char *name, int val)
{
    aep_assert(strcmp(name, "noname"), "files_open_set(noname)");
    uint16_t fno = hash_fname(name);
    uint8_t ret;

    sem_wait(shmem_sem);
    open_files[fno] += val;
    if (open_files[fno] < 0) {
        open_files[fno] = 0;
    }
    ret = open_files[fno];
    sem_post(shmem_sem);
    return ret;
}

static uint8_t files_open_get(const char *name)
{
    aep_assert(strcmp(name, "noname"), "files_open_get(noname)");
    uint16_t fno = hash_fname(name);
    uint8_t ret;

    sem_wait(shmem_sem);
    ret = open_files[fno];
    sem_post(shmem_sem);
    return ret;
}

static size_t read_data(void *destv, size_t size, data_fd_t *data_fd)
{
    dbg("read_data(%s)", data_fd->tpath);
    char fakepath[AEP_PATH_MAX];
    ssize_t ret;
    ssize_t (*read_remote_data)(void *destv, size_t size, char *tpath, off_t off) = aep_use_gs ? read_remote_data_gs : read_remote_data_nfs;
    /* define pointer to download file func */
    int (*download_file)(data_fd_t * data_fd, char *dest);
    download_file = aep_use_gs ? download_file_gs : download_file_nfs;
    struct stat stat;
    sem_t *sem;
    bool is_cached = false;

    strncpy(fakepath, cache_path, AEP_PATH_MAX);
    strncat(fakepath, data_fd->tpath, AEP_PATH_MAX - strlen(fakepath));

    sem = semopen(data_fd->tpath);
    // dbg("read_data %s", data_fd->tpath);
    sem_wait(sem);

    /* download whole file to cache if possible */
    if (!orig_stat(fakepath, &stat)) {
        if (data_fd->fe->size == stat.st_size) {
            is_cached = true;
        }
        if (!is_cached && data_fd->fe->size <= (int64_t)max_cached_file_size) {
            if (data_fd->fe->size + cache_size_get() > (int64_t)max_cached_size) {
                reduce_cache(data_fd->fe->size);
            }
            if (data_fd->fe->size + cache_size_get() < (int64_t)max_cached_size) {
                // dbg("download %s", data_fd->tpath);
                if (!download_file(data_fd, fakepath)) {
                    cache_size_set(data_fd->fe->size);
                    dbg("download %s done, cs %ld", data_fd->tpath, *cache_size);
                    is_cached = true;
                } else {
                    dbg("download %s failed, cs %ld", data_fd->tpath, *cache_size);
                }
            } else {
                dbgl("Can't cache %s %lu cs %ld", data_fd->tpath, data_fd->fe->size, *cache_size);
                dbg("Can't cache %s %lu cs %ld", data_fd->tpath, data_fd->fe->size, *cache_size);
            }
        }
    }

    if (is_cached) {
        int fd;
        struct timeval tv;
        unsigned int us;

        starttime(&tv);
        fd = orig_open(fakepath, O_RDONLY);
        aep_assert(fd >= 0, "read_data(%s) open", fakepath);
        orig_lseek(fd, data_fd->off, SEEK_SET);
        ret = orig_read(fd, destv, size);
        aep_assert(ret >= 0, "read_data(%s) read", fakepath);
        orig_close(fd);
        us = stoptime(&tv);
        sem_post(sem);
        sem_close(sem);
        dbgl("read cached file %s size %zu time %u us cache size %zu", data_fd->tpath, ret, us, *cache_size);
        aepst.read_cached++;
        aepst.read_cached_size += ret;
        aepst.read_cached_time += us;
    } else {
        sem_post(sem);
        sem_close(sem);
        ret = read_remote_data(destv, size, data_fd->tpath, data_fd->off);
        aep_assert(ret >= 0, "read_data(%s) read_remote_data", fakepath)
    }
    // dbg("read_data %s done", data_fd->tpath);
    data_fd->off += ret;
    dbgd("read_data(%s, %zu) %zd", data_fd->tpath, size, ret);
    return ret;
}

/* create fake file descriptor, FILE* and DIR*. Returns like open() */
static int fd_add(char *tpath)
{
    int fd;
    fe_t *fe;
    char fakepath[AEP_PATH_MAX];
    char *p = fakepath;
    struct stat statbuf;
    data_fd_t *data_fd;

    fe = find_fe(tpath);
    if (!fe) {
        return -1;
    }
    dbg("fd_add(%s) size 0x%jx", tpath, fe->size);

    strncpy(fakepath, cache_path, AEP_PATH_MAX);
    strncat(fakepath, tpath, AEP_PATH_MAX - strlen(fakepath));

    /* create cache file */
    if (orig_stat(fakepath, &statbuf)) {
        for (p = fakepath; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                mkdir(fakepath, 0777);
                *p = '/';
            }
        }
        if (fe->size) { /* it's a file, touch it */
            int fd;

            fd = orig_open(fakepath, O_CREAT | O_RDWR);
            aep_assert(fd >= 0, "fd_add(%s) touch errno %d", fakepath, errno);
            orig_close(fd);
        } else { /* is dir */
            mkdir(fakepath, 0777);
        }
    }

    if (fe->size) {
        files_open_set(tpath, 1);
    }
    fd = orig_open(fakepath, O_RDONLY);
    data_fd = &data_fds[fd];
    memset(data_fd, 0, sizeof(data_fd_t));
    aep_assert(!orig_fstat(fd, &statbuf), "fd_add(%s) fstat", tpath);
    data_fd->fe = fe;
    data_fd->off = 0;
#ifndef __GLIBC__
    data_fd->file.fd = fd;
    data_fd->file.read = f_read;
    data_fd->file.write = f_write;
    data_fd->file.seek = f_seek;
    data_fd->file.close = f_close;
#endif
    data_fd->readdir_p = NULL;
    data_fd->dir.fd = fd;
    free_tpath(data_fd->tpath); /* new std::map is zeroed */
    data_fd->tpath = tpath;
    dbg("fd_add(%s) %d done", tpath, fd);
    return fd;
}

static bool fd_is_remote(int fd)
{
    data_fd_t *data_fd = fd_get_data_fd(fd);
    return data_fd && data_fd->fe;
}

static void fd_rm(const int fd, bool closeit)
{
    data_fd_t *data_fd = fd_get_data_fd(fd);

    dbg("fd_rm(%d)", fd);
    if (!data_fd) {
        return;
    }
    if (data_fd->fe) {
        if (data_fd->fe->size) {
            files_open_set(data_fd->tpath, -1);
        }
        if (closeit) {
            orig_close(fd);
        }
    }
    free_tpath(data_fd->tpath);
    data_fds.erase(fd);
    dbg("fd_rm(%d) done", fd);
}

static inline data_fd_t *fd_get_data_fd(const int fd)
{
    auto search = data_fds.find(fd);

    if (search == data_fds.end()) {
        return NULL;
    }
    return &search->second;
}

static inline char *fd_get_dbg_name(const int fd)
{
    data_fd_t *data_fd = fd_get_data_fd(fd);
    if (data_fd) {
        return data_fd->tpath;
    }
    return (char *)"noname";
}

static inline void fd_set_dbg_name(const int fd, const char *tpath)
{
    data_fd_t *data_fd = fd_get_data_fd(fd);

    if ((aep_debug & DBG_ANY) && data_fd) {
        strncpy(data_fd->tpath, tpath, AEP_PATH_MAX);
    }
}

static bool is_remote_file(const char *path, char **tpath)
{
    char *rpath = NULL;

    if (!path) {
        *tpath = (char *)path;
        return false;
    }

    rpath = realpath(path, rpath);
    if (!rpath) {
        *tpath = (char *)path;
        return false;
    }

    if (strncmp(rpath, ae_mountpoint, strlen_ae_mountpoint) == 0) {
        if (strlen(rpath) == strlen_ae_mountpoint || rpath[strlen_ae_mountpoint] == '/') {
            *tpath = (char *)rpath + strlen_ae_mountpoint;
            dbgd("is_remote_file(%s -> %s)", path, *tpath);
            return true;
        } else {
            free(rpath);
            *tpath = (char *)path;
            dbgo("is_remote_file(%s)", *tpath);
            return false;
        }
    }
    free(rpath);
    *tpath = (char *)path;
    return false;
}

static int ftw_reduce_callback(const char *fpath, const struct stat *sb, int typeflag)
{
    if (typeflag == FTW_F && sb->st_size) {
        char *tpath = (char *)fpath + strlen(cache_path);
        if (!files_open_get(tpath)) {
            sem_t *sem;
            sem = semopen(tpath);
            sem_wait(sem);
            aep_assert(!truncate(fpath, 0), "truncate");
            sem_post(sem);
            cache_size_set(-sb->st_size);
            if (cache_size_get() + claimed_size <= (int64_t)max_cached_size) {
                return -1;
            }
            dbg("truncate(%s) cs %ld", tpath, *cache_size);
        }
    }
    return 0;
}

static void reduce_cache(uint64_t size)
{
    // dbg("reduce_cache(%lu)", size);
    claimed_size = (int64_t)size;
    ftw(cache_path, ftw_reduce_callback, 100);
    // dbg("reduce_cache(%lu) done", size);
}

static int ftw_callback(const char *fpath, const struct stat *sb, int typeflag)
{
    *cache_size += sb->st_size;
    return 0;
}

/* This library entrypoint */
void __attribute__((constructor)) aep_init(void)
{
    int ret;
    struct stat statbuf;
    int fd;
    uint8_t *fl;
    fe_t *free_fes; /* next empty file entry from fes array */
    fe_t **stack, *cstack;
    fe_t *cfe = NULL; /* last file entry added to tree */
    uint8_t *filelist_end;
    uint8_t tab_prev = 0;
    uint32_t entries_size;
    char *filelist_path;
    char *tmp;
    int shm_fd;

    /* check env vars */
    tmp = getenv("AFC_AEP_DEBUG");
    aep_debug = tmp ? atoi(tmp) : 0;
    if (aep_debug) {
        char *logname;

        logname = getenv("AFC_AEP_LOGFILE");
        if (!logname) {
            dbge("AFC_AEP_LOGFILE env var is not defined, log disabled");
            aep_debug = 0;
        } else {
            logfile = orig_open(logname, O_CREAT | O_RDWR | O_APPEND);
            if (logfile < 0) {
                dbge("Can not open %s, log disabled", logname);
                aep_debug = 0;
            }
        }
    }
    tmp = getenv("AFC_AEP_REAL_MOUNTPOINT");
    aep_assert(tmp, "AFC_AEP_REAL_MOUNTPOINT env var is not defined");
    real_mountpoint = realpath(tmp, real_mountpoint);
    aep_assert(real_mountpoint, "AFC_AEP_REAL_MOUNTPOINT env var path does not exist");

    tmp = getenv("AFC_AEP_ENGINE_MOUNTPOINT");
    aep_assert(tmp, "AFC_AEP_ENGINE_MOUNTPOINT env var is not defined");
    ae_mountpoint = realpath(tmp, ae_mountpoint);
    aep_assert(ae_mountpoint, "AFC_AEP_ENGINE_MOUNTPOINT env var path does not exist");
    strlen_ae_mountpoint = strlen(ae_mountpoint);

    if (getenv("AFC_AEP_GS")) {
        aep_use_gs = true;
        init_gs();
    }
    filelist_path = getenv("AFC_AEP_FILELIST");
    aep_assert(filelist_path, "AFC_AEP_FILELIST env var is not defined");
    tmp = getenv("AFC_AEP_CACHE_MAX_FILE_SIZE");
    aep_assert(tmp, "AFC_AEP_CACHE_MAX_FILE_SIZE env var is not defined");
    max_cached_file_size = atoll(tmp);
    tmp = getenv("AFC_AEP_CACHE_MAX_SIZE");
    aep_assert(tmp, "AFC_AEP_CACHE_MAX_SIZE env var is not defined");
    max_cached_size = atoll(tmp);
    if (max_cached_file_size > max_cached_size) {
        max_cached_file_size = max_cached_size;
    }
    cache_path = getenv("AFC_AEP_CACHE");
    aep_assert(cache_path, "AFC_AEP_CACHE env var is not defined");

    /* read filelist */
    ret = orig_stat(filelist_path, &statbuf);
    if (ret) {
        dbge("Filelist is not found");
        exit(ret);
    }
    filelist = (uint8_t *)malloc(statbuf.st_size);
    if (!filelist) {
        dbge("Memory allocation");
        exit(-ENOMEM);
    }
    fd = orig_open(filelist_path, O_RDONLY);
    if (fd < 0) {
        dbge("File IO");
        exit(-EIO);
    }
    if (orig_read(fd, filelist, statbuf.st_size) != statbuf.st_size) {
        dbge("File IO");
        exit(-EIO);
    }
    orig_close(fd);

    fl = filelist;
    filelist_end = filelist + statbuf.st_size;

    /* alloc file tree entries */
    entries_size = *((uint32_t *)fl);
    fl += sizeof(uint32_t);
    entries_size += *((uint32_t *)fl);
    fl += sizeof(uint32_t);
    entries_size *= sizeof(fe_t);
    fes = (fe_t *)calloc(1, entries_size);
    if (!fes) {
        dbge("Memory allocation");
        exit(-ENOMEM);
    }
    free_fes = fes;

    stack = (fe_t **)calloc(1, (*((uint8_t *)fl) + 1) * sizeof(fe_t *));
    if (!(stack)) {
        dbge("Memory allocation");
        exit(-ENOMEM);
    }
    fl += sizeof(uint8_t);
    stack[0] = &tree;
    tree.name = (char *)"root"; /* debug */
    cstack = stack[0];

    /* fill file tree */
    while (fl < filelist_end) {
        uint8_t tab = 0;
        char *name;
        int64_t size;

        /* read next line */
        while (*fl == '\t') {
            tab++;
            (fl)++;
        }
        name = (char *)(fl);
        fl += strlen(name) + 1;

        size = *((int64_t *)fl);
        fl += sizeof(int64_t);
        if (tab != tab_prev) {
            if (tab < tab_prev) {
                cstack = stack[tab];
                cfe = cstack->down;
                while (cfe && cfe->next) {
                    cfe = cfe->next;
                }
            } else {
                stack[tab] = cfe;
                cstack = stack[tab];
            }
            tab_prev = tab;
        }
        if (!cstack->down) {
            cstack->down = free_fes;
        } else {
            cfe->next = free_fes;
        }
        cfe = free_fes;
        free_fes++;
        cfe->name = name;
        cfe->size = size;
    }
    free(stack);

    /* share cache size */
    shmem_sem = sem_open("aep_shmem_sem", O_CREAT, 0666, 1);
    aep_assert(shmem_sem, "aep_init:sem_open");
    shm_fd = shm_open("aep_shmem", O_RDWR | O_CREAT | O_EXCL, 0666);
    // dbg("aep_init");
    sem_wait(shmem_sem);
    if (shm_fd < 0) {
        // dbg("aep_init cache skip");
        /* O_CREAT | O_EXCL failed, so shared memory object already was initialized */
        shm_fd = shm_open("aep_shmem", O_RDWR, 0666);
        aep_assert(shm_fd >= 0, "shm_open");
        cache_size = (int64_t *)mmap(NULL, sizeof(int64_t) + HASH_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        aep_assert(cache_size, "mmap");
        open_files = (int8_t *)(cache_size + 1);
    } else {
        // dbg("aep_init recount cache");
        aep_assert(!ftruncate(shm_fd, sizeof(uint64_t) + HASH_SIZE), "aep_init:ftruncate");
        cache_size = (int64_t *)mmap(NULL, sizeof(int64_t) + HASH_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        aep_assert(cache_size, "mmap");
        open_files = (int8_t *)(cache_size + 1);
        memset((void *)cache_size, 0, sizeof(int64_t) + HASH_SIZE);
        /* count existing cache size */
        ftw(cache_path, ftw_callback, 100);
    }
    sem_post(shmem_sem);

    dbg("aep_init done cs %lu", *cache_size);
}

FILE *fopen(const char *path, const char *mode)
{
    FILE *ret;
    char *tpath;

    if (is_remote_file(path, &tpath)) {
        int fd;

        fd = fd_add(tpath);
        if (fd < 0) {
            return NULL;
        }
        ret = &(fd_get_data_fd(fd)->file);
        dbgd("fopen(%s, %s) %d", tpath, mode, fd);
    } else {
        ret = orig_fopen(path, mode);
        if (ret) {
            dbgo("fopen(%s, %s) %d", path, mode, fileno(ret));
            fd_rm(fileno(ret));
        } else {
            dbgo("fopen(%s, %s) -1", path, mode);
        }
    }
    return ret;
}

size_t fread(void *destv, size_t size, size_t nmemb, FILE *f)
{
    size_t ret;

    if (fd_is_remote(fileno(f))) {
        data_fd_t *data_fd = fd_get_data_fd(fileno(f));

        ret = read_data(destv, size * nmemb, data_fd);
        ret /= size;
        // dbgd("fread(%d(%s), %zu * %zu) %zu", fileno(f), fd_get_dbg_name(fileno(f)), size,
        // nmemb, ret);
    } else {
        ret = orig_fread(destv, size, nmemb, f);
        // dbgo("fread(%d, %zu * %zu) %zu", fileno(f), size, nmemb, ret);
    }
    return ret;
}

int fclose(FILE *f)
{
    int ret;

    dbg("fclose(%p)", f);

    if (fd_is_remote(fileno(f))) {
        dbgd("fclose(%d(%s))", fileno(f), fd_get_dbg_name(fileno(f)));
        fd_rm(fileno(f), true);
        ret = 0;
        prn_statistic();
    } else {
        dbgo("fclose(%d)", fileno(f));
        ret = orig_fclose(f);
    }

    dbg("fclose(%p) %d done", f, ret);
    return ret;
}

int open(const char *pathname, int flags, ...)
{
    int ret;
    char *tpath;

    if (is_remote_file(pathname, &tpath)) {
        ret = fd_add(tpath);
        dbgd("open(%s, %x) %d", tpath, flags, ret);
    } else {
        ret = orig_open(pathname, flags);
        dbgo("open(%s, %x) %d", tpath, flags, ret);
    }
    return ret;
}

int openat(int dirfd, const char *pathname, int flags, ...)
{
    int ret;
    char *tpath;

    if (is_remote_file(pathname, &tpath)) {
        ret = fd_add(tpath);
        dbgd("openat(%s, %x) %d", tpath, flags, ret);
    } else {
        ret = orig_openat(dirfd, pathname, flags);
        dbgo("openat(%d, %s, %x) %d", dirfd, tpath, flags, ret);
    }
    return ret;
}

int close(int fd)
{
    int ret = 0;

    if (fd_is_remote(fd)) {
        dbgd("close(%d(%s))", fd, fd_get_dbg_name(fd));
        fd_rm(fd, true);
    } else {
        ret = orig_close(fd);
        dbgo("close(%d(%s))=%d", fd, fd_get_dbg_name(fd), ret);
    }
    return ret;
}

int stat(const char *pathname, struct stat *statbuf)
{
    char *tpath;
    int ret;

    if (is_remote_file(pathname, &tpath)) {
        int fd;
        data_fd_t *data_fd;

        dbgd("stat(%s)", tpath);

        fd = fd_add(tpath);
        if (fd < 0) {
            return -1;
        }
        data_fd = fd_get_data_fd(fd);

        memcpy(statbuf, &def_stat, sizeof(struct stat));
        if (data_fd->fe->size) {
            statbuf->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
        } else {
            statbuf->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
        }
        statbuf->st_size = data_fd->fe->size;
        statbuf->st_blocks = (statbuf->st_size >> 9) + (statbuf->st_size & 0x1ff) ? 1 : 0;
        dbgd("stat(%s, 0x%lx)", tpath, data_fd->fe->size);
        fd_rm(fd, true);
        ret = 0;
    } else {
        ret = orig_stat(pathname, statbuf);
        dbgo("stat(%s) %d", tpath, ret);
    }
    return ret;
}

int fstat(int fd, struct stat *statbuf)
{
    int ret;

    if (fd_is_remote(fd)) {
        data_fd_t *data_fd;
        data_fd = fd_get_data_fd(fd);

        dbgd("fstat(%d(%s))", fd, fd_get_dbg_name(fd));

        memcpy(statbuf, &def_stat, sizeof(struct stat));
        if (data_fd->fe->size) {
            statbuf->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
        } else {
            statbuf->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
        }
        statbuf->st_size = data_fd->fe->size;
        statbuf->st_blocks = (statbuf->st_size >> 9) + (statbuf->st_size & 0x1ff) ? 1 : 0;
        ret = 0;
        dbgd("fstat(%s, 0x%lx, %s) %d", fd_get_dbg_name(fd), data_fd->fe->size, data_fd->fe->size ? "file" : "dir", ret);
    } else {
        ret = orig_fstat(fd, statbuf);
        dbgo("fstat(%d) %d", fd, ret);
    }
    return ret;
}

int lstat(const char *pathname, struct stat *statbuf)
{
    int ret;
    char *tpath;

    if (is_remote_file(pathname, &tpath) && 0) {
        int fd;
        data_fd_t *data_fd;

        fd = fd_add(tpath);
        if (fd < 0) {
            return -1;
        }
        data_fd = fd_get_data_fd(fd);

        memcpy(statbuf, &def_stat, sizeof(struct stat));
        if (data_fd->fe->size) {
            statbuf->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
        } else {
            statbuf->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
        }
        statbuf->st_size = data_fd->fe->size;
        statbuf->st_blocks = (statbuf->st_size >> 9) + (statbuf->st_size & 0x1ff) ? 1 : 0;
        ret = 0;
        dbgd("lstat(%s, 0x%lx, %s) %d", tpath, data_fd->fe->size, data_fd->fe->size ? "file" : "dir", ret);
        fd_rm(fd, true);
    } else {
        ret = orig_lstat(pathname, statbuf);
    }
    return ret;
}

int access(const char *pathname, int mode)
{
    int ret;
    char *tpath;

    if (is_remote_file(pathname, &tpath)) {
        fe_t *fe;

        fe = find_fe(tpath);
        ret = fe ? 0 : -1;
        dbgd("access(%s, %d) %d", tpath, mode, ret);
        free_tpath(tpath);
    } else {
        ret = orig_access(pathname, mode);
        dbgo("access(%s, %d) %d", pathname, mode, ret);
    }
    return ret;
}

/* the statx wrapper for musl */
long int syscall(long int __sysno, ...)
{
    typedef int (*orig_syscall_t)(long int, ...);
    void *ret;

    if (__sysno == SYS_statx) {
        /* __syscall(SYS_statx, fd, path, flag, 0x7ff, &stx); */
        va_list args;
        char *path;
        int dirfd;
        int flags;
        unsigned int mask;
        struct statx *st;
        char *tpath;

        va_start(args, __sysno);
        dirfd = va_arg(args, int);
        (void)dirfd;
        path = va_arg(args, char *);
        flags = va_arg(args, int);
        (void)flags;
        mask = va_arg(args, unsigned int);
        (void)mask;
        st = va_arg(args, struct statx *);
        va_end(args);
        if (is_remote_file(path, &tpath)) {
            fe_t *fe;

            fe = find_fe(tpath);
            if (!fe) {
                dbgd("SYS_statx(%s) -1", tpath);
                free_tpath(tpath);
                return -1;
            }

            dbgd("syscall(SYS_statx, dirfd:%d, path:%s, flags:0x%x, mask:0x%x) 0x%lx", dirfd, path, flags, mask, fe->size);
            memcpy(st, &def_statx, sizeof(struct statx));
            if (fe->size) {
                st->stx_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
                st->stx_size = fe->size;
                st->stx_blocks = (fe->size >> 9) + (fe->size & 0x1ff) ? 1 : 0;
            } else {
                st->stx_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
            }
            free_tpath(tpath);
            return 0;
        } else {
            orig_syscall_t orig = (orig_syscall_t)dlsym(RTLD_NEXT, "syscall");
            void *argsb = __builtin_apply_args();

            dbgo("SYS_statx(%d, %s)", dirfd, path);
#if __cplusplus
            ret = __builtin_apply((void (*)(...))(orig), argsb, 512);
#else
            ret = __builtin_apply((void (*)())(orig), argsb, 512);
#endif
            __builtin_return(ret);
        }
    } else {
        orig_syscall_t orig = (orig_syscall_t)dlsym(RTLD_NEXT, "syscall");
        void *argsb = __builtin_apply_args();

        dbgo("syscall(unsupported %ld)", __sysno);
#if __cplusplus
        ret = __builtin_apply((void (*)(...))(orig), argsb, 512);
#else
        ret = __builtin_apply((void (*)())(orig), argsb, 512);
#endif
        __builtin_return(ret);
    }
}

int fcntl(int fd, int cmd, ...)
{
    if (fd_is_remote(fd)) {
        aep_assert(cmd == F_SETLK, "fcntl(unsupported cmd=%d)", cmd);
        dbgd("fcntl(%s, %d)", fd_get_dbg_name(fd), cmd);
        return 0;
    } else {
        void *args = __builtin_apply_args();
        void *ret;
        orig_fcntl_t orig = (orig_fcntl_t)dlsym(RTLD_NEXT, "fcntl");

        dbgo("fcntl(%d, %d)", fd, cmd);
#if __cplusplus
        ret = __builtin_apply((void (*)(...))(*orig), args, 512);
#else
        ret = __builtin_apply((void (*)())(*orig), args, 512);
#endif
        __builtin_return(ret);
    }
}

ssize_t read(int fd, void *buf, size_t count)
{
    ssize_t ret;

    if (fd_is_remote(fd)) {
        data_fd_t *data_fd = fd_get_data_fd(fd);

        ret = read_data(buf, count, data_fd);
        // dbgd("read(%d(%s), %zu) %zd", fd, fd_get_dbg_name(fd), count, ret);
    } else {
        ret = orig_read(fd, buf, count);
        // dbgo("read(%d(%s), %zu) %zd", fd, fd_get_dbg_name(fd), count, ret);
    }
    return ret;
}

off_t lseek(int fd, off_t offset, int whence)
{
    off_t ret;

    if (fd_is_remote(fd)) {
        data_fd_t *data_fd = fd_get_data_fd(fd);

        aep_assert(whence == SEEK_SET, "lseek(%s, %ld, %d) unsupported whence", fd_get_dbg_name(fd), offset, whence);
        data_fd->off = offset;
        ret = 0;
        dbgd("lseek(%d(%s), %ld, %d) %ld", fd, fd_get_dbg_name(fd), offset, whence, ret);
    } else {
        ret = orig_lseek(fd, offset, whence);
        dbgo("lseek(%d(%s), %ld, %d) %ld", fd, fd_get_dbg_name(fd), offset, whence, ret);
    }
    return ret;
}

struct dirent *readdir(DIR *dir)
{
    struct dirent *ret;

    if (fd_is_remote(dirfd(dir))) {
        data_fd_t *data_fd = fd_get_data_fd(dirfd(dir));

        if (!data_fd->readdir_p) {
            data_fd->readdir_p = data_fd->fe->down;
        } else {
            data_fd->readdir_p = data_fd->readdir_p->next;
        }
        if (!data_fd->readdir_p) {
            // dbgd("readdir(%s) NULL", fd_get_dbg_name(dirfd(dir)));
            return NULL;
        }

        data_fd->dirent.d_type = data_fd->readdir_p->size ? DT_REG : DT_DIR;
        strncpy(data_fd->dirent.d_name, data_fd->readdir_p->name, 256);
        // dbgd("readdir(%s) %s", fd_get_dbg_name(dirfd(dir)), data_fd->dirent.d_name);
        return &data_fd->dirent;
    } else {
        ret = orig_readdir(dir);
        if (ret) {
            dbgo("readdir(%d) %s", dirfd(dir), ret->d_name);
        } else {
            dbgo("readdir(%d) NULL", dirfd(dir));
        }
    }
    return ret;
}

void rewind(FILE *stream)
{
    if (fd_is_remote(fileno(stream))) {
        dbgd("rewind(%d(%s))", fileno(stream), fd_get_dbg_name(fileno(stream)));
        data_fd_t *data_fd = fd_get_data_fd(fileno(stream));
        data_fd->off = 0;
#ifndef __GLIBC__
        data_fd->file.flags &= ~32; /* clear F_ERR */
        data_fd->file.wpos = data_fd->file.wbase = data_fd->file.wend = 0;
        data_fd->file.rpos = data_fd->file.rend = 0;
#endif
    } else {
        orig_rewind(stream);
    }
}

DIR *opendir(const char *name)
{
    DIR *ret;
    char *tpath;

    if (is_remote_file(name, &tpath)) {
        int fd;

        fd = fd_add(tpath);
        ret = &(fd_get_data_fd(fd)->dir);
        dbgd("opendir(%s) %d", tpath, fd);
    } else {
        ret = orig_opendir(name);
        dbgo("opendir(%s) %d", name, dirfd(ret));
    }
    return ret;
}

DIR *fdopendir(int fd)
{
    DIR *ret;

    if (fd_is_remote(fd)) {
        ret = &(fd_get_data_fd(fd)->dir);
        dbgd("fdopendir(%d(%s))", fd, fd_get_data_fd(fd)->tpath);
    } else {
        ret = orig_fdopendir(fd);
        dbgo("fdopendir(%d)", fd);
    }
    return ret;
}

int closedir(DIR *dirp)
{
    dbgd("closedir");
    if (fd_is_remote(dirfd(dirp))) {
        dbgd("closedir(%d(%s))", dirfd(dirp), fd_get_dbg_name(dirfd(dirp)));
        fd_rm(dirfd(dirp), true);
        return 0;
    } else {
        dbgo("closedir(%d)", dirfd(dirp));
        return orig_closedir(dirp);
    }
}

int fgetc(FILE *stream)
{
    int ret;

    if (fd_is_remote(fileno(stream))) {
        data_fd_t *data_fd = fd_get_data_fd(fileno(stream));
        char c;

        if (read_data(&c, 1, data_fd) != 1) {
            ret = EOF;
        } else {
            ret = c;
        }
        dbgd("fgetc(%d(%s)) %d", fileno(stream), fd_get_dbg_name(fileno(stream)), ret);
    } else {
        ret = orig_fgetc(stream);
        dbgo("fgetc(%d(%s)) %d", fileno(stream), fd_get_dbg_name(fileno(stream)), ret);
    }
    return ret;
}

/* Google storage static data interface */
#ifndef NO_GOOGLE_LINK
    #include <google/cloud/storage/client.h>
namespace gcs = ::google::cloud::storage;
static gcs::Client client;
static char *bucket_name;
#endif

static int init_gs()
{
#ifndef NO_GOOGLE_LINK
    bucket_name = getenv("AFC_AEP_GS_BUCKET_NAME");
    client = gcs::Client();
#endif
    return 0;
}

static int download_file_gs(data_fd_t *data_fd, char *dest)
{
#ifndef NO_GOOGLE_LINK
    int output;

    if ((output = orig_open(dest, O_CREAT | O_WRONLY)) < 0) {
        return -1;
    }

    google::cloud::Status status = client.DownloadToFile(bucket_name, data_fd->tpath, dest);
    if (!status.ok()) {
        return -1;
    }

    orig_close(output);
#endif
    return 0;
}

static ssize_t read_remote_data_gs(void *destv, size_t size, char *tpath, off_t off)
{
#ifndef NO_GOOGLE_LINK
    // ObjectReadStream is std::basic_istream<char>
    gcs::ObjectReadStream stream = client.ReadObject(bucket_name, tpath, gcs::ReadRange(off, off + size));
    stream.read((char *)destv, size);
    return stream.tellg();
#else
    return 0;
#endif
}

/* copy file from nfs mount to local */
static int download_file_nfs(data_fd_t *data_fd, char *dest)
{
    int input, output, res;
    off_t copied = 0;
    char realpath[AEP_PATH_MAX];
    struct timeval tv;
    unsigned int us;

    strncpy(realpath, real_mountpoint, AEP_PATH_MAX);
    strncat(realpath, data_fd->tpath, AEP_PATH_MAX - strlen(realpath));

    starttime(&tv);
    if ((output = orig_open(dest, O_CREAT | O_RDWR)) < 0) {
        return -1;
    }

    if ((input = orig_open(realpath, O_RDONLY)) < 0) {
        return -1;
    }
    res = sendfile(output, input, &copied, data_fd->fe->size);

    orig_close(input);

    fsync(output);
    orig_close(output);
    us = stoptime(&tv);

    dbgl("cache file %s size %zu time %u us", realpath, data_fd->fe->size, us);
    aepst.read_write++;
    aepst.read_write_size += data_fd->fe->size;
    aepst.read_write_time += us;
    return res == (int)data_fd->fe->size ? 0 : -1;
}

static ssize_t read_remote_data_nfs(void *destv, size_t size, char *tpath, off_t off)
{
    int fd;
    ssize_t ret;
    char path[AEP_PATH_MAX];
    struct timeval tv;
    unsigned int us;

    strncpy(path, real_mountpoint, AEP_PATH_MAX);
    strncat(path, tpath, AEP_PATH_MAX - strlen(path));
    starttime(&tv);
    fd = orig_open(path, O_RDONLY);
    aep_assert(fd >= 0, "read_data_fs(%s) open", path);
    orig_lseek(fd, off, SEEK_SET);
    ret = orig_read(fd, destv, size);
    orig_close(fd);
    us = stoptime(&tv);
    dbgd("read_remote_data(%s, %zu) %zu", path, size, ret);
    dbgl("read remote file %s size %zu time %u us", path, size, us);
    aepst.read_remote++;
    aepst.read_remote_size += size;
    aepst.read_remote_time += us;
    return ret;
}
