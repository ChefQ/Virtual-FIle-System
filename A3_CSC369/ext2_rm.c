#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

#include "ext2_utils.c"
#include <time.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>

unsigned char* disk;

int main(int argc, char **argv){
    
    if(argc != 3) {
        fprintf(stderr, "Usage: ext2_rm <image file name> <path of file to be removed>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    struct ext2_group_desc *group_desc = (struct ext2_group_desc *)(disk + 1024 + 1024);

    char *path = argv[2];

    // path to use with dirname
    char temp_path [strlen(path) + 1];
    strcpy(temp_path, path);
    temp_path[strlen(path)] = '\0';

    struct ext2_inode *inode_table = (struct ext2_inode *)(disk + group_desc->bg_inode_table * 1024);
    
    int inode_number; // expected inode number from check_path function
    inode_number = findPath(path, (unsigned char *)inode_table); 
    
    if(inode_number < 0){ // check whether path exists
        fprintf(stderr, "ERROR: %s\n", strerror(ENOENT));
        exit(ENOENT);
    }
    
    struct ext2_inode inode = inode_table[inode_number - 1];
    // Check type of inode received
    if (S_ISDIR(inode.i_mode)){
        fprintf(stderr, "ERROR: %s\n", strerror(EISDIR));
        exit(EPERM);
    }
    else if(S_ISREG(inode.i_mode)){
        if(inode.i_links_count > 0){
            inode.i_links_count -= 1;
        }
    }
    else{
        fprintf(stderr, "ERROR: %s\n", strerror(EPERM));
        exit(EPERM);
    }
    
    // get dirname of path
    char * dir_name = dirname(temp_path);

    // inode number of dirname
    int dir_inode_number = findPath(dir_name, (unsigned char *)inode_table);

    struct ext2_inode dir_inode = inode_table[dir_inode_number - 1]; 

    // this for loop iterates through data blocks from a direct pointer
    for(int direct_p = 0; (dir_inode.i_block[direct_p] != 0) && (direct_p != 12); direct_p++){

        unsigned char * pointer = BLOCK_OFFSET(dir_inode.i_block[direct_p], disk + 1024);
        int block_counter = 0; // keeping track block size

        // be able to able to change rec_len of directory before you
        // you have to keep of previous dir_entry
        struct ext2_dir_entry * prev = (struct ext2_dir_entry *)pointer;

        // if file is the first dir_entry
        if(prev->inode == inode_number){
            prev->inode = 0;
            memset(prev->name, '\0', prev->name_len);
            prev->name_len = 0;
            prev->file_type = EXT2_FT_UNKNOWN;
            break;
        }

        while (block_counter < EXT2_BLOCK_SIZE){ // counter
            struct ext2_dir_entry * dir_entry =  (struct ext2_dir_entry *)pointer;
            if(dir_entry->inode == inode_number){
                prev->rec_len += dir_entry->rec_len;
                break;
            }

            prev = (struct ext2_dir_entry *)pointer;
            block_counter += dir_entry->rec_len;
            pointer += dir_entry->rec_len;
        }
    }
    
    if(inode.i_links_count == 0){
        int blocks_used[12]; // size is 12 since we are not using indirection here 
        // Set inode.i_dtime which is time of deletion
        inode.i_dtime = time(0);
        
        int data_index = 0;
        // Check data blocks of inodes received
            // store data blocks in an array
        while((inode.i_block[data_index] != 0) && (data_index != 12)){
            blocks_used[data_index] = inode.i_block[data_index];
            data_index++;
        }
    
        // Go to data bitmap, make entries from list 0 
        unsigned char *data_bitmap = disk + (1024 * group_desc->bg_block_bitmap);

        int data_count = 0;
        while(data_count < data_index){
            int current_index = blocks_used[data_count] - 1;

            int data_byte = current_index / 8;
            int data_position = current_index % 8;

            data_bitmap[data_byte] &= ~(1 << (data_position));
            data_count++;
        }
        
        // Go to inode bimap, make inode entry 0
        unsigned char *inode_bitmap = disk + (1024 * group_desc->bg_inode_bitmap);
        int inode_byte = (inode_number - 1) / 8;
        
        int inode_position = (inode_number - 1) % 8;
        inode_bitmap[inode_byte] &= ~(1 << (inode_position)); 
    }
    return 0;  
}
