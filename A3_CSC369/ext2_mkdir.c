#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>
#include <errno.h>
#include "ext2_utils.c"
#include <time.h>
unsigned char *disk;

int main(int argc, char **argv){

  char * dest;
  int split; // the index for the last occurance of '/' in argv[2]
  if(argc != 3){
    fprintf(stderr, "Usage: readimg <image file name>\n");
    exit(1);
  }

  int fd = open(argv[1], O_RDWR); // opens the img file

  disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  //arr_counter = usedInodes(disk);
  if(disk == MAP_FAILED) {
     perror("mmap");
      exit(1);
  }
  char path[strlen(argv[2])];
  for(split = strlen(argv[2])-1; split >= 0; split --){

    if (argv[2][split] == '/'){

      strncpy(path,argv[2],split+1);
      path[split+1] = '\0';


      break;


    }

    dest = argv[2];
    dest = dest + split;

  }

  printf("path: %s\n dest: %s\n",path,dest);
  unsigned char *IN_Table = disk + (5 * EXT2_BLOCK_SIZE); // going straight to inode table
  int node = findPath(path, IN_Table);

  if(node == -1){
    fprintf(stderr, "ERROR: %s\n", strerror(ENOENT));
    exit(ENOENT);
  }

  if (validDirectory( node, dest, IN_Table ) == EEXIST){
    fprintf(stderr, "ERROR: %s\n", strerror(EEXIST));
    exit(EEXIST);
  }


  struct ext2_inode * inode =  (struct ext2_inode *)
                          ( IN_Table + ( sizeof(struct ext2_inode)* ( node-1 ) ) );
  // this for loop iterates through data blocks from a direct pointer
  int direct_p;
  int dest_size = sizeof(Entry.inode) + sizeof(Entry.rec_len)
                + sizeof(Entry.name_len) + sizeof(Entry.file_type)
                + (int) strlen(dest);

  for(direct_p = 0;
    (inode->i_block[direct_p] != 0) && (direct_p != 12); direct_p++){

    unsigned char * pointer =  BLOCK_OFFSET(inode->i_block[direct_p], disk + 1024);
    int counter = 0;// keeping track block size
    int flag = 0; // the type of file of the directory entry
    while (counter < EXT2_BLOCK_SIZE ){ // counter
      unsigned char * dest_pointer = pointer; // stopped here
      struct ext2_dir_entry * dir_entry =  (struct ext2_dir_entry * )pointer;
      /*I MAY NEED TO EDIT THIS*/
      flag = dir_entry->rec_len;
      if(flag == EXT2_BLOCK_SIZE){
        break;
      }
      // this  is the variable size of a entry struct
      int std_size = sizeof(dir_entry->inode) + sizeof(dir_entry->rec_len)
                    + sizeof(dir_entry->name_len) + sizeof(dir_entry->file_type)
                    + (int) dir_entry->name_len;
      int padding = dir_entry->rec_len - std_size;

      if( std_size%4 != 0){ // standard size isn't a multiple of 4
        padding -= 4 - std_size%4;
        std_size += 4 - std_size%4; // adjusted standard size

      }


      if( (padding > 0) && (padding%4 == 0) && (dest_size <= padding)){
        dir_entry->rec_len =  std_size;
        dest_pointer +=  std_size ; //stopped here

        struct ext2_dir_entry * dest_folder =  (struct ext2_dir_entry * )dest_pointer;
        dest_folder->inode = freeInode(disk);
        dest_folder->rec_len = padding;
        dest_folder->name_len = strlen(dest);
        dest_folder->file_type = EXT2_FT_DIR;
        strncpy(dest_folder->name,dest,dest_folder->name_len);

        struct ext2_inode * dest_inode =  (struct ext2_inode *)
                                  ( IN_Table + ( sizeof(struct ext2_inode)* ( dest_folder->inode-1 ) ) );
            //Review this
        dest_inode->i_mode = EXT2_FT_DIR;
        dest_inode->i_uid = 0;
        dest_inode->i_size = 1024;
        dest_inode->i_gid = 0;
        dest_inode->i_links_count = 2;
        dest_inode->i_blocks=2;
        dest_inode->i_ctime = time(NULL); // not sure how to represent time
        //dest_inode->i_flags;
        dest_inode->osd1 = 0;
        dest_inode->i_block[0] = freeBlock(disk);
        dest_inode->i_generation=0;
        dest_inode->i_file_acl=0;
        dest_inode->i_dir_acl=0;
        dest_inode->i_faddr=0;
        //dest_inode-> extra;// should extra too be set to zero too
        //strncpy(path,argv[2],split+1);

        unsigned char * entry_pointer =  BLOCK_OFFSET(dest_inode->i_block[0], disk + 1024);
        struct  ext2_dir_entry * dest_entry =  (struct ext2_dir_entry * )entry_pointer;

        dest_entry->inode = dest_folder->inode;
        dest_entry->name_len = strlen(".");
        dest_entry->file_type = EXT2_FT_DIR;
        char * current = ".";
        strncpy(dest_entry->name,current,dest_entry->name_len);

        std_size  = sizeof(dest_folder->inode) + sizeof(dest_folder->rec_len)
                      + sizeof(dest_folder->name_len) + sizeof(dest_folder->file_type)
                      + (int) strlen(".");
        dest_entry->rec_len = std_size + (4 - std_size%4);
        entry_pointer +=  dest_entry->rec_len;

        dest_entry =  (struct ext2_dir_entry * )entry_pointer;
        dest_entry->inode = node;
        dest_entry->name_len = strlen("..");
        dest_entry->file_type = EXT2_FT_DIR;
        char * parent = "..";
        strncpy(dest_entry->name,parent,dest_entry->name_len);
        dest_entry->rec_len = 1024 - ( std_size + (4 - std_size%4) );


// case for full inode / bitmap
        return 0;
      }
      pointer +=  dir_entry->rec_len;
      counter+=dir_entry->rec_len;
      flag = dir_entry->rec_len;
    }

  }

return 0;
}
