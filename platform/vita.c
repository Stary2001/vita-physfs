/*
 * PS Vita support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon and Ezekiel Bethel.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_VITA

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
// #include <dirent.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <errno.h>
#include <fcntl.h>

#include "physfs_internal.h"

#define MAXPATHLEN 1024

int __PHYSFS_platformInit(void)
{
    return(1);  /* always succeed. */
} /* __PHYSFS_platformInit */


int __PHYSFS_platformDeinit(void)
{
    return(1);  /* always succeed. */
} /* __PHYSFS_platformDeinit */

const char *__PHYSFS_platformDirSeparator = "/";

char *__PHYSFS_platformCopyEnvironmentVariable(const char *varname)
{
    const char *envr = getenv(varname);
    char *retval = NULL;

    if (envr != NULL)
    {
        retval = (char *) allocator.Malloc(strlen(envr) + 1);
        if (retval != NULL)
            strcpy(retval, envr);
    } /* if */

    return(retval);
} /* __PHYSFS_platformCopyEnvironmentVariable */


static char *getUserNameByUID(void)
{
    return(NULL);
} /* getUserNameByUID */


static char *getUserDirByUID(void)
{
    return(NULL);
} /* getUserDirByUID */


char *__PHYSFS_platformGetUserName(void)
{
    char *retval = getUserNameByUID();
    if (retval == NULL)
        retval = __PHYSFS_platformCopyEnvironmentVariable("USER");
    return(retval);
} /* __PHYSFS_platformGetUserName */


char *__PHYSFS_platformGetUserDir(void)
{
    char *retval = __PHYSFS_platformCopyEnvironmentVariable("HOME");

    /* if the environment variable was set, make sure it's really a dir. */
    if (retval != NULL)
    {
        struct stat statbuf;
        if ((stat(retval, &statbuf) == -1) || (S_ISDIR(statbuf.st_mode) == 0))
        {
            allocator.Free(retval);
            retval = NULL;
        } /* if */
    } /* if */

    if (retval == NULL)
        retval = getUserDirByUID();

    return(retval);
} /* __PHYSFS_platformGetUserDir */


int __PHYSFS_platformExists(const char *fname)
{
    struct SceIoStat statbuf;
    BAIL_IF_MACRO(sceIoGetstat(fname, &statbuf) < 0, "stat failed", 0);
    return(1);
} /* __PHYSFS_platformExists */


int __PHYSFS_platformIsSymLink(const char *fname)
{
    struct SceIoStat statbuf;
    BAIL_IF_MACRO(sceIoGetstat(fname, &statbuf) < 0, "stat failed", 0);
    return( (PSP2_S_ISLNK(statbuf.st_mode)) ? 1 : 0 );
} /* __PHYSFS_platformIsSymlink */


int __PHYSFS_platformIsDirectory(const char *fname)
{
    struct SceIoStat statbuf;
    BAIL_IF_MACRO(sceIoGetstat(fname, &statbuf) < 0, "stat failed", 0);
    return( (PSP2_S_ISDIR(statbuf.st_mode)) ? 1 : 0 );
} /* __PHYSFS_platformIsDirectory */


char *__PHYSFS_platformCvtToDependent(const char *prepend,
                                      const char *dirName,
                                      const char *append)
{
    int len = ((prepend) ? strlen(prepend) : 0) +
              ((append) ? strlen(append) : 0) +
              strlen(dirName) + 1;
    char *retval = (char *) allocator.Malloc(len);

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    /* platform-independent notation is Unix-style already.  :)  */

    if (prepend)
        strcpy(retval, prepend);
    else
        retval[0] = '\0';

    strcat(retval, dirName);

    if (append)
        strcat(retval, append);

    return(retval);
} /* __PHYSFS_platformCvtToDependent */



void __PHYSFS_platformEnumerateFiles(const char *dirname,
                                     int omitSymLinks,
                                     PHYSFS_EnumFilesCallback callback,
                                     const char *origdir,
                                     void *callbackdata)
{
    SceUID dir;
    struct SceIoDirent ent;
    int bufsize = 0;
    char *buf = NULL;
    int dlen = 0;

    if (omitSymLinks)  /* !!! FIXME: this malloc sucks. */
    {
        dlen = strlen(dirname);
        bufsize = dlen + 256;
        buf = (char *) allocator.Malloc(bufsize);
        if (buf == NULL)
            return;
        strcpy(buf, dirname);
        if (buf[dlen - 1] != '/')
        {
            buf[dlen++] = '/';
            buf[dlen] = '\0';
        } /* if */
    } /* if */

    errno = 0;
    dir = sceIoDopen(dirname);
    if (dir < 0)
    {
        allocator.Free(buf);
        return;
    } /* if */

    while (sceIoDread(dir, &ent))
    {
        if (strcmp(ent.d_name, ".") == 0)
            continue;

        if (strcmp(ent.d_name, "..") == 0)
            continue;

        if (omitSymLinks)
        {
            char *p;
            int len = strlen(ent.d_name) + dlen + 1;
            if (len > bufsize)
            {
                p = (char *) allocator.Realloc(buf, len);
                if (p == NULL)
                    continue;
                buf = p;
                bufsize = len;
            } /* if */

            strcpy(buf + dlen, ent.d_name);
            if (__PHYSFS_platformIsSymLink(buf))
                continue;
        } /* if */

        callback(callbackdata, origdir, ent.d_name);
    } /* while */

    allocator.Free(buf);
    sceIoDclose(dir);
} /* __PHYSFS_platformEnumerateFiles */


int __PHYSFS_platformMkDir(const char *path)
{
    int rc;
    errno = 0;
    rc = sceIoMkdir(path, S_IRWXU);
    BAIL_IF_MACRO(rc < 0, "mkdir failed", 0);
    return(1);
} /* __PHYSFS_platformMkDir */


static void *doOpen(const char *filename, int mode)
{
    const int appending = (mode & O_APPEND);
    int fd;
    int *retval;
    errno = 0;

    /* O_APPEND doesn't actually behave as we'd like. */
    mode &= ~O_APPEND;

    fd = open(filename, mode, S_IRUSR | S_IWUSR);
    BAIL_IF_MACRO(fd < 0, strerror(errno), NULL);

    if (appending)
    {
        if (lseek(fd, 0, SEEK_END) < 0)
        {
            close(fd);
            BAIL_MACRO(strerror(errno), NULL);
        } /* if */
    } /* if */

    retval = (int *) allocator.Malloc(sizeof (int));
    if (retval == NULL)
    {
        close(fd);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    *retval = fd;
    return((void *) retval);
} /* doOpen */


void *__PHYSFS_platformOpenRead(const char *filename)
{
    return(doOpen(filename, O_RDONLY));
} /* __PHYSFS_platformOpenRead */


void *__PHYSFS_platformOpenWrite(const char *filename)
{
    return(doOpen(filename, O_WRONLY | O_CREAT | O_TRUNC));
} /* __PHYSFS_platformOpenWrite */


void *__PHYSFS_platformOpenAppend(const char *filename)
{
    return(doOpen(filename, O_WRONLY | O_CREAT | O_APPEND));
} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    int fd = *((int *) opaque);
    int max = size * count;
    int rc = read(fd, buffer, max);

    BAIL_IF_MACRO(rc == -1, strerror(errno), rc);
    assert(rc <= max);

    if ((rc < max) && (size > 1))
        lseek(fd, -(rc % size), SEEK_CUR); /* rollback to object boundary. */

    return(rc / size);
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    int fd = *((int *) opaque);
    int max = size * count;
    int rc = write(fd, (void *) buffer, max);

    BAIL_IF_MACRO(rc == -1, strerror(errno), rc);
    assert(rc <= max);

    if ((rc < max) && (size > 1))
        lseek(fd, -(rc % size), SEEK_CUR); /* rollback to object boundary. */

    return(rc / size);
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    int fd = *((int *) opaque);

    #ifdef PHYSFS_HAVE_LLSEEK
      unsigned long offset_high = ((pos >> 32) & 0xFFFFFFFF);
      unsigned long offset_low = (pos & 0xFFFFFFFF);
      loff_t retoffset;
      int rc = llseek(fd, offset_high, offset_low, &retoffset, SEEK_SET);
      BAIL_IF_MACRO(rc == -1, strerror(errno), 0);
    #else
      BAIL_IF_MACRO(lseek(fd, (int) pos, SEEK_SET) == -1, strerror(errno), 0);
    #endif

    return(1);
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    int fd = *((int *) opaque);
    PHYSFS_sint64 retval;

    #ifdef PHYSFS_HAVE_LLSEEK
      loff_t retoffset;
      int rc = llseek(fd, 0, &retoffset, SEEK_CUR);
      BAIL_IF_MACRO(rc == -1, strerror(errno), -1);
      retval = (PHYSFS_sint64) retoffset;
    #else
      retval = (PHYSFS_sint64) lseek(fd, 0, SEEK_CUR);
      BAIL_IF_MACRO(retval == -1, strerror(errno), -1);
    #endif

    return(retval);
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    int fd = *((int *) opaque);
    struct stat statbuf;
    BAIL_IF_MACRO(fstat(fd, &statbuf) == -1, strerror(errno), -1);
    return((PHYSFS_sint64) statbuf.st_size);
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformEOF(void *opaque)
{
    PHYSFS_sint64 pos = __PHYSFS_platformTell(opaque);
    PHYSFS_sint64 len = __PHYSFS_platformFileLength(opaque);
    return((pos < 0) || (len < 0) || (pos >= len));
} /* __PHYSFS_platformEOF */


int __PHYSFS_platformFlush(void *opaque)
{
    BAIL_IF_MACRO(sceIoSync("cache0:", 0) < -1, "sync failed", 0);
    return(1);
} /* __PHYSFS_platformFlush */


int __PHYSFS_platformClose(void *opaque)
{
    int fd = *((int *) opaque);
    BAIL_IF_MACRO(close(fd) == -1, strerror(errno), 0);
    allocator.Free(opaque);
    return(1);
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    BAIL_IF_MACRO(remove(path) == -1, strerror(errno), 0);
    return(1);
} /* __PHYSFS_platformDelete */


PHYSFS_sint64 __PHYSFS_platformGetLastModTime(const char *fname)
{
    struct SceIoStat statbuf;
    BAIL_IF_MACRO(sceIoGetstat(fname, &statbuf) < 0, "stat failed", -1);
    PHYSFS_sint64 t = 0;
    t += (PHYSFS_sint64)statbuf.st_mtime.year * 365 * 24 * 60 * 60;
    t += (PHYSFS_sint64)statbuf.st_mtime.month * 30 * 24 * 60 * 60;
    t += (PHYSFS_sint64)statbuf.st_mtime.day * 24 * 60 * 60;
    t += (PHYSFS_sint64)statbuf.st_mtime.hour * 60 * 60;
    t += (PHYSFS_sint64)statbuf.st_mtime.minute * 60;
    t += (PHYSFS_sint64)statbuf.st_mtime.second;

    return t;
} /* __PHYSFS_platformGetLastModTime */

#ifdef PHYSFS_NO_THREAD_SUPPORT
void *__PHYSFS_platformGetThreadID(void) { return((void *) 0x0001); }
void *__PHYSFS_platformCreateMutex(void) { return((void *) 0x0001); }
void __PHYSFS_platformDestroyMutex(void *mutex) {}
int __PHYSFS_platformGrabMutex(void *mutex) { return(1); }
void __PHYSFS_platformReleaseMutex(void *mutex) {}
#endif

#ifdef PHYSFS_NO_CDROM_SUPPORT

/* Stub version for platforms without CD-ROM support. */
void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
} /* __PHYSFS_platformDetectAvailableCDs */

#endif

char *__PHYSFS_platformRealPath(const char *path)
{
    char *ret = allocator.Malloc(strlen(path)+1);
    if (ret != NULL)
        strcpy(ret, path);
    return(ret);
} /* __PHYSFS_platformRealPath */

char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    char *ret = allocator.Malloc(37);
    if (ret != NULL)
        strcpy(ret, "cache0:/VitaDefilerClient/Documents/");
    return(ret);
}

int __PHYSFS_platformSetDefaultAllocator(PHYSFS_Allocator *a)
{
    return(0);  /* just use malloc() and friends. */
} /* __PHYSFS_platformSetDefaultAllocator */

#endif  /* PHYSFS_PLATFORM_VITA */

/* end of vita.c ... */

