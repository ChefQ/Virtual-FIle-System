#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>
#include "ext2_utils.c"
#include <errno.h>
unsigned char *disk;
unsigned char *gd_Offest;



int main(int argc, char **argv) {
    char * path;

    if(argc != 3){
      fprintf(stderr, "Usage: readimg <image file name>\n");
      exit(1);
    }

    int fd = open(argv[1], O_RDWR); // opens the img file


    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if(disk == MAP_FAILED) {
	     perror("mmap");
	      exit(1);
    }
    path = argv[2];



    unsigned char *IN_Table = disk + (5 * EXT2_BLOCK_SIZE); // going straight to inode table



    int node = findPath(path, IN_Table);
    if(node == -1){
      fprintf(stderr,"ERROR: dd  %s\n", strerror(ENOENT) );
      exit(ENOENT);
    }



    // enter the struct of the inode
    struct ext2_inode * inode =  (struct ext2_inode *)
                            ( IN_Table + ( sizeof(struct ext2_inode)* ( node-1 ) ) );

    // this for loop iterates through data blocks from a direct pointer
    int direct_p;
    for(direct_p = 0; (inode->i_block[direct_p] != 0) && (direct_p != 13); direct_p++){

      // enter inside each block
      unsigned char * pointer =  BLOCK_OFFSET(inode->i_block[direct_p], disk + 1024);

      int counter = 0;// keeping track block size
      while (counter < EXT2_BLOCK_SIZE ){ // counter

        struct ext2_dir_entry * dir_entry =  (struct ext2_dir_entry * )pointer;


        int ret = strncmp(dir_entry->name,"",dir_entry->name_len);

        if(ret <= 0){ // if there exists a file/directory'name = to dest
          break;
        }
        printf("%.*s\n", dir_entry->name_len, dir_entry->name );

        pointer +=  dir_entry->rec_len;
        counter+=dir_entry->rec_len;
      }

    }



    return 0;
}
