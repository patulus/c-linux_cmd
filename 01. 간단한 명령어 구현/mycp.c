#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define MAX_BUFFER_SIZE 1024

void chk_file_err();
void write_file(char*, char*);

int main(int argc, char* argv[])
{
	// 옵션 문자를 저장합니다.
	char opt = 0;
	// 대상 디렉터리의 이름을 저장합니다.
	char dest_dir[MAX_BUFFER_SIZE];
	int cpy_size = 0;

	// argv의 원소로 명령어만 존재하면 원본 파일과 대상 파일이 없으므로 에러를 출력합니다.
	if (argc < 2)
	{
		fprintf(stderr, "[k20210463] mycp: missing file operand\n");
		exit(EXIT_FAILURE);
	}
	
	// 명령어의 옵션을 확인합니다.
	while ((opt = getopt(argc, argv, "")) != -1)
	{
		// 명령어의 옵션을 무시합니다.
	}

	// 인자가 두 개 이상이 아니면 대상 파일이 없으므로 에러를 출력합니다.
	if (argc - optind < 2)
	{
		fprintf(stderr, "[k20210463] mycp: missing destination file operand after \'%s\'\n", argv[optind]);
		exit(EXIT_FAILURE);
	}
	// 인자가 두 개이면 원본 파일을 읽어 대상 파일에 덮어씁니다.
	else if (argc - optind == 2)
	{
		write_file(argv[optind], argv[optind + 1]);
		++optind;
	}
	// 인자가 세 개 이상이면 원본 파일을 읽어 디렉터리 내 해당 이름으로 파일을 덮어씁니다.
	else if (argc - optind > 2)
	{
		// 마지막 인자를 제외한 파일을 읽어 이름이 마지막 인자인 디렉터리에 읽은 파일의 이름으로 파일을 씁니다.
		while (optind < argc - 1)
		{
			strcpy(dest_dir, argv[argc - 1]);
			strcat(dest_dir, "/\0");
			strcat(dest_dir, argv[optind]);

			write_file(argv[optind], dest_dir);
			++optind;
		}
	}

	return 0;
}

void chk_file_err()
{
	if (errno != 0)
	{
		perror("[k20210463] mycp");
		errno = 0;
		exit(EXIT_FAILURE);
	}
}

void write_file(char *name_org, char *name_dest)
{
	// 원본 파일과 대상 파일의 포인터를 생성합니다.
	FILE *org = NULL, *dest = NULL;
	// 읽은 파일의 길이를 저장합니다.
	int read_size = 0;
	// 원본 파일을 읽어 메모리에 일시 보관합니다.
	char buffer[MAX_BUFFER_SIZE] = { 0 };

	// 원본 파일을 읽기 모드로 엽니다.
	org = fopen(name_org, "rb");

	// 원본 파일을 없거나, 읽을 권한이 없는 등 에러가 발생하면 에러를 출력합니다.
	chk_file_err();

	// 대상 파일을 쓰기 모드로 엽니다.
	dest = fopen(name_dest, "wb");

	// 대상 파일을 쓸 권한이 없는 등 에러가 발생하면 에러를 출력합니다.
	chk_file_err();

	// 원본 파일을 읽고, 읽은만큼 대상 파일에 씁니다.
	while ((read_size = fread(buffer, 1, MAX_BUFFER_SIZE, org)) != 0)
	{
		fwrite(buffer, 1, read_size, dest);
	}

	// 파일을 닫습니다.
	fclose(org);
	fclose(dest);
}
