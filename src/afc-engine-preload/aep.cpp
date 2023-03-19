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

/* max number of simultaneously open files */
#define AEP_NO_FILES 30

#define AEP_PATH_MAX PATH_MAX
#define AEP_FILENAME_MAX FILENAME_MAX

#define aep_assert(cond, format, ...) \
	if (!(cond)) { \
		fprintf(stderr, format " Abort!\n", ##__VA_ARGS__); \
		if (aep_debug) { \
			dprintf(logfile, format " Abort!\n", ##__VA_ARGS__); \
		} \
		abort(); \
	}
#define dbg(format, ...) \
	if (aep_debug & 2) { \
		dprintf(logfile, "%d: " format "\n", getpid(), ##__VA_ARGS__); \
	}
#define dbge(format, ...) \
	fprintf(stderr, format " Error!\n", ##__VA_ARGS__); \
	if (aep_debug) { \
		dprintf(logfile, format " Error!\n", ##__VA_ARGS__); \
	}
#define dbgd(format, ...) \
	if (aep_debug & 8) { \
		dprintf(logfile, "data " format "\n", ##__VA_ARGS__); \
	}
#define dbgo(format, ...) \
	if (aep_debug & 4) { \
		dprintf(logfile, "orig " format "\n", ##__VA_ARGS__); \
	}
#define dbgl(format, ...) \
	if (aep_debug & 1) { \
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
					       | S_IFREG | S_IRUSR | S_IRGRP | S_IROTH - file */
			       .st_uid = 0x4466,
			       .st_gid = 0x592,
			       .st_rdev = 0,
			       .st_size = 0,
			       .st_blksize = 0x80000,
			       .st_blocks = 0, /* st_size / 512 rounded up */
			       .st_atim = {0x63b45b04, 0},
			       .st_mtim = {0x63b45b04, 0},
			       .st_ctim = {0x63b45b04, 0}};

static struct statx def_statx = {
	.stx_mask = 0x17ff,
	.stx_blksize = 0x80000,
	.stx_attributes = 0,
	.stx_nlink = 1,
	.stx_uid = 0x4466,
	.stx_gid = 0x592,
	.stx_mode = 0, /*  S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO - directory,
			| S_IFREG | S_IRUSR | S_IRGRP | S_IROTH - file */
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
		uint64_t size;
} fe_t;

typedef struct {
		fe_t *fe;
		FILE file;
		DIR dir;
		off_t off;
		char tpath[AEP_PATH_MAX];
		struct dirent dirent;
		fe_t *readdir_p;
} data_fd_t;

static fe_t tree = {}; /* the root "/" entry of file tree */
static data_fd_t data_fds[AEP_NO_FILES] = {};

/* Dynamically allocated buffers. Sometime in the future I'll free them */
static uint8_t *filelist; /* row file tree buffer */
static fe_t *fes; /* file tree */

static aep_statistic_t aepst = {};

/* configuration from getenv() */
static char *cache_path;
static uint64_t max_cached_file_size;
static uint64_t max_cached_size;
static uint32_t aep_debug = 0;
static char *ae_mountpoint; /* The path in which afc-engine seeking for static data */
static char *real_mountpoint; /* The path in which static data really is */
static bool aep_use_gs = false;
static int logfile = -1;
static volatile int64_t *cache_size;
static sem_t *cache_size_sem;
#if 0
static uint64_t cache_size_add;
#endif
static const struct timespec sem_timeout = {.tv_sec = 30, .tv_nsec = 0,};
#define sem_wait(s, i) if (sem_timedwait(s, &sem_timeout)) {dbg("sem_timedwait error %s", i);}

static data_fd_t *fd_get_data_fd(int fd);
static char *fd_get_name(int fd);
static void fd_rm(int fd);
static int download_file_nfs(data_fd_t *data_fd, char *dest);
static ssize_t read_remote_data_nfs(void *destv, size_t size, char *tpath, off_t off);
static int download_file_gs(data_fd_t *data_fd, char *dest);
static ssize_t read_remote_data_gs(void *destv, size_t size, char *tpath, off_t off);
static int init_gs();

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

static inline FILE *orig_fopen(const char *path, const char *mode)
{
	typedef FILE *(*orig_fopen_t)(const char *, const char *);
	orig_fopen_t orig = (orig_fopen_t)dlsym(RTLD_NEXT, "fopen");
	FILE *ret;

	ret = (*orig)(path, mode);
	if (aep_debug && ret) {
		strcpy(data_fds[fileno(ret)].tpath, path);
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
	if (aep_debug && fd >= 0) {
		strcpy(data_fds[fd].tpath, pathname);
	}
	return fd;
}

static inline int orig_openat(int dirfd, const char *pathname, int flags, ...)
{
	typedef int (*orig_openat_t)(int, const char *, int);
	orig_openat_t orig = (orig_openat_t)dlsym(RTLD_NEXT, "openat");
	int fd;

	fd = (*orig)(dirfd, pathname, flags);
	if (aep_debug && fd >= 0) {
		strcpy(data_fds[fd].tpath, pathname);
	}
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
	dbgo("orig_read(%d, %zu) %zd", fd, count, ret);
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
	if (aep_debug && ret) {
		strcpy(data_fds[dirfd(ret)].tpath, name);
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
	dbgo("FILE->read(%d(%s), %zu)", fileno(f), fd_get_name(fileno(f)), size);
	return 0;
}
/* never used */
static size_t f_write(FILE *f, const unsigned char *buff, size_t size)
{
	dbgo("FILE->write(%d(%s), %zu)", fileno(f), fd_get_name(fileno(f)), size);
	f->wpos = 0;
	return 0;
}

static off_t f_seek(FILE *f, off_t off, int whence)
{
	if (whence == SEEK_CUR && off == 0) {
		/* ignore */
		return fd_get_data_fd(fileno(f))->off;
	}
	aep_assert(whence == SEEK_SET,
		   "f_seek(%d(%s), %ld, %d) unsupported whence",
		   fileno(f),
		   fd_get_name(fileno(f)),
		   off,
		   whence);
	dbgd("FILE->seek(%d(%s), %jd, %d)", fileno(f), fd_get_name(fileno(f)), off, whence);
	fd_get_data_fd(fileno(f))->off = off;
	return off;
}
/* never used */
static int f_close(FILE *f)
{
	dbgd("FILE->close(%d(%s))", fileno(f), fd_get_name(fileno(f)));
	fd_rm(fileno(f));
	return 0;
}
#endif

static inline void cache_size_set(int64_t size)
{
	sem_wait(cache_size_sem, "cache_size_sem");
	*cache_size += size;
	sem_post(cache_size_sem);
}

static inline int64_t cache_size_get()
{
	int64_t tmp;
	sem_wait(cache_size_sem, "cache_size_sem");
	tmp = *cache_size;
	sem_post(cache_size_sem);
	return tmp;
}

#if 0
static int ftw_remove_callback(const char *fpath, const struct stat *sb, int typeflag)
{
	if (typeflag == FTW_F && sb->st_size) {
		sem_t *sem;
		int ret;

		dbg("Remove %s size=%ld cs=%lu", (char *)fpath + strlen(cache_path), sb->st_size, *cache_size);
		sem = sem_open((char *)fpath + strlen(cache_path), O_CREAT, 0666, 1);
		aep_assert(sem, "sem_open");
		sem_wait(sem, (char *)fpath + strlen(cache_path));
		ret = unlink(fpath);
		sem_post(sem);
		if (!ret) {
			cache_size_set(-sb->st_size);
		}
		dbg("Remove %s done, cs %lu", (char *)fpath + strlen(cache_path), *cache_size);
	}
	return *cache_size + cache_size_add < max_cached_size ? -1 : 0;
}

/* remove files in the cache until the cache have <size> free space */
static void reduce_cache(uint64_t size)
{
	dbg("reduce_cache cs %lu", *cache_size);
	if (cache_size_get() + size > max_cached_size) {
		cache_size_add = size;
		ftw(cache_path, ftw_remove_callback, 100);
	}
	dbg("reduce_cache done cs %lu", *cache_size);
}
#endif

static size_t read_data(void *destv, size_t size, data_fd_t *data_fd)
{
	char fakepath[AEP_PATH_MAX];
	ssize_t ret;
	ssize_t (*read_remote_data)(void *destv, size_t size, char *tpath, off_t off) =
		aep_use_gs ? read_remote_data_gs : read_remote_data_nfs;
	/* define pointer to download file func */
	int (*download_file)(data_fd_t * data_fd, char *dest);
	download_file = aep_use_gs ? download_file_gs : download_file_nfs;
	struct stat stat;
	sem_t *sem;
	bool is_cached = false;

	strcpy(fakepath, cache_path);
	strcat(fakepath, data_fd->tpath);

	sem = sem_open(data_fd->tpath, O_CREAT, 0666, 1);
	aep_assert(sem, "sem_open");
	sem_wait(sem, data_fd->tpath);

	/* download whole file to cache if possible */
	if (!orig_stat(fakepath, &stat)) {
		
		if (data_fd->fe->size == (uint64_t)stat.st_size) {
			is_cached = true;
		}
		if (!is_cached && data_fd->fe->size <= max_cached_file_size) {
			dbg("download %s", data_fd->tpath);
			if (!download_file(data_fd, fakepath)) {
				cache_size_set(data_fd->fe->size);
				dbg("download %s done, cs %lu", data_fd->tpath, *cache_size);
				is_cached = true;
			} else {
				dbg("download %s failed, cs %lu", data_fd->tpath, *cache_size);
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
	sem_t *sem;

	fe = find_fe(tpath);
	if (!fe) {
		return -1;
	}
	strcpy(fakepath, cache_path);
	strcat(fakepath, tpath);

	/* create cache file */
	if (fe->size) {
		dbg("fd_add(%s)", tpath);
		sem = sem_open(tpath, O_CREAT, 0666, 1);
		sem_wait(sem, tpath);
	}
	if (orig_stat(fakepath, &statbuf)) {
		for (p = fakepath; *p; p++) {
			if (*p == '/') {
				*p = '\0';
				mkdir(fakepath, 0777);
				*p = '/';
			}
		}
		if (fe->size) { /* is file */
			int fd;

			fd = orig_open(fakepath, O_CREAT | O_RDWR);
			aep_assert(fd >= 0, "fd_add(%s) touch errno %d", fakepath, errno);
			orig_close(fd);
		} else { /* is dir */
			mkdir(fakepath, 0777);
		}
	}
	if (fe->size) {
		sem_post(sem);
		sem_close(sem);
		dbg("fd_add(%s) done", tpath);
	}

	fd = orig_open(fakepath, O_RDONLY);
	aep_assert(fd >= 0, "fd_add(%s) open()", tpath);
	memset(data_fds + fd, 0, sizeof(data_fd_t));
	aep_assert(!orig_fstat(fd, &statbuf), "fd_add(%s) fstat", tpath);
	data_fds[fd].fe = fe;
	data_fds[fd].off = 0;
#ifndef __GLIBC__
	data_fds[fd].file.fd = fd;
	data_fds[fd].file.read = f_read;
	data_fds[fd].file.write = f_write;
	data_fds[fd].file.seek = f_seek;
	data_fds[fd].file.close = f_close;
#endif
	data_fds[fd].readdir_p = NULL;
	data_fds[fd].dir.fd = fd;
	strcpy(data_fds[fd].tpath, tpath);
	dbgd("fd_add(%s) %d", tpath, fd);
	return fd;
}

static bool fd_is_remote(int fd)
{
	return fd >= 0 && fd < AEP_NO_FILES && data_fds[fd].fe;
}

static void fd_rm(int fd)
{
	data_fd_t *data_fd = data_fds + fd;

	if (!data_fd->fe) {
		return;
	}

	if (data_fd->fe->size && cache_size_get() > (int64_t)max_cached_size) {
		sem_t *sem;
		struct stat stat;

		sem = sem_open(data_fd->tpath + strlen(cache_path), O_CREAT, 0666, 1);
		sem_wait(sem, data_fd->tpath);
		if (!fstat(fd, &stat)) {
			if (stat.st_size) {
				ftruncate(fd, 0);
				cache_size_set(-stat.st_size);
			}
		}
		sem_post(sem);
	}
	orig_close(fd);
	data_fd->fe = NULL;
}

static inline data_fd_t *fd_get_data_fd(int fd)
{
	return data_fds + fd;
}

static inline char *fd_get_name(int fd)
{
	return data_fds[fd].tpath;
}

static bool is_remote_file(const char *path, char **tpath)
{
	if (!path) {
		*tpath = (char *)path;
		return false;
	}

	if (strncmp(path, ae_mountpoint, strlen(ae_mountpoint)) == 0) {
		*tpath = (char *)path + strlen(ae_mountpoint);
		return true;
	}
	*tpath = (char *)path;
	return false;
}

#if 0
/* for testing */
static void prn_tree(fe_t *fe, uint8_t tab)
{
	fe_t *tmp = fe;
	int i;
	char str[10] = {};
	for (i = 0; i < tab; i++) {
		str[i] = '\t';
	}
	while (tmp) {
		printf("%s%s\n", str, tmp->name);
		if (tmp->down) {
			prn_tree(tmp->down, tab + 1);
		}
		tmp = tmp->next;
	}
}
#endif

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
	real_mountpoint = getenv("AFC_AEP_REAL_MOUNTPOINT");
	aep_assert(real_mountpoint, "AFC_AEP_REAL_MOUNTPOINT env var is not defined");
	if (real_mountpoint[strlen(real_mountpoint) - 1] == '/') {
		real_mountpoint[strlen(real_mountpoint) - 1] = '\0';
	}
	ae_mountpoint = getenv("AFC_AEP_ENGINE_MOUNTPOINT");
	aep_assert(ae_mountpoint, "AFC_AEP_ENGINE_MOUNTPOINT env var is not defined");
	if (ae_mountpoint[strlen(ae_mountpoint) - 1] == '/') {
		ae_mountpoint[strlen(ae_mountpoint) - 1] = '\0';
	}
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
		uint64_t size;

		/* read next line */
		while (*fl == '\t') {
			tab++;
			(fl)++;
		}
		name = (char *)(fl);
		fl += strlen(name) + 1;

		size = *((uint64_t *)fl);
		fl += sizeof(uint64_t);
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
	cache_size_sem = sem_open("aep_shmem_sem", O_CREAT, 0666, 1);
	aep_assert(cache_size_sem, "sem_open");
	shm_fd = shm_open("aep_shmem", O_RDWR | O_CREAT | O_EXCL, 0666);
	dbg("aep_init");
	sem_wait(cache_size_sem, "cache_size_sem");
	if (shm_fd < 0) {
		/* O_CREAT | O_EXCL failed, so shared memory object already was initialized */
		shm_fd = shm_open("aep_shmem", O_RDWR, 0666);
		aep_assert(shm_fd >= 0, "shm_open");
		cache_size = (int64_t *)mmap(NULL, sizeof(int64_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
		aep_assert(cache_size, "mmap");
	} else {
		dbg("aep_init recount cache");
		ftruncate(shm_fd, sizeof(uint64_t));
		cache_size = (int64_t *)mmap(NULL, sizeof(int64_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
		*cache_size = 0;
		aep_assert(cache_size, "mmap");
		/* count existing cache size */
		ftw(cache_path, ftw_callback, 100);
	}
	sem_post(cache_size_sem);
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
		dbgd("fread(%d(%s), %zu * %zu) %zu",
		     fileno(f),
		     fd_get_name(fileno(f)),
		     size,
		     nmemb,
		     ret);
	} else {
		ret = orig_fread(destv, size, nmemb, f);
		dbgo("fread(%d, %zu * %zu) %zu", fileno(f), size, nmemb, ret);
	}
	return ret;
}

int fclose(FILE *f)
{
	int ret;

	if (fd_is_remote(fileno(f))) {
		dbgd("fclose(%d(%s))", fileno(f), fd_get_name(fileno(f)));
		fd_rm(fileno(f));
		ret = 0;
		dbgl("statistics: remote %u/%u/%u cached %u/%u/%u cache %u/%u/%u cache size %lu",
		     aepst.read_remote,
		     aepst.read_remote_size,
		     aepst.read_remote_time,
		     aepst.read_cached,
		     aepst.read_cached_size,
		     aepst.read_cached_time,
		     aepst.read_write,
		     aepst.read_write_size,
		     aepst.read_write_time,
		     *cache_size);
	} else {
		dbgo("fclose(%d)", fileno(f));
		ret = orig_fclose(f);
	}
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
		dbgd("close(%d(%s))", fd, fd_get_name(fd));
		fd_rm(fd);
	} else {
		ret = orig_close(fd);
		dbgo("close(%d(%s))=%d", fd, fd_get_name(fd), ret);
	}
	return ret;
}

int stat(const char *pathname, struct stat *statbuf)
{
	char *tpath;
	int ret;

	if (is_remote_file(pathname, &tpath)) {
		fe_t *fe;

		fe = find_fe(tpath);
		if (!fe) {
			return -1;
		}

		memcpy(statbuf, &def_stat, sizeof(struct stat));
		if (fe->size) {
			statbuf->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
		} else {
			statbuf->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
		}
		statbuf->st_size = fe->size;
		statbuf->st_blocks = (statbuf->st_size >> 9) + (statbuf->st_size & 0x1ff) ? 1 : 0;
		ret = 0;
		dbgd("stat(%s, 0x%lx, %s) %d", tpath, fe->size, fe->size ? "file" : "dir", ret);
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

		dbgd("fstat(%d(%s))", fd, fd_get_name(fd));

		memcpy(statbuf, &def_stat, sizeof(struct stat));
		if (data_fd->fe->size) {
			statbuf->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
		} else {
			statbuf->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
		}
		statbuf->st_size = data_fd->fe->size;
		statbuf->st_blocks = (statbuf->st_size >> 9) + (statbuf->st_size & 0x1ff) ? 1 : 0;
		ret = 0;
		dbgd("fstat(%s, 0x%lx, %s) %d",
		     fd_get_name(fd),
		     data_fd->fe->size,
		     data_fd->fe->size ? "file" : "dir",
		     ret);
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
		fe_t *fe;

		fe = find_fe(tpath);
		if (!fe) {
			return -1;
		}

		memcpy(statbuf, &def_stat, sizeof(struct stat));
		if (fe->size) {
			statbuf->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
		} else {
			statbuf->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
		}
		statbuf->st_size = fe->size;
		statbuf->st_blocks = (statbuf->st_size >> 9) + (statbuf->st_size & 0x1ff) ? 1 : 0;
		ret = 0;
		dbgd("lstat(%s, 0x%lx, %s) %d", tpath, fe->size, fe->size ? "file" : "dir", ret);
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
				return -1;
			}

			dbgd("SYS_statx(%s, 0x%lx, %s) 0",
			     tpath,
			     fe->size,
			     fe->size ? "file" : "dir");
			memcpy(st, &def_statx, sizeof(struct statx));
			if (fe->size) {
				st->stx_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
				st->stx_size = fe->size;
				st->stx_blocks = (fe->size >> 9) + (fe->size & 0x1ff) ? 1 : 0;
			} else {
				st->stx_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
			}
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
		dbgd("fcntl(%s, %d)", fd_get_name(fd), cmd);
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
		dbgd("read(%d(%s), %zu) %zd", fd, fd_get_name(fd), count, ret);
	} else {
		ret = orig_read(fd, buf, count);
		dbgo("read(%d(%s), %zu) %zd", fd, fd_get_name(fd), count, ret);
	}
	return ret;
}

off_t lseek(int fd, off_t offset, int whence)
{
	off_t ret;

	if (fd_is_remote(fd)) {
		data_fd_t *data_fd = fd_get_data_fd(fd);

		aep_assert(whence == SEEK_SET,
			   "lseek(%s, %ld, %d) unsupported whence",
			   fd_get_name(fd),
			   offset,
			   whence);
		data_fd->off = offset;
		ret = 0;
		dbgd("lseek(%d(%s), %ld, %d) %ld", fd, fd_get_name(fd), offset, whence, ret);
	} else {
		ret = orig_lseek(fd, offset, whence);
		dbgo("lseek(%d(%s), %ld, %d) %ld", fd, fd_get_name(fd), offset, whence, ret);
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
			dbgd("readdir(%s) NULL", fd_get_name(dirfd(dir)));
			return NULL;
		}

		data_fd->dirent.d_type = data_fd->readdir_p->size ? DT_REG : DT_DIR;
		strcpy(data_fd->dirent.d_name, data_fd->readdir_p->name);
		dbgd("readdir(%s) %s", fd_get_name(dirfd(dir)), data_fd->dirent.d_name);
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
		dbgd("rewind(%d(%s))", fileno(stream), fd_get_name(fileno(stream)));
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
	if (fd_is_remote(dirfd(dirp))) {
		dbgd("closedir(%d(%s))", dirfd(dirp), fd_get_name(dirfd(dirp)));
		fd_rm(dirfd(dirp));
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
		dbgd("fgetc(%d(%s)) %d", fileno(stream), fd_get_name(fileno(stream)), ret);
	} else {
		ret = orig_fgetc(stream);
		dbgo("fgetc(%d(%s)) %d", fileno(stream), fd_get_name(fileno(stream)), ret);
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
	if (!status.ok())
		return -1;

	orig_close(output);
#endif
	return 0;
}

static ssize_t read_remote_data_gs(void *destv, size_t size, char *tpath, off_t off)
{
#ifndef NO_GOOGLE_LINK
	// ObjectReadStream is std::basic_istream<char>
	gcs::ObjectReadStream stream = client.ReadObject(bucket_name,
							 tpath,
							 gcs::ReadRange(off, off + size));
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

	strcpy(realpath, real_mountpoint);
	strcat(realpath, data_fd->tpath);

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

	strcpy(path, real_mountpoint);
	strcat(path, tpath);
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

#if 0 /* unused overwrites */
FILE *freopen(const char *filename, const char *mode, FILE *stream)
{
	dbg("UNSUPPORTED freopen");
	return NULL;
}

int fflush(FILE *stream)
{
	if (fd_is_remote(fileno(stream))) {
		dbg("UNSUPPORTED fflush(%d)", fileno(stream));
		return EOF;
	} else {
		int (*orig)(FILE* stream) = dlsym(RTLD_NEXT, "fflush");

		return (*orig)(stream);
	}
}

void setbuf(FILE *stream, char *buf)
{
	dbg("UNSUPPORTED setbuf");
}

int setvbuf(FILE *stream, char *buf, int mode, size_t size)
{
	dbg("UNSUPPORTED setvbuf");
	return -1;
}

int fgetpos(FILE *stream, fpos_t *pos)
{
	dbg("UNSUPPORTED fgetpos");
	return -1;
}

int fsetpos(FILE *stream, const fpos_t *pos)
{
	dbg("UNSUPPORTED fsetpos");
	return -1;
}

void clearerr(FILE *stream)
{
	dbg("UNSUPPORTED clearerr");
}

int ferror(FILE *stream)
{
	dbg("UNSUPPORTED ferror");
	return 0;
}

char *fgets(char *s, int n, FILE *stream)
{
	dbg("UNSUPPORTED fgets");
	return NULL;
}

int getc(FILE *stream)
{
	dbg("UNSUPPORTED getc");
	return EOF;
}

long int ftell(FILE *stream)
{
	dbg("UNSUPPORTED ftell");
	return -1;
}

int fseek(FILE *stream, long offset, int whence)
{
	int ret;

	if (fd_is_remote(fileno(stream))) {
		data_fd_t *data_fd = fd_get_data_fd(fileno(stream));

		aep_assert(whence == SEEK_SET, "fseek(%d(%s), %ld, %d) unsupported whence", fileno(stream), fd_get_name(fileno(stream)), offset, whence);
		data_fd->off = offset;
		ret = 0;
		dbg("fseek(%d(%s), %ld, %d) %d", fileno(stream), fd_get_name(fileno(stream)), offset, whence, ret);
	} else {
		int (*orig)(FILE*, long, int) = dlsym(RTLD_NEXT, "fseek");

		ret = (*orig)(stream, offset, whence);
		dbg("fseek(%d, %ld, %d) %d", fileno(stream), offset, whence, ret);
	}
	return ret;
}

int __fseeko(FILE *stream, off_t offset, int whence)
{
	int ret;

	if (fd_is_remote(fileno(stream))) {
		data_fd_t *data_fd = fd_get_data_fd(fileno(stream));

		aep_assert(whence == SEEK_SET, "fseeko(%d(%s), %ld, %d) unsupported whence", fileno(stream), fd_get_name(fileno(stream)), offset, whence);
		data_fd->off = offset;
		ret = 0;
		dbg("fseeko(%d(%s), %ld, %d) %d", fileno(stream), fd_get_name(fileno(stream)), offset, whence, ret);
	} else {
		int (*orig)(FILE*, off_t, int) = dlsym(RTLD_NEXT, "__fseeko");

		ret = (*orig)(stream, offset, whence);
		dbg("fseeko(%d, %ld, %d) %d", fileno(stream), offset, whence, ret);
	}
	return ret;
}

int fscanf(FILE *stream, const char *format, ...)
{
	dbg("UNSUPPORTED fscanf");
	return EOF;
}

int scanf(const char *format, ...)
{
	dbg("UNSUPPORTED scanf");
	return EOF;
}

int sscanf(const char *s, const char *format, ...)
{
	dbg("UNSUPPORTED sscanf");
	return EOF;
}
/* unused */
int fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags)
{
	int ret;
	int (*orig)(int, const char*, struct stat*, int);
	char* tpath;

	orig = dlsym(RTLD_NEXT, "fstatat");
	ret = (*orig)(dirfd, pathname, statbuf, flags);
	if (is_remote_file(pathname, &tpath)) {
		dbg("fstatat(%d, %s) %d", dirfd, pathname, ret);
	}
	return ret;
}

/* syscall(SYS_statx) in musl */
int statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx *statxbuf)
{
	int ret;
	int (*orig)(int, const char*, int, unsigned int, struct statx*);
	orig = dlsym(RTLD_NEXT, "statx");
	ret = (*orig)(dirfd, pathname, flags, mask, statxbuf);
	if (is_static(-1, pathname)) {
		dbg("statx(%d, %s) %d", dirfd, pathname, ret);
	}
	return ret;
}

/* fread in musl */
ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t ret;
	int (*orig)(int fd, const struct iovec *iov, int iovcnt);
	orig = dlsym(RTLD_NEXT, "readv");
	ret = (*orig)(fd, iov, iovcnt);
	if (is_static(fd, NULL)) {
		dbg("readv(%s, %zu * %d) %zu", fd_get_name(fd), iov->iov_len, iovcnt, ret);
	}
	return ret;
}

/* readdir() in musl */
int getdents(int fd, struct dirent *dirp, size_t count)
{
	ssize_t ret;
	int (*orig)(int fd, struct dirent *, size_t count);
	orig = dlsym(RTLD_NEXT, "getdents");
	ret = (*orig)(fd, dirp, count);
	if (is_static(fd, NULL)) {
		dbg("getdents(%s, %zu) %zu", fd_get_name(fd), count, ret);
	}
	return ret;
}

/* readdir() in musl */
size_t getdents64(int fd, void *dirp, size_t count)
{
	ssize_t ret;
	int (*orig)(int fd, void *dirp, size_t count);
	orig = dlsym(RTLD_NEXT, "getdents64");
	ret = (*orig)(fd, dirp, count);
	if (is_static(fd, NULL)) {
		dbg("getdents64(%d, %zu) %zu", fd, count, ret);
	} else {
		//dbg("non-static getdents64");
	}
	return ret;
}

#endif
