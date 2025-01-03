#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include <dirent.h>
#include <pwd.h>
#include <grp.h>

#define PROGRAM_NAME "myls23456"
#define BUFFER_SIZE 4096
#define PATH_BUFFER_SIZE 4096

#define MYLS_OPT_LI (1 << 6)
#define MYLS_OPT_LS (1 << 5)
#define MYLS_OPT_UF (1 << 4)
#define MYLS_OPT_LA (1 << 3)
#define MYLS_OPT_LL (1 << 2)
#define MYLS_OPT_UR (1 << 1)
#define MYLS_OPT_LR 1

struct fileinfo
{
    struct dirent dir_entry;
    struct stat file_info;
};

int get_digits(long int n);
long int read_directory(
    char dir_path_buffer[PATH_BUFFER_SIZE],
    char file_path_buffer[PATH_BUFFER_SIZE],
    struct fileinfo **dir_files,
    long int *file_buffer_capacity,
    int opt_flags
);
int reverse_compare(const void *a, const void *b);
void print_file_info(char* dir_path_buffer, char* file_path_buffer, struct fileinfo *fileinfo, int opt_flags);


int main(int argc, char* argv[])
{
    // 옵션 처리와 관련된 변수입니다.
    int opt = 0;
    int n_args = 0;
    int opt_flags = 0;

    struct fileinfo *dir_files = malloc(sizeof(struct fileinfo) * BUFFER_SIZE);
    long int file_buffer_capacity = BUFFER_SIZE;

    char dir_path_buffer[PATH_BUFFER_SIZE];
    char file_path_buffer[PATH_BUFFER_SIZE];

    // 2) -a, -i, -s, -F 옵션 기능을 추가한다. –ais와 같이 여러 옵션을 동시에 적용할 수 있어야 한다.
    // 옵션을 처리합니다.
    while ((opt = getopt(argc, argv, "isFalRr")) != -1)
    {
        switch (opt)
        {
            case 'i':
                opt_flags = opt_flags | MYLS_OPT_LI;
                break;
            case 's':
                opt_flags = opt_flags | MYLS_OPT_LS;
                break;
            case 'F':
                opt_flags = opt_flags | MYLS_OPT_UF;
                break;
            case 'a':
                opt_flags = opt_flags | MYLS_OPT_LA;
                break;
            case 'l':
                opt_flags = opt_flags | MYLS_OPT_LL;
                break;
            case 'R':
                opt_flags = opt_flags | MYLS_OPT_UR;
                break;
            case 'r':
                opt_flags = opt_flags | MYLS_OPT_LR;
                break;
            default:
                if (opterr)
                {
                    opterr = 0;
                }
                else
                {
                    perror(PROGRAM_NAME);
                    errno = 0;
                }
                exit(EXIT_FAILURE);
        }
    }

    // 1) 인자가 없으면 현재 디렉터리에서 숨은 파일을 제외하고 파일의 이름을 출력한다.
    //    인자가 하나 이상 있으면 해당 인자가 정상적인 디렉터리인 경우 그 디렉터리의 목록을 출력한다.
    //    이때 출력 순서는 디렉터리 수록 순서대로 하고 한 줄에 한 이름씩 출력한다.
    n_args = argc - optind;
    while (argc - optind >= 0)
    {
        // 모든 인자를 탐색했으면 중단합니다.
        if (n_args > 0 && argc - optind == 0)
        {
            break;
        }

        if (n_args > 1)
        {
            if (n_args != argc - optind)
            {
                printf("\n");
            }
            printf("%s:\n", argv[optind]);
        }

        // 현재 디렉터리를 경로 버퍼에 저장합니다.
        if (n_args == 0)
        {
            strcpy(dir_path_buffer, ".");
        }
        else
        {
            strcpy(dir_path_buffer, argv[optind]);
        }

        read_directory(dir_path_buffer, file_path_buffer, &dir_files, &file_buffer_capacity, opt_flags);

        ++optind;
    }

    free(dir_files);
    
    return 0;
}

int get_digits(long int n)
{
    int res = 0;

    while (n > 0)
    {
        n = n / 10;
        ++res;
    }

    return res;
}

long int read_directory(
    char dir_path_buffer[PATH_BUFFER_SIZE],
    char file_path_buffer[PATH_BUFFER_SIZE],
    struct fileinfo **dir_files,
    long int *file_buffer_capacity,
    int opt_flags
)
{
    DIR *dirp;
    struct dirent *dir_entry;
    
    long int iter = 0;

    long int n_files = 0;
    long int n_blocks = 0;
    long int n_print_files = 0;
    short int col_print_size = 0;
    struct winsize w;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
        perror("ioctl");
        exit(EXIT_FAILURE);
    }

    struct fileinfo dirinfo;
    if (stat(dir_path_buffer, &dirinfo.file_info) == -1)
    {
        perror(PROGRAM_NAME);
        errno = 0;
        return -1;
    }
    else
    {
        if (!S_ISDIR(dirinfo.file_info.st_mode))
        {
            strcpy(dirinfo.dir_entry.d_name, dir_path_buffer);
            print_file_info(dir_path_buffer, file_path_buffer, &dirinfo, opt_flags);
            printf("\n");
            return 0;
        }
    }

    // 경로 버퍼의 경로의 디렉터리가 읽기 가능한지 확인합니다.
    if (access(dir_path_buffer, R_OK))
    {
        errno = EACCES;
        fprintf(stderr, "%s: cannot open directory '%s': %s\n", PROGRAM_NAME, dir_path_buffer, strerror(errno));
        errno = 0;
        return -1;
    }

    // 디렉터리를 엽니다.
    dirp = opendir(dir_path_buffer);

    // 디렉터리 항목을 하나씩 읽어옵니다.
    while ((dir_entry = readdir(dirp)) != NULL)
    {
        // 옵션 -a가 설정되지 않았으면 ., .., 숨김 파일(이름이 .으로 시작하는 파일)은 건너뜁니다.
        if (!(opt_flags & MYLS_OPT_LA) && dir_entry->d_name[0] == '.')
        {
            continue;
        }

        while (n_files * sizeof(struct fileinfo) >= *file_buffer_capacity)
        {
            *file_buffer_capacity = *file_buffer_capacity * 2;
            *dir_files = realloc(*dir_files, sizeof(struct fileinfo) * (*file_buffer_capacity));
            if (dir_files == NULL)
            {
                fprintf(stderr, "%s: realloc error!", PROGRAM_NAME);
                exit(EXIT_FAILURE);
            }
        }

        struct fileinfo fileinfo;
        fileinfo.dir_entry = *dir_entry;

        strcpy(file_path_buffer, dir_path_buffer);
        strcat(file_path_buffer, "/");
        strcat(file_path_buffer, dir_entry->d_name);

        if (lstat(file_path_buffer, &fileinfo.file_info) == -1)
        {
            perror(PROGRAM_NAME);
            errno = 0;
            continue;
        }

        (*dir_files)[n_files] = fileinfo;

        n_blocks = n_blocks + fileinfo.file_info.st_blocks / 2;

        ++n_files;
    }

    // 5) 정렬 기능과 –r 옵션을 추가한다. –r 옵션은 파일 이름의 역순으로 정렬하여 출력하는 것이다.
    if (opt_flags & MYLS_OPT_LR)
    {
        qsort(*dir_files, n_files, sizeof(struct fileinfo), reverse_compare);
    }

    // 옵션 -l 또는 -s가 설정돼 있으면 전체 블록 수를 출력합니다.
    if (opt_flags & (MYLS_OPT_LL | MYLS_OPT_LS))
    {
        printf("total %ld\n", n_blocks);
    }

    for (iter = 0; iter < n_files; ++iter)
    {
        int current_file_length = strlen((*dir_files)[iter].dir_entry.d_name);
        
        if (opt_flags & MYLS_OPT_LI)
        {
            current_file_length += get_digits((*dir_files)[iter].file_info.st_ino) + 1;
        }
        if (opt_flags & MYLS_OPT_LS)
        {
            current_file_length += get_digits((*dir_files)[iter].file_info.st_blocks / 2) + 1;
        }
        if (opt_flags & MYLS_OPT_UF)
        {
            current_file_length += 1;
        }

        current_file_length += 3;

        // 옵션 -l이 설정되었으면 개행을, 그렇지 않으면 띄어쓰기를 출력합니다.
        if (n_print_files > 0)
        {
            if (opt_flags & MYLS_OPT_LL)
            {
                printf("\n");
            }
            else
            {
                // 6) 터미널의 가로 크기에 맞추어 한 줄에 출력하는 이름의 수를 조절하도록 한다.
                if (col_print_size + current_file_length > w.ws_col)
                {
                    printf("\n");
                    col_print_size = 0; // 새 줄이 시작되므로 초기화
                }
                else
                {
                    printf("   "); // 각 파일 간 띄어쓰기
                }
            }
        }

        // 파일 정보 출력
        print_file_info(dir_path_buffer, file_path_buffer, &(*dir_files)[iter], opt_flags);

        // 현재 파일 길이를 누적하여 `col_print_size` 갱신
        col_print_size += current_file_length;

        ++n_print_files;
    }
    printf("\n");

    // 4) -R 기능을 추가한다.
    if (opt_flags & MYLS_OPT_UR)
    {
        for (iter = 0; iter < n_files; ++iter)
        {
            // 서브 디렉터리인지 확인합니다.
            if (S_ISDIR((*dir_files)[iter].file_info.st_mode))
            {
                // ., .. 디렉터리는 건너뜁니다.
                if (strcmp((*dir_files)[iter].dir_entry.d_name, ".") == 0 || strcmp((*dir_files)[iter].dir_entry.d_name, "..") == 0)
                {
                    continue;
                }

                strcpy(file_path_buffer, dir_path_buffer);
                strcat(file_path_buffer, "/");
                strcat(file_path_buffer, (*dir_files)[iter].dir_entry.d_name);

                if (access(file_path_buffer, R_OK) == -1)
                {
                    fprintf(stderr, "%s: cannot open directory '%s': %s\n", PROGRAM_NAME, file_path_buffer, strerror(errno));
                    errno = 0;
                    continue;
                }

                char subdir_path_buffer[PATH_BUFFER_SIZE];
                struct fileinfo *subdir_files = malloc(sizeof(struct fileinfo) * BUFFER_SIZE);
                strcpy(subdir_path_buffer, file_path_buffer);

                printf("\n%s:\n", subdir_path_buffer);
                read_directory(subdir_path_buffer, file_path_buffer, &subdir_files, file_buffer_capacity, opt_flags);

                free(subdir_files);
            }
        }
    }

    closedir(dirp);

    return n_files;
}

int reverse_compare(const void *a, const void *b)
{
    return strcmp(((struct fileinfo*) b)->dir_entry.d_name, ((struct fileinfo*) a)->dir_entry.d_name);
}

void print_file_info(char* dir_path_buffer, char* file_path_buffer, struct fileinfo *fileinfo, int opt_flags)
{
    char ctime_str[26];

    // 옵션 -i가 설정되었으면 아이노드 번호를 출력합니다.
    if (opt_flags & MYLS_OPT_LI)
    {
        printf("%ld ", fileinfo->file_info.st_ino);
    }

    // 옵션 -l 또는 -s가 설정되었으면 블록 크기를 출력합니다.
    if (opt_flags & MYLS_OPT_LS)
    {
        printf("%ld ", fileinfo->file_info.st_blocks / 2);
    }

    // 3) -l 기능을 추가한다.
    // 옵션 -l이 설정되었으면 상세 정보를 출력합니다.
    if (opt_flags & MYLS_OPT_LL)
    {
        // 권한을 출력합니다.
        switch (fileinfo->file_info.st_mode & S_IFMT)
        {
            case S_IFDIR:
                printf("d");
                break;
            case S_IFLNK:
                printf("l");
                break;
            case S_IFSOCK:
                printf("s");
                break;
            case S_IFBLK:
                printf("b");
                break;
            case S_IFCHR:
                printf("c");
                break;
            default:
                printf("-");
        }
        printf("%c", (fileinfo->file_info.st_mode & S_IRUSR) ? 'r' : '-');
        printf("%c", (fileinfo->file_info.st_mode & S_IWUSR) ? 'w' : '-');
        printf("%c", (fileinfo->file_info.st_mode & S_IXUSR) ? 'x' : '-');
        printf("%c", (fileinfo->file_info.st_mode & S_IRGRP) ? 'r' : '-');
        printf("%c", (fileinfo->file_info.st_mode & S_IWGRP) ? 'w' : '-');
        printf("%c", (fileinfo->file_info.st_mode & S_IXGRP) ? 'x' : '-');
        printf("%c", (fileinfo->file_info.st_mode & S_IROTH) ? 'r' : '-');
        printf("%c", (fileinfo->file_info.st_mode & S_IWOTH) ? 'w' : '-');
        printf("%c", (fileinfo->file_info.st_mode & S_IXOTH) ? 'x' : '-');
        printf(" ");

        // 하드 링크 개수를 출력합니다.
        printf("%ld ", fileinfo->file_info.st_nlink);

        // 소유자를 출력합니다.
        printf("%s ", getpwuid(fileinfo->file_info.st_uid)->pw_name);

        // 그룹을 출력합니다.
        printf("%s ", getgrgid(fileinfo->file_info.st_gid)->gr_name);

        // 바이트 단위 크기를 출력합니다.
        printf("%ld ", fileinfo->file_info.st_size);

        // 생성 날짜를 출력합니다.
        strcpy(ctime_str, ctime(&(fileinfo->file_info.st_mtime)));
        ctime_str[strlen(ctime_str) - 1] = '\0';
        strcpy(ctime_str, ctime_str + 4);
        printf("%s ", ctime_str);
    }

    printf("%s", fileinfo->dir_entry.d_name);

    // 옵션 -l이 설정되었으면 심볼릭 링크일 때 원본 경로도 출력합니다.
    if ((opt_flags & MYLS_OPT_LL) && S_ISLNK(fileinfo->file_info.st_mode))
    {
        strcpy(file_path_buffer, dir_path_buffer);
        strcat(file_path_buffer, "/");
        strcat(file_path_buffer, fileinfo->dir_entry.d_name);

        int read_size = 0;
        if ((read_size = readlink(file_path_buffer, file_path_buffer, PATH_BUFFER_SIZE)) == -1)
        {
            perror(PROGRAM_NAME);
            exit(EXIT_FAILURE);
        }
        file_path_buffer[read_size] = '\0';

        printf(" -> %s", file_path_buffer);
    }

    // 옵션 -F가 설정되었으면 파일 형식을 출력합니다.
    if (opt_flags & MYLS_OPT_UF)
    {
        if (S_ISDIR(fileinfo->file_info.st_mode))
        {
            printf("/");
        }
        else if (S_ISREG(fileinfo->file_info.st_mode))
        {
            if ((fileinfo->file_info.st_mode) & (S_IXUSR | S_IXGRP | S_IXOTH))
            {
                printf("*");
            }
        }
        else if (!(opt_flags & MYLS_OPT_LL) && S_ISLNK(fileinfo->file_info.st_mode))
        {
            printf("@");
        }
        else if (S_ISSOCK(fileinfo->file_info.st_mode))
        {
            printf("=");
        }
        else if (S_ISFIFO(fileinfo->file_info.st_mode))
        {
            printf("|");
        }
    }
}