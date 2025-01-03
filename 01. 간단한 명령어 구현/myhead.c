#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define MAX_BUFFER_SIZE 1024

int main(int argc, char* argv[])
{
	// 옵션 문자를 저장합니다.
	char opt = 0;
	// 표준 입력에서 읽은 문자를 저장합니다.
	int stdin_char = 0;

	FILE *src;
	// 읽은 파일의 길이를 저장합니다.
	int read_size = 0;
	// 원본 파일을 읽어 메모리에 일시 보관합니다.
	char buffer[MAX_BUFFER_SIZE] = { 0 };
	char *res = NULL;
	int cntfile = 0;
	int cntline = 0;

	// 주어진 숫자에 맞는 줄 수만큼 출력하는 옵션이 있으면 1, 없으면 0
	int nflag = 0;
	// 기본 줄 수는 10줄
	int nline = 10;

	// 명령어의 옵션을 확인합니다.
	while ((opt = getopt(argc, argv, "n:")) != -1)
	{
		switch (opt)
		{
			// n 옵션인 경우
			// nflag를 1로 세트하고 인자를 nline에 숫자로 변환해 저장합니다.
			case 'n':
				nflag = 1;
				if (atoi(optarg) > 0)
				{
					nline = atoi(optarg);
				}
				else
				{
					fprintf(stderr, "[k20210463] myhead: invalid number of lines: \'%s\'\n", optarg);
					exit(EXIT_FAILURE);
				}
				break;
			default:
				fprintf(stderr, "[k20210463] myhead: invaild option -- \'%c\'\n", optopt);
				exit(EXIT_FAILURE);
		}
	}

	// argv의 원소로 명령어만 존재하면 표준 입력에서 들어오는 내용을 출력합니다.
	if (argc - optind < 1)
	{
		// EOF까지 문자를 하나 읽어서 하나 출력합니다.
		while ((stdin_char = getchar()) != EOF)
		{
			putchar(stdin_char);
		}
	}
	else
	{
		// 인자를 파일명으로 하여 해당 파일들의 내용을 출력합니다.
		cntfile = argc - optind;
		while (argc - optind > 0)
		{
			src = fopen(argv[optind++], "rb");

			// 원본 파일이 없거나, 읽기 권한이 없는 등 에러가 발생하면 에러를 출력합니다.
			if (errno != 0)
			{
				perror("[k20210463] myhead");
				errno = 0;
				continue;
			}

			// 파일명 인자가 여러 개이면 파일의 이름을 출력합니다.
			if (cntfile > 1)
			{
				printf("==> %s <==\n", argv[optind - 1]);
			}

			// 줄 수만큼 출력하기 위해 0으로 세트합니다.
			cntline = 0;

			// 한 줄을 읽어 출력합니다.
			while ((res = fgets(buffer, MAX_BUFFER_SIZE, src)) != NULL)
			{
				fputs(res, stdout);

				// 한 줄이 너무 길어 버퍼 사이즈를 초과하면 '\n'이 있을 때까지 출력을 반복합니다.
				while (strchr(buffer, '\n') == NULL)
				{
					if ((res = fgets(buffer, MAX_BUFFER_SIZE, src)) == NULL)
					{
						break;
					}
					fputs(res, stdout);
				}

				// 인자로 전달한 줄 수가 되면 출력을 중단합니다.
				++cntline;
				if (cntline == nline)
				{
					break;
				}
			}

			// 파일명 인자가 여러 개이면 줄 바꿈으로 파일을 구분합니다.
			if (cntfile > 1)
			{
				printf("\n");
			}

			fclose(src);
		}
	}

	return 0;
}
