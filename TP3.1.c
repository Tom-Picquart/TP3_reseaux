#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "tosfs.h"

int main(){
    char *addr;
	int fd;
	struct stat sb;

    if ((fd = open("./test_tosfs_files",O_RDWR))==-1){
		perror("open");
		exit(1);}
    
    if (stat("./test_tosfs_files", &sb) == -1) {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    addr = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE,MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED){
        perror("mmap");
        close(fd);
        exit(EXIT_FAILURE);}
        

    struct tosfs_superblock *superblock = (struct tosfs_superblock *)addr;
    struct tosfs_inode *inodes =(struct tosfs_inode *)((char *)addr + TOSFS_BLOCK_SIZE);

 
    printf("\n=== Superbloc ===\n");
    printf("Magic number : 0x%08x\n", superblock->magic);
    printf("Block bitmap : 0x%08x\n", superblock->block_bitmap);
    printf("Inode bitmap : 0x%08x\n", superblock->inode_bitmap);
    printf("Block size   : %u bytes\n", superblock->block_size);
    printf("Number of blocks : %u\n", superblock->blocks);
    printf("Number of inodes : %u\n", superblock->inodes);
    printf("Root inode   : %u\n", superblock->root_inode);

    printf("\n=== Inodes ===\n");

    for (int i = 0; i < 32; i++) {
        struct tosfs_inode *inode = &inodes[i];

        if (inode->inode != 0) {
            printf("\nInode %u\n", inode->inode);
            printf("  Block number : %u\n", inode->block_no);
            printf("  UID          : %u\n", inode->uid);
            printf("  GID          : %u\n", inode->gid);
            printf("  Mode         : 0x%04x\n", inode->mode);
            printf("  Permissions  : 0x%04x\n", inode->perm);
            printf("  Size         : %u bytes\n", inode->size);
            printf("  Link count   : %u\n", inode->nlink);
        }
    }

	munmap(addr, sb.st_size);

    return 0;
}