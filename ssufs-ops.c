#include "ssufs-ops.h"

extern struct filehandle_t file_handle_array[MAX_OPEN_FILES];

int ssufs_allocFileHandle() {
	for (int i = 0; i < MAX_OPEN_FILES; i++) {
		if (file_handle_array[i].inode_number == -1) {
			return i;
		}
	}
	return -1;
}


int ssufs_create(char* filename) {
	/* 1 */

	int inode_index = -1;

	inode_index = ssufs_allocInode();
	if (inode_index == -1) { //파일 생성할 공간이 없는 경우
		return -1;
	}

	struct inode_t* tmp2 = (struct inode_t*)malloc(sizeof(struct inode_t));

	for (int i = 0; i < NUM_INODES; i++) {
		ssufs_readInode(i, tmp2);
		if (strcmp(tmp2->name, filename) == 0) { //동일한 이름의 파일이 있는 경우
			free(tmp2);
			return -1;
		}
	}

	tmp2->status = INODE_IN_USE;
	memcpy(tmp2->name, filename, strlen(filename));
	ssufs_writeInode(inode_index, tmp2);

	free(tmp2);
	return inode_index;
}


void ssufs_delete(char* filename) {
	/* 2 */

	int inode_index = -1;

	inode_index = open_namei(filename);

	struct inode_t* tmp = (struct inode_t*)malloc(sizeof(struct inode_t));
	ssufs_readInode(inode_index, tmp);

	if (strcmp(tmp->name, filename) == 0) {
		memcpy(tmp->name, "", 1);
		ssufs_writeInode(inode_index, tmp);
		ssufs_freeInode(inode_index);
	}

	free(tmp);
}


int ssufs_open(char* filename) {
	/* 3 */

	int file_handle = 0;
	int inode_index = -1;

	file_handle = ssufs_allocFileHandle();
	inode_index = open_namei(filename);

	if (inode_index == -1) {
		return -1;
	}

	file_handle_array[file_handle].inode_number = inode_index;
	file_handle_array[file_handle].offset = 0;

	return file_handle;
}


void ssufs_close(int file_handle) {
	file_handle_array[file_handle].inode_number = -1;
	file_handle_array[file_handle].offset = 0;
}


int ssufs_read(int file_handle, char* buf, int nbytes) {
	/* 4 */

	/* 에러처리 */
	if (nbytes == 0) { //0byte 쓰기 요청
		return -1;
	}

	if (file_handle_array[file_handle].inode_number == -1) { //파일 not open
		return -1;
	}

	struct inode_t* tmp = (struct inode_t*)malloc(sizeof(struct inode_t));
	ssufs_readInode(file_handle_array[file_handle].inode_number, tmp);

	if (file_handle_array[file_handle].offset + nbytes > tmp->file_size) { //파일 크기 초과
		free(tmp);
		return -1;
	}


	/* data block read, offset update */
	int offset = file_handle_array[file_handle].offset;

	if (nbytes <= BLOCKSIZE) {
		ssufs_readDataBlock(tmp->direct_blocks[offset / BLOCKSIZE], buf);
		file_handle_array[file_handle].offset += nbytes;
	}
	else if ((nbytes > BLOCKSIZE) && (nbytes <= 2 * BLOCKSIZE)) {
		char* buf2 = malloc(BLOCKSIZE);
		ssufs_readDataBlock(tmp->direct_blocks[offset / BLOCKSIZE], buf);
		ssufs_readDataBlock(tmp->direct_blocks[offset / BLOCKSIZE + 1], buf2);
		strcat(buf, buf2);
		free(buf2);

		file_handle_array[file_handle].offset += nbytes;
	}
	else if ((nbytes > BLOCKSIZE * 2) && (nbytes <= 3 * BLOCKSIZE)) {
		char* buf2 = malloc(BLOCKSIZE);
		char* buf3 = malloc(BLOCKSIZE);
		ssufs_readDataBlock(tmp->direct_blocks[offset / BLOCKSIZE], buf);
		ssufs_readDataBlock(tmp->direct_blocks[offset / BLOCKSIZE + 1], buf2);
		ssufs_readDataBlock(tmp->direct_blocks[offset / BLOCKSIZE + 2], buf3);
		strcat(buf, buf2);
		strcat(buf, buf3);
		free(buf2);
		free(buf3);

		file_handle_array[file_handle].offset += nbytes;
	}
	else {
		char* buf2 = malloc(BLOCKSIZE);
		char* buf3 = malloc(BLOCKSIZE);
		char* buf4 = malloc(BLOCKSIZE);
		ssufs_readDataBlock(tmp->direct_blocks[offset / BLOCKSIZE], buf);
		ssufs_readDataBlock(tmp->direct_blocks[offset / BLOCKSIZE + 1], buf2);
		ssufs_readDataBlock(tmp->direct_blocks[offset / BLOCKSIZE + 2], buf3);
		ssufs_readDataBlock(tmp->direct_blocks[offset / BLOCKSIZE + 3], buf4);
		strcat(buf, buf2);
		strcat(buf, buf3);
		strcat(buf, buf4);
		free(buf2);
		free(buf3);
		free(buf4);

		file_handle_array[file_handle].offset += nbytes;
	}


	free(tmp);
	return 0;
}


int ssufs_write(int file_handle, char* buf, int nbytes) {
	/* 5 */

	/* 에러처리 */
	if (nbytes == 0) { //0byte 쓰기 요청
		return -1;
	}

	if (file_handle_array[file_handle].inode_number == -1) { //파일 not open
		return -1;
	}

	struct inode_t* tmp = (struct inode_t*)malloc(sizeof(struct inode_t));
	ssufs_readInode(file_handle_array[file_handle].inode_number, tmp);

	if (tmp->file_size + nbytes > BLOCKSIZE * MAX_FILE_SIZE) { //파일 크기 초과
		free(tmp);
		return -1;
	}

	int block_num = -1;
	int block_check_num = -1;
	for (int i = 0; i < MAX_FILE_SIZE; i++) {
		if (tmp->direct_blocks[i] == -1) {
			block_check_num = i - 1;
			break;
		}
	}

	/* data block alloc, write */
	if (block_check_num == -1) {
		block_num = ssufs_allocDataBlock();
		if (block_num == -1 || block_num > 7) { //free block not exist
			ssufs_freeDataBlock(block_num);
			return -1;
		}
		ssufs_writeDataBlock(block_num, buf);
		for (int i = 0; i < MAX_FILE_SIZE; i++) {
			if (tmp->direct_blocks[i] == -1) {
				tmp->direct_blocks[i] = block_num;
				break;
			}
		}

	}
	else {
		block_num = block_check_num;
		int block_num2 = -1;

		char* read_data_block = malloc(BLOCKSIZE);
		char* copy_block = malloc(BLOCKSIZE);
		ssufs_readDataBlock(block_num, read_data_block);
		strcpy(copy_block, read_data_block);

		if (BLOCKSIZE - strlen(read_data_block) > nbytes) {
			strcat(copy_block, buf);
			ssufs_writeDataBlock(block_num, copy_block);
		}
		else {
			char* tmp_block = malloc(BLOCKSIZE);
			char* tmp_block2 = malloc(BLOCKSIZE);
			for (int i = 0; i < BLOCKSIZE - strlen(read_data_block); i++) {
				tmp_block[i] = buf[i];
			}
			strcat(copy_block, tmp_block);

			ssufs_writeDataBlock(block_num, copy_block);

			block_num2 = ssufs_allocDataBlock();
			if (block_num2 == -1 || block_num2 > 7) { //free block not exist
				ssufs_freeDataBlock(block_num2);
				return -1;
			}
			for (int i = 0; i < nbytes - (BLOCKSIZE - strlen(read_data_block)); i++) {
				tmp_block2[i] = buf[i + (BLOCKSIZE - strlen(read_data_block))];
			}


			ssufs_writeDataBlock(block_num2, tmp_block2);
			for (int i = 0; i < MAX_FILE_SIZE; i++) {
				if (tmp->direct_blocks[i] == -1) {
					tmp->direct_blocks[i] = block_num2;
					break;
				}
			}
			//			free(tmp_block);
			//			free(tmp_block2);
		}

		free(read_data_block);
		free(copy_block);
	}


	/* inode 정보 update */
	tmp->file_size += nbytes;
	ssufs_writeInode(file_handle_array[file_handle].inode_number, tmp);


	/* offset update */
	file_handle_array[file_handle].offset += nbytes;



	free(tmp);
	return 0;
}


int ssufs_lseek(int file_handle, int nseek) {
	int offset = file_handle_array[file_handle].offset;

	struct inode_t* tmp = (struct inode_t*)malloc(sizeof(struct inode_t));
	ssufs_readInode(file_handle_array[file_handle].inode_number, tmp);

	int fsize = tmp->file_size;

	offset += nseek;

	/* 오프셋이 현재 파일 크기 경계 이상으로 이동하면 에러 */
	if ((fsize == -1) || (offset < 0) || (offset > fsize)) {
		free(tmp);
		return -1;
	}

	file_handle_array[file_handle].offset = offset;
	free(tmp);

	return 0;
}
