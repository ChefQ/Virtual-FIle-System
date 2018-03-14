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

// remember to include this in ext2_utils.c
unsigned char *disk;
unsigned char *gd_Offest;
int free_inodes[32] = {};
int directories[32] = {};
int free_blocks[128] ={};




 // returns the number of inodes in the used_inodes array
int freeInodes(unsigned char * disk){
  int inode_number = 0;
  int arr_counter = 0;
  unsigned char *IB_Offset = disk + 4*EXT2_BLOCK_SIZE ;

  for(int byte = 0; byte < 4; byte++){ //there are 16 bytes in each block

    //checking if the bit is 1 or 0

    for(int count = 0; count < 8; count++){
      inode_number += 1;
      if( ( ( ( *(IB_Offset + byte) >> count) & 1) == 0) && inode_number > 11 ){

        // add used inodes that are that 11 in the used_inodes array
        free_inodes[arr_counter] = inode_number;

        arr_counter+=1;

      }

    }

  }
  if(arr_counter == 0){
    fprintf(stderr, "ERROR: %s\n", strerror(ENOSPC));
    exit(ENOSPC);
  }
  return arr_counter;
}

// Returns the next available inode the free_inodes array
int freeInode(unsigned char * disk){
  freeInodes(disk); // number of free inodes
  int inode_number = free_inodes[0];

  unsigned char *gd_Offest = disk + 2*EXT2_BLOCK_SIZE;
  struct ext2_group_desc *group_desc = (struct ext2_group_desc *) gd_Offest;
  unsigned char *inode_bitmap = disk + (1024 * group_desc->bg_inode_bitmap);

  int inode_byte = (inode_number - 1) / 8;
  int inode_position = (inode_number - 1) % 8;
  inode_bitmap[inode_byte] ^= (1 << (inode_position));
  return inode_number;
}

// returns the number of blocks in the used_inodes array
int freeBlocks(unsigned char * disk ){
  int arr_counter = 0;
  int block_number = 0;
  unsigned char *DB_Offset = disk + 3*EXT2_BLOCK_SIZE; // get offset from group descriptor

  // to increment throug the chars in the Data bitmap
  for(int byte = 0; byte < 16; byte++){ //there are 16 bytes of valid bits in each block
    //checking if the bit is 1 or 0
    for(int count = 0; count < 8; count++){
      block_number++;
       if( ( ( *(DB_Offset + byte) >> count) & 1) == 0){
         free_blocks[arr_counter] = block_number;
         arr_counter++;
       }

    }

  }
  if(arr_counter == 0){
    fprintf(stderr, "ERROR: %s\n", strerror(ENOSPC));
    exit(ENOSPC);
  }
  return arr_counter;
}

// Returns the next available block the free_blocks array
int freeBlock(unsigned char * disk){
  freeBlocks(disk); // number of free inodes
  int block_number = free_blocks[0];

  unsigned char *gd_Offest = disk + 2*EXT2_BLOCK_SIZE;
  struct ext2_group_desc *group_desc = (struct ext2_group_desc *) gd_Offest;
  unsigned char *block_bitmap = disk + (1024 * group_desc->bg_block_bitmap);

  int block_byte = (block_number - 1) / 8;
  int block_position = (block_number - 1) % 8;
  block_bitmap[block_byte] ^= (1 << (block_position));
  return block_number;
}

//returns an indirect block
int indirectBlock(unsigned char * disk, int blocks){
    int indirect_block = freeBlock(disk);
    unsigned char * pointer =  BLOCK_OFFSET(indirect_block, disk + 1024);
    int * block_number = (int *) pointer;


    for(int i = 0; (i < (blocks-12)); i++){
      *block_number = freeBlock(disk);
      block_number+=1;
    }
    return indirect_block;
}
/* returns the inode of the path
   If it fails returns a negative number
*/

int findPath(char * path, unsigned char *IN_Table){
  const char delim[2] = "/";
  char * token  = strtok(path, delim);
  int node = 2;
  int count = 0;

  if(path[0]!='/'){ // checks if the path starts from root
    return -1;
  }
  while(token != NULL){
    int flag = 0; // this is used signify that path is not within a directory
    struct ext2_inode * inode =  (struct ext2_inode *)
                            ( IN_Table + ( sizeof(struct ext2_inode)* ( node-1 ) ) );




    // this for loop iterates through data blocks from a direct pointer
      for(int direct_p = 0; (inode->i_block[direct_p] != 0) && (direct_p != 12); direct_p++){

        // a pointer to the first directory entry of a block
        unsigned char * pointer = BLOCK_OFFSET(inode->i_block[direct_p], disk + 1024);
        int counter = 0;// keeping track block size
        while (counter < EXT2_BLOCK_SIZE && (token != NULL) ){ // counter

          struct ext2_dir_entry * dir_entry =  (struct ext2_dir_entry * )pointer;

          int ret = strncmp(token,dir_entry->name,dir_entry->name_len);

          // if token is = to current dir_entry->name
          if(ret == 0){
            node = dir_entry->inode;
            count++;

            token = strtok(NULL, delim);

            flag = 1;
          }
          else{
          }

          counter+=dir_entry->rec_len;

          pointer +=  dir_entry->rec_len;


        }

        if(flag == 0){

          return -1;
        }

        flag = 0;

    }



  }

  return node;
}


// checks whether a dest already exits as a file in a specific directory(inode)
int validDirectory(int node, char *dest,unsigned char *IN_Table){
  // enter the struct of the inode

  if(dest == NULL){
    fprintf(stderr, "FILE already exists\n");

    return(EEXIST);
  }

  struct ext2_inode * inode =  (struct ext2_inode *)
                          ( IN_Table + ( sizeof(struct ext2_inode)* ( node-1 ) ) );
  // this for loop iterates through data blocks from a direct pointer
  int direct_p;
  for(direct_p = 0;
    (inode->i_block[direct_p] != 0) && (direct_p != 12); direct_p++){

    unsigned char * pointer =  BLOCK_OFFSET(inode->i_block[direct_p], disk + 1024);
    int counter = 0;// keeping track block size
    int flag = 0; // the type of file of the directory entry
    while (counter < EXT2_BLOCK_SIZE ){ // counter
      struct ext2_dir_entry * dir_entry =  (struct ext2_dir_entry * )pointer;

      flag = dir_entry->rec_len;
      if(flag == EXT2_BLOCK_SIZE){
        break;
      }
      int ret = strncmp(dest,dir_entry->name,strlen(dir_entry->name));

      if(ret == 0){ // if there exists a file/directory'name = to dest
        fprintf(stderr, "FILE alre2ady exists\n");

        return(EEXIST);
      }


      pointer +=  dir_entry->rec_len;
      counter+=dir_entry->rec_len;
      flag = dir_entry->rec_len;
    }
  }
  return 0;
}
