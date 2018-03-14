#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

unsigned char *disk;
unsigned char *gd_Offest;
int used_inodes[32] = {2};
int directories[32] = {};
int main(int argc, char **argv) {
    int inode_number = 0; //
    int arr_counter = 1; // the number of inodes in the used_inodes array
    int dir_counter = 0; // index for directories
    if(argc != 2) {
        fprintf(stderr, "Usage: readimg <image file name>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
	perror("mmap");
	exit(1);
    }


    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);

    gd_Offest = disk + 2*EXT2_BLOCK_SIZE;
    struct ext2_group_desc *gd = (struct ext2_group_desc *) gd_Offest;
    printf("\tblock bitmap: %d\n",gd->bg_block_bitmap);
    printf("\tinode bitmap: %d\n",gd->bg_inode_bitmap);
    printf("\tfree blocks: %d\n",gd->bg_free_blocks_count);
    printf("\tfree inodes: %d\n",gd->bg_free_inodes_count);
    printf("\tused_dirs: %d\n",gd->bg_used_dirs_count);

    //exercise 9

    unsigned char *DB_Offset = disk + 3*EXT2_BLOCK_SIZE; // get offset from group descriptor
    // or
    //unsigned char * DB_Offset = BLOCK_OFFSET(gd->bg_block_bitmap, disk + 1024);

    // to increment throug the chars in the Data bitmap
    printf("Block bitmap:");
    for(int byte = 0; byte < 16; byte++){ //there are 16 bytes of valid bits in each block
      //checking if the bit is 1 or 0
      for(int count = 0; count < 8; count++){
         if( ( ( *(DB_Offset + byte) >> count) & 1) == 1){
           printf("1");
         }
         else{
           printf("0");
         }
      }
      printf("  ");
    }
    printf("\n");

    // to increment throug the chars in the Inode bitmap
    unsigned char *IB_Offset = disk + 4*EXT2_BLOCK_SIZE ;
    printf("Inode bitmap:");

    for(int byte = 0; byte < 4; byte++){ //there are 4 bytes of valid bits in each block
      //checking if the bit is 1 or 0

      for(int count = 0; count < 8; count++){

         inode_number += 1;
         if( ( ( *(IB_Offset + byte) >> count) & 1) == 1){
           printf("1");
           // add used inodes that are that 11 in the used_inodes array

           if( inode_number > 11){
             used_inodes[arr_counter] = inode_number;
             arr_counter+=1;
           }
         }
         else{
           printf("0");
         }

      }


      printf("  ");
    }
    //printf("%d", inode_number);

    printf("\n\n");

    printf("Inodes: \n");

    // increment through the inode table
    unsigned char *IN_Table = disk + (5 * EXT2_BLOCK_SIZE);
    for(int index = 0; index < arr_counter; index++){



      struct ext2_inode * inode =  (struct ext2_inode *)
                                ( IN_Table + ( sizeof(struct ext2_inode)* ( used_inodes[index]-1 ) ) );
      char type;    // type of file
      if(S_ISREG(inode->i_mode)){

        type = 'f';
      }
      else if(S_ISDIR(inode->i_mode) || inode->i_mode == EXT2_FT_DIR){

        directories[dir_counter] = used_inodes[index];
        type = 'd';
        dir_counter+=1;
      }
      else{
        type = 's';
      }

      printf("[%d] type: %c size: %u links: %u blocks: %u\n", used_inodes[index], type, inode->i_size, inode->i_links_count ,inode->i_blocks);

      printf("[%d] Blocks: ", used_inodes[index]);

      for(int direct_p = 0; (inode->i_block[direct_p] != 0) && (direct_p != 12); direct_p++){

        printf("%d ", inode->i_block[direct_p] );

      }

      if(inode->i_block[12] != 0){ // if there is an indirect pointer
        unsigned int * indirect_pointer = (unsigned int *) BLOCK_OFFSET(inode->i_block[12], disk + 1024);
        for(int index_of_block = 0;
           ( index_of_block < ( EXT2_BLOCK_SIZE / sizeof(unsigned int *) -1) && indirect_pointer[index_of_block] != 0) ; // index_of_block less than numbr of pointers in a block -1
          index_of_block++  ){

          printf("%d ", indirect_pointer[index_of_block]);
        }
      }


      printf("\n");



    }
    for(int index =0 ; index <  dir_counter ; index++ ){

      struct ext2_inode * inode =  (struct ext2_inode *)
                                ( IN_Table + ( sizeof(struct ext2_inode)* ( directories[index]-1 ) ) );

      // this for loop iterates through data blocks from a direct pointer
      for(int direct_p = 0; (inode->i_block[direct_p] != 0) && (direct_p != 12); direct_p++){

        printf("\tDIR BLOCK NUM: %d \n", inode->i_block[direct_p] );

        unsigned char * pointer =
                                            BLOCK_OFFSET(inode->i_block[direct_p], disk + 1024);
        int counter = 0;// keeping track block size
        char type; // the type of file of the directory entry
        while (counter < EXT2_BLOCK_SIZE){ // counter

          struct ext2_dir_entry * dir_entry =  (struct ext2_dir_entry * )pointer;
          if(dir_entry->file_type ==  EXT2_FT_REG_FILE){ // if the directory entry is a file
            type = 'f';
          }
          else if( dir_entry->file_type == EXT2_FT_DIR){
            type = 'd';
          }
          else{
            type = 's';
          }

          printf("Inode:[%d] rec_len: %hu name_len: %d type= %c ",
                              dir_entry->inode, dir_entry->rec_len, dir_entry->name_len, type);
          printf(" name=%.*s \n", dir_entry->name_len, dir_entry->name );

          counter+=dir_entry->rec_len;

          pointer +=  dir_entry->rec_len;


        }

      }


    }

    return 0;
}
