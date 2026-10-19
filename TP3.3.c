/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello_ll.c `pkg-config fuse --cflags --libs` -o hello_ll
*/

#define FUSE_USE_VERSION 26
#include <sys/mman.h>
#include <sys/stat.h>
#include <fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "tosfs.h"


//defining global variables so that only one map is needed
static int fs_fd = -1;
static size_t fs_size;
static void *fs_addr = MAP_FAILED;
static struct tosfs_superblock *fs_superblock;
static struct tosfs_inode *fs_inodes;


static struct tosfs_inode *tosfs_get_inode(fuse_ino_t ino)
{
    for (unsigned int i = 0; i < fs_superblock->inodes; i++) {
        if (fs_inodes[i].inode == ino) {
            return &fs_inodes[i];
        }
    }

    return NULL;
}


static int tosfs_load(const char *path)
{
    struct stat st;

    fs_fd = open(path, O_RDWR);

    if (fs_fd == -1) {
        perror("open");
        return -1;
    }

    if (fstat(fs_fd, &st) == -1) {
        perror("fstat");
        close(fs_fd);
        fs_fd = -1;
        return -1;
    }

    fs_size = (size_t)st.st_size;

    fs_addr = mmap(NULL, fs_size,PROT_READ | PROT_WRITE,MAP_SHARED, fs_fd, 0);

    if (fs_addr == MAP_FAILED) {
        perror("mmap");
        close(fs_fd);
        fs_fd = -1;
        return -1;
    }

    fs_superblock = (struct tosfs_superblock *)fs_addr;

    fs_inodes = (struct tosfs_inode *)((char *)fs_addr + TOSFS_INODE_BLOCK * TOSFS_BLOCK_SIZE);

    printf("TOSFS loaded\n");

    return 0;
}


static int tosfs_stat(fuse_ino_t ino, struct stat *stbuf)
{   
    struct tosfs_inode *inode;
    inode = tosfs_get_inode(ino);

    if (inode == NULL) {
        return -1;
    }
    memset(stbuf, 0, sizeof(*stbuf));

    stbuf->st_ino = inode->inode;
    stbuf->st_uid = inode->uid;
    stbuf->st_gid = inode->gid;
    stbuf->st_size = inode->size;
    stbuf->st_nlink = inode->nlink;
    stbuf->st_mode = inode->mode;
    stbuf->st_blksize = TOSFS_BLOCK_SIZE;
    //number of 4096 Bytes blocs used (+4095 to get the upper rounding)
    stbuf->st_blocks =(inode->size + 4095) / 4096;
    
    return 0;
}


static void tosfs_ll_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    struct stat stbuf;
    (void) fi;

    memset(&stbuf, 0, sizeof(stbuf));

    if (tosfs_stat(ino, &stbuf) == -1) {
        fuse_reply_err(req, ENOENT);
    } 
    else {
        fuse_reply_attr(req, &stbuf, 1.0);
    }
}


static struct fuse_lowlevel_ops tosfs_oper = {
    .getattr = tosfs_ll_getattr,
};

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_chan *ch;
	char *mountpoint;
	int err = -1;
    if (tosfs_load("./test_tosfs_files") == -1) {
        fuse_opt_free_args(&args);
        return EXIT_FAILURE;
    }

	if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 && (ch = fuse_mount(mountpoint, &args)) != NULL) {
		struct fuse_session *se;

		se = fuse_lowlevel_new(&args, &tosfs_oper,sizeof(tosfs_oper), NULL);

		if (se != NULL) {
			if (fuse_set_signal_handlers(se) != -1) {
				fuse_session_add_chan(se, ch);
				err = fuse_session_loop(se);
				fuse_remove_signal_handlers(se);
				fuse_session_remove_chan(ch);
			}
			fuse_session_destroy(se);
		}
		fuse_unmount(mountpoint, ch);
	}
	fuse_opt_free_args(&args);

    if (fs_addr != MAP_FAILED)
        munmap(fs_addr, fs_size);

    if (fs_fd != -1)
        close(fs_fd);
        
    free(mountpoint);

	return err ? 1 : 0;
}
