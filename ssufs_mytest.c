#include "ssufs-ops.h"

/* 
   설계과제 6 테스트 프로그램 (Made By 김한비)
   총 5개의 테스트 케이스가 존재합니다.('*'은 중요도)
   1번(***) > 기본 read/write 테스트
   2번(**) > 최대 파일크기(256bytes) 초과 read/write 에러 처리 테스트
   3번(**) > inode 생성 공간 부족한 경우 에러 처리 테스트
   4번(*****) > block 생성 공간 부족한 경우 에러 처리 및 기존 상태 복구 테스트
   5번(**) > 한 파일의 fd를 여러개 할당받아 다양한 offset에서 write 테스트

   위의 테스트 모두 통과하면 file_handle_array 상태 점검도 해보면 좋을듯~
   
   [혹시 이상한 부분있으면 말좀 해주세요....ㅎㅎ]
 */

int main()
{
	int fd1, fd2, fds[8];
	char st[] = "!10 Bytes!";
	char str[] = "!------30 Bytes of Data------!";
	char stri[] = "!----------------50 Bytes of Data----------------!!----------------50 Bytes of Data----------------!";
	char FullStr[256]; // 256 bytes of data
	memset(FullStr, 'x', 256);
	char str2[256];
	memset(str2, 0, 256);

	ssufs_formatDisk();

	/* 기본 write 및 read와 block 중간 offset에서 write 및 read 테스트
	   직접 데이터 dump 함수로 눈으로 비교해야함*/
	printf("#Test Case 1\n");
	if(ssufs_create("f1.txt") < 0) {
		fprintf(stderr, "create error for case 1\n");
		return 1;
	}
	if((fd1 = ssufs_open("f1.txt")) < 0) {
		fprintf(stderr, "open error for case 1\n");
		return 1;
	}
	// 모든 리턴값이 0이면 성공
	printf("Write Data: %d\n", ssufs_write(fd1, st, 10));
	printf("Write Data: %d\n", ssufs_write(fd1, str, 30));
	printf("Write Data: %d\n", ssufs_write(fd1, str, 30));
	printf("Write Data: %d\n", ssufs_write(fd1, stri, 100));
	printf("Write Data: %d\n", ssufs_write(fd1, str, 30));
	ssufs_lseek(fd1, -200);
	printf("Read Data: %d\n", ssufs_read(fd1, str2, 200)); // 10-30-30-100-30(200bytes)
	printf("DATA : %s\n", str2); // 10-30-30-50-50-30 순서로 출력 시 성공
	memset(str2, 0, 256);
	ssufs_lseek(fd1, -130);
	printf("Read Data: %d\n", ssufs_read(fd1, str2, 130)); 
	printf("DATA : %s\n", str2); // 50-50-30 순서로 출력 시 성공
	ssufs_dump();
	ssufs_delete("f1.txt");
	ssufs_dump();
	printf("\n\n");

	/* 최대 파일크기를 초과해서 write 및 read 시 에러 처리하는지 검사*/
	printf("#Test Case 2\n");
	if(ssufs_create("f2.txt") < 0) {
		fprintf(stderr, "create error for case 2\n");
		return 1;
	}
	if((fd2 = ssufs_open("f2.txt")) < 0) {
		fprintf(stderr, "open error for case 2\n");
		return 1;
	}
	printf("Write Data: %d\n", ssufs_write(fd2, FullStr, 256));
	ssufs_dump();
	if(ssufs_read(fd2, str2, 10) < 0)
		printf("Test(read) 2 Passed!\n");
	else
		printf("Test(read) 2 Failed..\n");
	if(ssufs_write(fd2, st, 10) < 0)
		printf("Test(write) 2 Passed!\n");
	else
		printf("Test(write) 2 Failed..\n");
	ssufs_delete("f2.txt");
	ssufs_dump();
	printf("\n\n");

	/* inode 생성 공간 부족 에러 처리하는지 검사 */
	printf("#Test Case 3\n");
	char filename[] = "f0.txt";
	for(int i = 0; i < 8; i++) {
		filename[1] = i + '0';
		if(ssufs_create(filename) < 0) {
			fprintf(stderr, "create error for case 2\n");
			return 1;
		}
		if((fds[i] = ssufs_open(filename)) < 0) {
			fprintf(stderr, "open error for case 3\n");
			return 1;
		}
	}
	ssufs_dump();
	if(ssufs_create("error.txt") < 0) 
		printf("Test 3 Passed!\n");
	else
		printf("Test 3 Failed!\n");
	printf("\n\n");

	/* DataBlock 공간 부족시 에러 처리하고 롤백되는지 검사 */
	printf("#Test Case 4\n");
	int fd_A, fd_B, std = 10;
	fd_A = open("tmp1", O_CREAT|O_RDWR|O_TRUNC, 0664);
	fd_B = open("tmp2", O_CREAT|O_RDWR|O_TRUNC, 0664);
	dup2(1, std);
	dup2(fd_A, 1);

	for(int i = 0; i < 7; i++) 
		ssufs_write(fds[i], FullStr, 256);
	ssufs_write(fds[7], FullStr, 64);
	ssufs_dump();

	ssufs_write(fds[7], FullStr, 192);
	dup2(fd_B, 1);
	ssufs_dump();

	dup2(std, 1);
	close(fd_A);
	close(fd_B);

	// tmp1과 tmp2의 차이가 있으면 실패 (기존 상태로 복구하는 상황이기 때문임) 
	printf("If something is printed By diff command, Test is failed!!\n");
	printf("--------------------------------------------------------------------------------\n");
	system("diff tmp1 tmp2");
	printf("--------------------------------------------------------------------------------\n");
	// dump 확인을 하려면 unlink 함수 주석처리 후 tmp파일 확인
	unlink("tmp1");
	unlink("tmp2");

	// 디스크 정리
	for(int i = 0; i < 8; i++) {
		filename[1] = i + '0';
		ssufs_delete(filename);
	}
	ssufs_dump();
	printf("\n\n");

	/* 한 파일의 fd를 여러개 생성해서 offset을 다양하게 write.. 
	   눈으로 확인해야함(각 숫자[0~7]가 20개씩 순서대로 저장 시 성공 */
	printf("#Test Case 5\n");
	char nums[20];
	if(ssufs_create(filename) < 0) {
		fprintf(stderr, "create error for case 5\n");
		return 1;
	}
	for(int i = 0; i < 8; i++) {
		if((fds[i] = ssufs_open(filename)) < 0) {
			fprintf(stderr, "open error for case 5\n");
			return 1;
		}
		ssufs_lseek(fds[i], i*20);
		for(int j = 0; j < 20; j++)
			nums[j] = i + '0';
		ssufs_write(fds[i], nums, 20);
	}
	ssufs_dump();
	ssufs_delete(filename);
	ssufs_dump();
}
