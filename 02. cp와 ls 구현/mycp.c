#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PROGRAM_NAME "mycp"
#define BUFFER_SIZE 1024
#define PATH_BUFFER_SIZE 4096

int write_file(const int, const int, char*);
char* get_filename(char* const);

int main(int argc, char* argv[])
{
    // 옵션 처리와 관련된 변수입니다.
    int opt = -1;

    char buffer[BUFFER_SIZE] = { 0 };
    char path_buffer[PATH_BUFFER_SIZE] = { 0 };

    // 파일 디스크립터와 관련된 변수입니다.
    int src_filedes = -1;
    int out_filedes = -1;

    // 파일 속성과 관련된 변수입니다.
    struct stat src_info;
    struct stat out_info;

    // 옵션을 처리합니다.
    while ((opt = getopt(argc, argv, "")) != -1)
    {
        // 옵션이 주어지면 옵션을 무시합니다.
    }

    // 3)
    // 인자가 없는 경우, 인자가 하나인 경우, 인자가 두 개 주어졌으나 파일을 열 수 없는 경우 등에 대하여
    // cp 명령어의 실행 결과를 보고 그와 동일한 동작을 수행하도록 한다.
    // 주어진 인자가 없는 경우에 오류를 출력하고 프로그램을 종료합니다.
    if (argc - optind < 1)
    {
        fprintf(stderr, "%s: missing file operand\n", PROGRAM_NAME);
        exit(EXIT_FAILURE);
    }
    // 인자가 하나만 주어진 경우에 오류를 출력하고 프로그램을 종료합니다.
    else if (argc - optind < 2)
    {
        fprintf(stderr, "%s: missing destination file operand after '%s'\n", PROGRAM_NAME, argv[argc - 1]);
        exit(EXIT_FAILURE);
    }
    // 1)
    // `mycp a b`에서 a와 b가 모두 일반 파일인 경우 a의 내용을 b에 복사한다.
    // 이때 b가 존재하지 않는 파일이면 새 파일을 생성하고 접근 권한은 a와 동일하게 설정한다.
    // b가 이미 존재하는 파일인 경우 내용만 삭제하고 열어 a의 내용을 복사해 넣는다.
    // a가 심볼릭 링크인 경우 원본 파일의 내용이 복사된다.
    // 2)
    // `mycp a b`에서 a가 디렉터리인 경우 cp 명령어의 오류 메시지와 동일한 메시지를 출력하고 중단한다.
    // b가 디렉터리인 경우 b 디렉터리 아래 a 이름으로 파일을 복사한다.
    // 이때 b/a 파일의 존재 유무에 따라 새 파일 생성 또는 기존 파일 덮어쓰기 등으로 처리한다.
    // b/a 이름이 존재하나 디렉터리인 경우, cp 명령어의 오류 메시지와 동일한 메시지를 출력하고 중단한다.
    // 4)
    // 인자가 3개 이상인 경우 마지막을 제외한 인자의 파일을 하나씩 열어 마지막 인자(디렉터리)에 복사한다.
    // 오류 및 파일 종류에 따른 처리는 앞과 같다.
    // 마지막 인자가 디렉터리가 아니면 그에 해당하는 오류 메시지를 출력한다.
    // 인자가 둘 이상인 경우
    else if (argc - optind >= 2)
    {
        // 마지막 인자를 제외한 모든 인자를 대상으로 합니다.
        while (argc - optind > 1)
        {
            strcpy(path_buffer, argv[argc - 1]);

            // 마지막 인자를 이름으로 하는 파일이 존재하는 경우
            if (stat(argv[argc - 1], &out_info) == 0)
            {
                switch (out_info.st_mode & S_IFMT)
                {
                    // 마지막 인자가 일반 파일인 경우
                    case S_IFREG:
                        // 인자가 2개가 아닌 여러 개인 경우 파일에 여러 개의 파일을 쓸 수 없으므로 오류를 출력하고 프로그램을 종료합니다.
                        if (argc - optind != 2)
                        {
                            errno = ENOTDIR;
                            fprintf(stderr, "%s: target '%s': %s\n", PROGRAM_NAME, argv[argc - 1], strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                        break;
                    // 마지막 인자가 디렉터리인 경우
                    case S_IFDIR:
                        // 마지막 인자 안에 읽은 파일의 이름과 동일한 파일을 열 수 있도록 경로 버퍼를 수정합니다.
                        // `cp` 역시 심볼릭 링크의 이름을 그대로 사용합니다.
                        strcat(path_buffer, "/");
                        strcat(path_buffer, get_filename(argv[optind]));

                        // 해당 디렉터리 내에 파일이 존재하는 경우
                        if (stat(path_buffer, &out_info) == 0)
                        {
                            switch (out_info.st_mode & S_IFMT)
                            {
                                // 그 파일이 일반 파일인 경우는 아무런 동작도 수행하지 않습니다.
                                case S_IFREG:
                                    break;
                                // 그 파일이 디렉터리인 경우에는 파일을 쓸 수 없으므로 오류를 출력하고 프로그램을 종료합니다.
                                case S_IFDIR:
                                    fprintf(stderr, "%s: cannot overwrite directory '%s' with non-directory\n", PROGRAM_NAME, path_buffer);
                                    ++optind;
                                    errno = 0;
                                    continue;
                                    break;
                                default:
                                    perror("mycp");
                                    exit(EXIT_FAILURE);
                            }
                        }
                        break;
                    default:
                        perror("mycp");
                        exit(EXIT_FAILURE);
                }
                // 해당 파일에 쓸 권한이 있는지 확인합니다.
                if (stat(path_buffer, &out_info) == 0 && access(path_buffer, W_OK) != 0)
                {
                    errno = EACCES;
                    fprintf(stderr, "%s: cannot create regular file '%s': %s\n", PROGRAM_NAME, argv[optind], strerror(errno));
                    
                    // 다음 파일을 복사하기 위해 인덱스를 증가합니다.
                    ++optind;
                    
                    // 발생한 오류를 초기화합니다.
                    errno = 0;

                    continue;
                }
            }
            // 파일의 이름을 인자로 하는 파일이 존재하지 않는 경우
            else
            {
                // 파일의 인자가 2개가 아니라면 디렉터리에 파일을 복사하는 것이어야 하는데
                // 디렉터리가 있어야 하므로 오류를 출력하고 프로그램을 종료합니다.
                if (argc - optind != 2)
                {
                    errno = ENOENT;
                    fprintf(stderr, "%s: target '%s': %s\n", PROGRAM_NAME, argv[argc - 1], strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
            errno = 0;

            // `mycp a b`에서 a가 일반 파일인지, 디렉터리인지 확인합니다.
            // `mycp a b c d e`에서 a,b,c,d가 일반 파일인지, 디렉터리인지 확인합니다.
            // 없는 파일이면 오류를 출력하고 프로그램을 종료합니다.
            if (stat(argv[optind], &src_info) == -1) {
                fprintf(stderr, "%s: cannot stat '%s': %s\n", PROGRAM_NAME, argv[optind], strerror(errno));

                // 다음 파일을 복사하기 위해 인덱스를 증가합니다.
                ++optind;
                
                // 발생한 오류를 초기화합니다.
                errno = 0;

                continue;
            }
            // 일반 파일이 아니면 오류를 출력하고 프로그램을 종료합니다.
            if (!S_ISREG(src_info.st_mode)) {
                errno = EISDIR;
                fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, argv[optind], strerror(errno));

                // 다음 파일을 복사하기 위해 인덱스를 증가합니다.
                ++optind;
                
                // 발생한 오류를 초기화합니다.
                errno = 0;

                continue;
            }

            // 파일을 열기 전에 읽기 권한이 있는지 확인합니다.
            if (access(argv[optind], R_OK) != 0)
            {
                errno = EACCES;
                fprintf(stderr, "%s: cannot open '%s' for reading: %s\n", PROGRAM_NAME, argv[optind], strerror(errno));

                // 파일을 닫습니다.
                close(out_filedes);

                // 다음 파일을 복사하기 위해 인덱스를 증가합니다.
                ++optind;
                
                // 발생한 오류를 초기화합니다.
                errno = 0;

                continue;
            }
            // a를 읽기 모드로 엽니다.
            src_filedes = open(argv[optind], O_RDONLY);
            // 파일 열기에 오류가 발생하면 오류를 출력하고 프로그램을 종료합니다.
            if (errno != 0)
            {
                perror(PROGRAM_NAME);

                // 파일을 닫습니다.
                close(src_filedes);

                // 다음 파일을 복사하기 위해 인덱스를 증가합니다.
                ++optind;
                
                // 발생한 오류를 초기화합니다.
                errno = 0;

                continue;
            }
            // b를 쓰기 모드로 엽니다. 열 때는 파일이 없으면 만들고, 존재하면 내용을 지우고 만듭니다.
            out_filedes = open(path_buffer, O_WRONLY | O_CREAT | O_TRUNC, src_info.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
            // 파일 열기에 오류가 발생하면 오류를 출력하고 프로그램을 종료합니다.
            if (errno != 0)
            {
                perror(PROGRAM_NAME);
                exit(EXIT_FAILURE);
            }

            // 파일을 씁니다.
            write_file(src_filedes, out_filedes, buffer);

            // 파일을 닫습니다.
            close(out_filedes);

            // 파일을 닫습니다.
            close(src_filedes);

            // 다음 파일을 복사하기 위해 인덱스를 증가합니다.
            ++optind;

            // 발생한 오류를 초기화합니다.
            errno = 0;
        }
    }

    return 0;
}

int write_file(const int src_filedes, const int out_filedes, char* const buffer)
{
    // 파일을 얼만큼 읽었는지와 관련된 변수입니다.
    int read_size = -1;

    // 파일을 읽어 buffer에 저장합니다. 읽은 크기가 0 이하이면 읽기를 종료합니다.
    while ((read_size = read(src_filedes, buffer, BUFFER_SIZE)) > 0)
    {
        // 파일에 buffer의 내용을 읽은만큼 씁니다. 쓴 내용이 읽은 크기보다 작으면 오류를 출력하고 프로그램을 종료합니다.
        if (write(out_filedes, buffer, read_size) < read_size)
        {
            fprintf(stderr, "%s: write error\n", PROGRAM_NAME);
            exit(EXIT_FAILURE);
        }
    }
}

char* get_filename(char* const path)
{
    // path에서 마지막 '/'의 위치를 저장합니다.
    char* filename = strrchr(path, '/');
    
    if (filename != NULL)
    {
        // '/' 다음 위치가 파일 이름이기 때문에 포인터를 char형만큼 이동합니다.
        ++filename;
        return filename;
    }
    // 마지막 '/'가 없으면 ., .., 또는 파일 이름만 존재하는 경우로 path 그대로 반환합니다.
    else
    {
        return path;
    }
}