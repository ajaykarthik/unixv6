/*****************************************************************************************************************
Operating Systems project 2
Ajay Karthik Ganesan and Ashwin Rameshkumar
This program, when run creates a virtual unix file system, which removes the V-6 file system size limit of 16MB
Compiled and run on cs3.utdallas.edu
compile as CC fsaccess.cc and run as ./a.out
Max file size is 12.5 GB
******************************************************************************************************************/

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include <iostream.h>
#include <fstream.h>
#include<vector>
//for file size
#include <sys/stat.h>
using namespace std;

//Global constants
const block_Size = 2048; //Given block size
//Easier to work with global files as we need them in almost all functions and
// we don't need to pass them as parameters every time
int num_iNodes, num_Blocks;
string new_File_System;
int fd;
int next_free_inode = 1; //initialised to 2 , inode 1 is root
//This dynamic memory is written into the file system during the q command and also de-allocated
void *root_dir;
void *super;
long long max_file_size = 13421772800;

//function prototype
off_t fsize(const char *filename);

void test();
/*************************************************************************
struct superblock
Variables same as those in v-6 file-system, 
From the given requirements, we consider that there is one i node per block
**************************************************************************/
struct superblock
{
	unsigned short isize;
	unsigned short fsize;
	unsigned short nfree;
	unsigned int free[100];
	unsigned short ninode;
	unsigned int inode[100];
	char flock;
	char ilock;
	char fmod;
	unsigned short time[2];

	//constructor
	superblock()
	{
		isize = num_iNodes;
		fsize = num_Blocks; //First block not potentially available for allocation to a file
		ninode = num_iNodes;
		//initial n-free value
		nfree = 100;
		//dummy values for the rest of the variables
		flock = 'j';
		ilock = 'b';
		fmod = 'n';
		time[0] = time[1] = 0;
		//current_block will be used later
		int current_block= (num_iNodes + 3) ;
		
		//Write free data blocks into free[] list 
		//For example, if num_iNodes is 100, free data blocks start from 103 ( We use one block per iNode)
		for (int j=0; (current_block < (num_iNodes + 103)) && (current_block < num_Blocks); current_block++,j++)
		{
			//Block 0 is unused, block 1 is super-block,block 3 is root directory
			//rest of the num_iNodes are blocks allocated for iNodes
			//Hence data blocks start from num_iNodes+ 3
			free[99-j] = current_block;
			//inode[] is not used in this implementation, hence a dummy value is assigned
			inode[j] = 0;
		}
		//i is the first block not written into the free list

		//Write next 100 free free data block into the 0th block of the free[] array
		//repeat this till the data blocks are exhausted
		int first_data_block;
		int new_nfree;
		int *new_free_array;
		
		while(current_block < num_Blocks)
		{
			first_data_block = current_block-1;

			//lseek to this block and write nfree as first 2 bytes
			//Get pointer to block 1 
			if (lseek(fd,first_data_block * block_Size, SEEK_SET) < 0)
			{
				cout << "Error getting to first_data_block for assigning new nfree\n";
			}
			
			//write nfree 
			if((num_Blocks - current_block) > 100) 
			{
				new_nfree = 100;
			}
			else
			{
				new_nfree = (num_Blocks - current_block) ;
			}
			new_free_array = new int[new_nfree+1];
			new_free_array[0] = new_nfree;
			//use current block and write next 100 blocks (if blocks are available)
			for(int j=1;j < new_nfree+1 ;j++,current_block++)
			{
				new_free_array[(new_nfree+1)-j] = current_block ;
			}
			
			//Write the whole block, because its easier
			if (write(fd, new_free_array ,block_Size) < block_Size)
			{
				cout << "Error writing new block";
			}
			delete[] new_free_array;
		}
		
		
		
	}
	/***************************************************************************************************
	get the next free data block to assign to a file or directory, if nfree becomes 0 , read 
	the contents of free[0] and assign first number to nfree and next 100 number to free[] array
	Note: No check is made to see if all data blocks are exhausted as this is not part of the requirement
	****************************************************************************************************/
	int get_next_freeblock()
	{
		nfree--;
		if(nfree == 0)
		//bring in contents from free[0] and fill it up as the new nfree and free array
		{
			int block_to_return = free[0];

			if (lseek(fd,free[0] * block_Size, SEEK_SET) < 0)
			{
				cout << "Error getting to free[0] for reading new nfree\n";
				return -1;
			}
			//max size will be 101 
			int *new_free_array = new int[101];
			
			if (read(fd, new_free_array ,block_Size) < 0)
			{
				cout << "Error reading new block";
				return -1;
			}
			nfree=new_free_array[0];
			for(int i=0;i<nfree;i++)
			{
				free[i] = new_free_array[i+1];
			}
			delete[] new_free_array;
			return block_to_return;
		}
		//Business as usual
		else
		{
			return free[nfree];
		}
	}
	
	/***************************************************************************************************
	return the last free block allocated, used for reference
	****************************************************************************************************/
	int last_block_used()
	{
		return free[nfree];
	}
	
	//destructor
	~superblock()
	{
		delete[] free;
	    delete[] inode;
	    delete[] time;
	}
};

/**************************************************************************************
struct inode
Variables same as those in v-6 filesystem, but size of file and addr[] 
size values are updated to increase max size 
**************************************************************************************/

struct inode
{
	unsigned int flags;
	char nlinks;
	char uid;
	char gid;
	//Max file size is 12.5GB
	unsigned long long int size;
	//Each is a double in-direct block  
	unsigned int addr[25];
	unsigned short actime;
	unsigned short modtime[2];
	
	//constructor
	inode()
	{
		flags = 004777; //initialized to unallocated, plain small file, set uid on execution, permission for all users is 1
		nlinks='0';
		uid='1';
		gid='2';
		size=0;
		modtime[0]=0;
		modtime[1]=0;
		actime=1;
	}

};

/**************************************************************************************
struct directory
Used to write the root directory (along with file and sub-directory entries) 
and also sub-directories
**************************************************************************************/
struct directory
{
	//Entry could be a file or a directory
	string *entry_Name ;
	int *inode_list;
	int inode_iterator;
	
	//Initialise root directory with given name , written to block after the inodes are assigned
	directory()
	{
		entry_Name = new string[num_iNodes+1];
		inode_iterator = 0;
		inode_list = new int[num_iNodes];
		entry_Name[inode_iterator] = new_File_System; // file system name for every directory, including root
		inode_list[inode_iterator]=1;// inode of root is 1
		inode_iterator++;
		entry_Name[inode_iterator] = new_File_System;
		inode_list[inode_iterator]=1;
		inode_iterator++;

	}
	
	
	//Initialize sub directory (mkdir)
	directory(string dir_name)
	{
		entry_Name = new string[2]; // one for root, one for self
		inode_iterator = 0;
		inode_list = new int[2];
		entry_Name[inode_iterator] = dir_name;
		inode_list[inode_iterator] = next_free_inode;
		inode_iterator++;
		entry_Name[inode_iterator] = new_File_System;
		inode_list[inode_iterator] = 1;//root
		inode_iterator++;
		
	}
	
	
	
	//Delete the dynamic heap memory to prevent leakage
	~directory()
	{
		delete[] inode_list;
		delete[] entry_Name;
	}
	
	//Entry inside a folder ( Only the root folder has entries in this implementation )
	void file_entry(string entry)
	{

		entry_Name[inode_iterator]= entry; 
		inode_list[inode_iterator] = next_free_inode;
		inode_iterator++;
		//return 0;
		
	}
	
};

/*************************************************************************
function:
initfs returns int so as the return -1 on read,write and seek errors
initialise the virtual file system 
Global variables used
path: Path of the file that represents the virtual disk
num_Blocks: number of blocks allocated in total
num_iNodes: number of iNodes ( We store one iNode per block, the remaining 
part of each block is not used )
**************************************************************************/
int initfs()
{
	
	int file_System_Size = num_Blocks * 2048; // Size of on block * number of blocks
	char *disk_Size_dummy = new char[file_System_Size]; //to fill the disk with 0's

	/***************************************************************************
	Initialize the file system (all blocks) with '0's
	***************************************************************************/
	
	//Set all blocks to '0' 
	for (int i=0; i < file_System_Size; i++)
	{
		disk_Size_dummy[i] = '0';
	}

	//Get pointer to block 0 
	if (lseek(fd, 0, SEEK_SET) < 0)
	{
		cout << "Error getting to block 0 for assigning 0's\n";
		return -1;
	}

	//Write 0's to the whole file system
	if (write(fd, disk_Size_dummy, file_System_Size) < file_System_Size)
	{
		cout << "Error writing file system";
		return -1;
	}

	//delete dummy value from heap
	delete[] disk_Size_dummy;

	/***************************************************************************
	Write super block to block 1 of the file system
	super block size is 820 bytes (Remaining bytes in the block are unused)
	***************************************************************************/
	
	//Create super-block
	super = new superblock();

	//Get pointer to block 1 
	if (lseek(fd, block_Size, SEEK_SET) < 0)
	{
		cout << "Error getting to block 1 for assigning super-block\n";
		return -1;
	}

	//Write super-block onto the file system
	if (write(fd, super, sizeof(superblock)) < sizeof(superblock))
	{
		cout << "Error writing super-block\n";
		return -1;
	}
	
	/**************************************************************************************
	Write iNodes to the file system
	One iNode per block, iNode size is 220 bytes (Remaining bytes in the block are unused)
	Start from block 2 ( Block 0 is unused and Block 1 superblock )
	***************************************************************************************/

	//Create an i-node to write num_inode times
	inode *temp_iNode = new inode();
	
	
	for(int i=0; i<num_iNodes; i++)
	{
		
		//Get pointer to block i+2
		if(lseek(fd, (i+2)*block_Size, SEEK_SET) < 0)
		{
			cout<<"Error getting to block for writing i nodes\n";
			return -1;
		}
	
		//Write block i+2 with inode
		if(write(fd,temp_iNode,sizeof(inode)) < sizeof(inode))
		{
			cout<<"Error writing inode number "<<i<<endl;
			return -1;
		}
		
	}
	delete[] temp_iNode;
	/**************************************************************************************
	Write the root directory information into the first data block 
	This is used to keep track of the files and directories in the file system
   ***************************************************************************************/   
	root_dir = new directory();
	
	//write root directory in the file system
	//Get pointer to block i+2
	if(lseek(fd, (num_iNodes+ 2)*block_Size, SEEK_SET) < 0)
	{
		cout<<"Error getting to block for writing root dir\n";
		return -1;
	}

	//Write block i+2 with inode
	if(write(fd,root_dir,sizeof(directory)) < sizeof(directory))
	{
		cout<<"Error writing directory \n";
		return -1;
	}
	
	/***********************************************************************************************
	Create inode for the root directory and write it into block 2 (beginning of inodes)
	***********************************************************************************************/
	inode *root_inode = new inode();
	root_inode->flags = 144777; // i node is allocated and file type is directory
	
	//go to the inode
	if (lseek(fd,2 * block_Size, SEEK_SET) < 0)
	{
		cout << "Error getting to block 0 for assigning 0's\n";
		//return -1;
	}

	//Write root inode
	if (write(fd, root_inode, sizeof(inode)) < sizeof(inode))
	{
		cout << "Error writing root inode";
		//return -1;
	}
	
	delete[] root_inode;
}

/**************************************************************************************
	cpin : create a new file called v6-file in the newly created file system and copy the 
	contents of the input file into the new file system.
	v6-file: input file name
	
***************************************************************************************/

int cpin ( string v6_file  )
{
	//file descriptors for the external(input) file
	int inputfd;
	inputfd = 0;

	if((inputfd=open(v6_file.c_str(),O_RDWR)) < -1)
	{
		cout<<"Error opening input file\n";
		return -1;
	}
	
	inode *node = new inode();
	
	unsigned long long int filesize ;
	filesize = fsize(v6_file.c_str());

	node->size = filesize;

	if(filesize == 0)
	{
		cout<<"Error empty file\n";
		return -1;
	}
	
	//inode for file
	next_free_inode++;
	
	int num_blocks_needed_for_file=0;
		
	//calculate the the data blocks required to store the file
	num_blocks_needed_for_file = filesize/block_Size;
	
	if(filesize%block_Size != 0) num_blocks_needed_for_file++; //extra data lesser than a block size
	
	
	
	if(filesize <= 51200)
	//Small file
	{	
		char* input_file_contents = new char[filesize];
		// read the file 
		if (lseek(inputfd, 0 ,SEEK_SET) < 0)
		{
			cout << "Error seek input file\n";
			return -1;
		}
		
		if (read(inputfd, input_file_contents ,filesize) < filesize)
		{
			cout << "Error reading input file\n";
			return -1;
		}
	
		node->flags = 104777; //allocated, plain , small file
		
		//get contents for the addr[] array

		for(int i=0;i<num_blocks_needed_for_file;i++)
		{
			node->addr[i] = ((superblock*)(super))->get_next_freeblock();
		}
		
		//write null values to the remaining addr[]
		for(int i=num_blocks_needed_for_file;i<25;i++)
		{
			node->addr[i] = 0;//null
		}
		/**********************************************************************************
		write inode
		**********************************************************************************/
		
		//first inode starts from block 2. (i.e inode 1(root) is in block 2)
		if (lseek(fd,(next_free_inode+1)  * block_Size, SEEK_SET) < 0)
		{
			cout << "Error getting to block "<<next_free_inode<<endl;
			return -1;
		}

		if (write(fd, node, sizeof(inode)) < sizeof(inode))
		{
			cout << "Error writing inode "<<next_free_inode<<endl;
			return -1;
		}

		/**********************************************************************************
		write data
		**********************************************************************************/

		if (lseek(fd,(node->addr[0])  * block_Size, SEEK_SET) < 0)
		{
			cout << "Error getting to addr[0] small file \n";
			return -1;
		}

		if (write(fd, input_file_contents, filesize) < filesize)
		{
			cout << "Error writing file "<<endl;
			return -1;
		}

		delete[] input_file_contents;
	}
	else
	//Large file
	{
		node->flags = 114777; //allocated, plain , large file
		
		//one addr[] stores 512*512 = 262144 blocks
		int addr_count_required = num_blocks_needed_for_file/262144 ;
		
		if(num_blocks_needed_for_file%262144!=0)addr_count_required++;
		
		//file size exceeds maximum
		if(addr_count_required > 25)
		{
			cout<<"File size exceeds maximum\n";
			return -1;
		}

		/**********************************************************************************
		write addr array
		a single address in the addr array can point to 512*512 blocks
		**********************************************************************************/
		
		//addr[]
		for(int i=0;i<addr_count_required;i++)
		{
			node->addr[i] = ((superblock*)(super))->get_next_freeblock();
		}
		
		//write null values to the remaining addr[]
		for(int i=addr_count_required;i<25;i++)
		{
			node->addr[i] = 0;//null
		}
		/**********************************************************************************
		write inode
		**********************************************************************************/
		
		//first inode starts from block 2. (i.e inode 1(root) is in block 2)
		if (lseek(fd,(next_free_inode+1)  * block_Size, SEEK_SET) < 0)
		{
			cout << "Error getting to block "<<next_free_inode<<endl;
			return -1;
		}
		
		if (write(fd, node, sizeof(inode)) < sizeof(inode))
		{
			cout << "Error writing inode "<<next_free_inode<<endl;
			return -1;
		}
		/**********************************************************************************
		write pointers - Level 1
		A Single address in a level can point to 512 blocks 
		**********************************************************************************/
		int *blocks_to_assign ;
		//assume blocks till addr_count_required would be full (there is some data wastage)
		//Level 1
		int num_blocks_allocated = 0; // Keep track of num blocks allocated Vs num of blocks in file
		blocks_to_assign = new int [512];
		for(int i=0;i<addr_count_required;i++)
		{ 	
			
			//write 512 addresses into the first indirect block
			int j=0;
			for(;(j<512) && (num_blocks_allocated<=num_blocks_needed_for_file);j++)
			{
				blocks_to_assign[j] =((superblock*)(super))->get_next_freeblock();	
				num_blocks_allocated = num_blocks_allocated + 512;			
			}
			
			//add zeros if less than 512
			for(;j<512;j++)
			{
				blocks_to_assign[j] = 0;
			}

			//go to addr[i]
			if (lseek(fd, node->addr[i] * block_Size, SEEK_SET) < 0)
			{
				cout << "Error getting to addr "<<endl;
				return -1;
			}	
			
			//write these free blocks into addr[i]
			if (write(fd, blocks_to_assign, block_Size) < block_Size)
			{
				cout << "Error writing pointers "<<endl;
				return -1;
			}
			
		}
		delete[] blocks_to_assign;
		/**********************************************************************************
		write pointers - Level 2
		A Single address in a level can point to 1 block
		**********************************************************************************/
		//The starting address is the block next to addr[addr_count_required] as data is sequential
		int start = ((node->addr[addr_count_required-1])+1);
		int stop = ((superblock*)(super))->last_block_used();

		num_blocks_allocated = 0;//reset 
		blocks_to_assign = new int [512];
		for(int i=start ; i<= stop  ; i++)
		{		
			//write 512 addresses into the first indirect block
			int j=0;
			for(;(j<512) && (num_blocks_allocated<=num_blocks_needed_for_file);j++)
			{
				blocks_to_assign[j] =((superblock*)(super))->get_next_freeblock();	
				num_blocks_allocated++ ;			
			}
			
			//add zeros if less than 512
			for(;j<512;j++)
			{
				blocks_to_assign[j] = 0;
			}

			//go to addr[i]
			if (lseek(fd, i * block_Size, SEEK_SET) < 0)
			{
				cout << "Error getting to addr "<<endl;
				return -1;
			}	
			
			//write these free blocks into addr[i]
			if (write(fd, blocks_to_assign, block_Size) < block_Size)
			{
				cout << "Error writing pointers "<<endl;
				return -1;
			}
			
		}
		delete[] blocks_to_assign;
		
		/**********************************************************************************
		read input data
		**********************************************************************************/
		
		char* input_file_contents = new char[filesize];
		// read the file 
		
		if (lseek(inputfd, 0, SEEK_SET) < 0)
		{
			cout << "Error getting to addr "<<endl;
			return -1;
		}
		
		if (read(inputfd, input_file_contents ,filesize) < filesize)
		{
			cout << "Error reading input file\n";
			return -1;
		}
		
		/**********************************************************************************
		write data
		**********************************************************************************/
		int write_data_from = stop +1; //The starting address is the block next to stop as data is sequential
		//go to addr[i]

		if (lseek(fd, write_data_from * block_Size, SEEK_SET) < 0)
		{
			cout << "Error getting to addr "<<endl;
			return -1;
		}	
		//write data
		if (write(fd, input_file_contents, filesize) < filesize)
		{
			cout << "Error writing data "<<endl;
			return -1;
		}
		
		
		delete[] input_file_contents;
	}

	//Entry into the root directory
	((directory*) root_dir)->file_entry(v6_file.c_str());
	delete[] node;
	
}

/******************************************************************************************************
This function is used to find the file size of the given input file for the cpin function
This function is from the stack overflow website
http://stackoverflow.com/questions/8236/how-do-you-determine-the-size-of-a-file-in-c
filename: input file name
off_t : returns the size of the file
*******************************************************************************************************/
off_t fsize(const char *filename) {
    struct stat st; 

    if (stat(filename, &st) == 0)
        return st.st_size;

    return -1; 
}


/**************************************************************************************
	cpout : create external file if v6 file exists
***************************************************************************************/
void cpout(string v6_file, string externalfile)
{
	//Check if v6 file exists
	int ilist_match=0;
	for(;ilist_match<=((directory*)root_dir)->inode_iterator;ilist_match++)
	{
		if(((directory*)root_dir)->entry_Name[ilist_match] == v6_file) break;
	}
	
	if(ilist_match>=((directory*)root_dir)->inode_iterator) 
	{
		cout<<"File doesn't exist\n";
		return;
	}
	//get inode for the v6 file
	int v6_inode = ((directory*)root_dir)->inode_list[ilist_match];
	
	inode *node = new inode();

	//inode x is in block x+1
	if (lseek(fd, (v6_inode+1) * block_Size ,SEEK_SET) < 0)
	{
		cout << "Error getting to addr "<<endl;
	}	
	
	if (read(fd, node ,sizeof(inode)) < sizeof(inode))
	{
		cout << "Error reading v6 inode\n";
	}

	//get file size and starting address, file is contiguous
	unsigned long long int file_size = node->size;
	
	int starting_addr;
	
	if(file_size <= 51200)
	{
		starting_addr = node->addr[0];
	}
	else
	{
		//get addr[0] block
		int *block0 = new int[512];
		
		if (lseek(fd, node->addr[0] * block_Size,SEEK_SET) < 0)
		{
			cout << "Error getting to starting_addr "<<endl;
		}
		
		if (read(fd, block0 ,block_Size) < block_Size)
		{
			cout << "Error reading input file block0\n";
		}
		
		//get first element of block0
		int *block1 = new int[512];
		
		if (lseek(fd, block0[0] * block_Size,SEEK_SET) < 0)
		{
			cout << "Error getting to starting_addr "<<endl;
		}
		
		if (read(fd, block1 ,block_Size) < block_Size)
		{
			cout << "Error reading input file block1\n";
		}

		starting_addr = block1[0];
		delete[] block0;
		delete[] block1;
	}
	
	
	
	char* v6_file_contents = new char[file_size];
	
	//read file
	if (lseek(fd, starting_addr * block_Size,SEEK_SET) < 0)
	{
		cout << "Error getting to starting_addr "<<endl;
	}	
	
	if (read(fd, v6_file_contents ,file_size) < file_size)
	{
		cout << "Error reading input file\n";
	}
	//write output
	ofstream file_to_return;;
	file_to_return.open(externalfile.c_str());
	
	int external_fd;
	if((external_fd=open(externalfile.c_str(),O_RDWR)) < -1)
	{
		cout<<"Error opening file descriptor for next free inode\n";
	}
	
	if (lseek(external_fd, 0,SEEK_SET) < 0)
	{
		cout << "Error getting to external "<<endl;
	}	

	if (write(external_fd, v6_file_contents ,file_size) < file_size)
	{
		cout << "Error writing input file\n";
	}
	file_to_return.close();
	delete[] v6_file_contents;
}


/**************************************************************************************
	mkdir: create a new directory by the name dirname and store its contents in
	root dir ilist and also create an inode 
***************************************************************************************/
void mkdir(string dirname)
{

	//Check if dir exists
	for(int ilist_match=0;ilist_match<=((directory*)root_dir)->inode_iterator;ilist_match++)
	{
		if(((directory*)root_dir)->entry_Name[ilist_match] == dirname) 
		{
			cout<<"Directory already exists\n";
			return;
		}
	}
	
	//entry in root's ilist
	((directory*)root_dir)->file_entry(dirname.c_str());
	
	//inode for dir
	next_free_inode++;
	
	//create inode
	inode *dir_inode = new inode();
	
	
	dir_inode->flags = 144777; // allocated, directory, user ID, all access
	//get block for storing addr
	int block = ((superblock*)(super))->get_next_freeblock();	
	dir_inode->addr[0] = block;
	
	//write inode in file system
	if (lseek(fd, (next_free_inode+1) * block_Size ,SEEK_SET) < 0)
	{
		cout << "Error getting to inode "<<endl;
	}	

	if (write(fd, dir_inode ,sizeof(inode)) < sizeof(inode))
	{
		cout << "Error writing input file\n";
	}
	
	//create the sub directory
	directory* sub_dir = new directory(dirname.c_str());
	
	//write the directory into the file system
	if (lseek(fd, block * block_Size ,SEEK_SET) < 0)
	{
		cout << "Error getting to dir "<<endl;
	}	

	if (write(fd, sub_dir ,sizeof(directory)) < sizeof(directory))
	{
		cout << "Error writing input file\n";
	}
	
	delete[] dir_inode;
	
}


/**************************************************************************************
	quit: save all system data and close
***************************************************************************************/
void quit()
{
	//need to save superblock and root directory
	//write superblock in file system
	if (lseek(fd, block_Size ,SEEK_SET) < 0)
	{
		cout << "Error getting to inode "<<endl;
	}	

	if (write(fd, (superblock*)super ,sizeof(superblock)) < sizeof(superblock))
	{
		cout << "Error writing superblock file\n";
	}
	
	//write root dir in first block after inodes (this space was initially reserved)
	if (lseek(fd,(num_iNodes +1 ) * block_Size ,SEEK_SET) < 0)
	{
		cout << "Error getting to inode "<<endl;
	}	

	if (write(fd, (directory*)root_dir ,sizeof(directory)) < sizeof(directory))
	{
		cout << "Error writing directory file\n";
	}

}

int main()
{
	//temp assignment (get from user)
	new_File_System = "testing";
	num_iNodes = 300;
	num_Blocks = 10000;

	ofstream outputFile;
	outputFile.open(new_File_System.c_str());
	
	
	if((fd=open(new_File_System.c_str(),O_RDWR)) < -1)
	{
		cout<<"Error opening file descriptor for next free inode\n";
        return -1;
	}
	
	
	if(initfs() == -1)
	{
		cout<<"Error initializing file system\n";
		return -1;
	}

	
	cpin("test.docx");
	
	cpout("test.docx","extern.txt");

	mkdir("folder");
	
	outputFile.close();
	
	//temp (add to q)
	delete[] root_dir;
	delete[] super;
	return (0);
	
}



