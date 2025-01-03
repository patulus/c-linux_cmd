#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define PROGRAM_NAME "mysh"
#define BUFFER_SIZE 4096

#define HISTSIZE 20

#define STR_LEN 1024  // 최대 명령 입력 길이
#define MAX_TOKENS 10 // 공백으로 분리되는 명령어 단위 수

struct CMD {
    char *name;                         // 명령어 이름
    char *desc;                         // 명령 설명
    int (*cmd)(int argc, char *argv[]); // 명령 실행 함수에 대한 포인터
};

int cmd_cd(int argc, char *argv[]);
int cmd_pwd(int argc, char *argv[]);
int cmd_exit(int argc, char *argv[]);
int cmd_help(int argc, char *argv[]);
int cmd_history(int argc, char *argv[]);
int cmdProcessing(void);

struct CMD builtin[] = {
    { "cd", "change the shell working directory", cmd_cd },
    { "pwd", "print the name of the currrent working directory", cmd_pwd },
    { "history", "display or manipulate the history list", cmd_history },
    { "help", "display infomation about builtin commands", cmd_help },
    { "exit", "exit the shell", cmd_exit }
};
const int builtins = sizeof(builtin) / sizeof(struct CMD); // 내장 명령의 개수

char hist_file[BUFFER_SIZE];
char* hist[HISTSIZE];
int hist_front;
int hist_rear;
int hist_size;

void hist_enqueue(char* cmd);
char* hist_dequeue(void);
int load_hist(char* hist_file);
int save_hist(char* hist_file);

char buffer[BUFFER_SIZE];
char prev_path[BUFFER_SIZE];
char curr_path[BUFFER_SIZE];

struct sigaction act;

void int_handler(int signum) {}

int main(void)
{
    int isExit = 0;

    // 경로 버퍼를 초기화합니다.
    strcpy(hist_file, getenv("HOME"));
    strcat(hist_file, "/.mysh_history");
    getcwd(prev_path, BUFFER_SIZE);
    getcwd(curr_path, BUFFER_SIZE);

    hist_front = 0;
    hist_rear = -1;
    hist_size = 0;
    load_hist(hist_file);

    act.sa_handler = int_handler;
    sigfillset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);
    
    while (!isExit)
    {
        // 명령 처리 루틴을 호출합니다.
        isExit = cmdProcessing();
    }

    save_hist(hist_file);
    
    fputs("마이 셸을 종료합니다.\n", stdout);
    return 0;
}

int cmdProcessing(void)
{
    char cmdLine[STR_LEN];       // 입력 명령 전체를 저장하는 배열
    char *cmdTokens[MAX_TOKENS]; // 입력 명령을 공백으로 분리하여 저장하는 배열
    char delim[] = " \t\n\r";    // 토큰 구분자 - strtok에서 사용
    char *token;                 // 하나의 토큰을 분리하는데 사용
    int tokenNum;                // 입력 명령에 저장된 토큰 수
    int exitCode = 0;            // 종료 코드 (default = 0)

    int i;        // 반복 횟수

    fputs("[mysh v0.1] $ ", stdout); // 프롬프트 출력
    fgets(cmdLine, STR_LEN, stdin);  // 프롬프트 입력

    if (cmdLine[0] != '\n')
    {
        char* hist_cmdLine = malloc(sizeof(char) * (strlen(cmdLine) + 1));
        strcpy(hist_cmdLine, cmdLine);
        hist_enqueue(hist_cmdLine);
    }
    
    // 명령줄을 토큰 단위로 분리
    tokenNum = 0;
    token = strtok(cmdLine, delim);
    while (token)
    {
        // 분리된 토큰을 cmdTokenList 배열에 복사함
        cmdTokens[tokenNum++] = token;
        token = strtok(NULL, delim); // 연속 호출 시 NULL 설정해야 함
    }
    cmdTokens[tokenNum] = NULL; // 명령 끝 표시
    
    for (i = 0; i < builtins; ++i)
    {
        if (strcmp(cmdTokens[0], builtin[i].name) == 0)
        {
            // 명령 문자열 수와 문자열 포인터에 대한 배열을 인자로 하여 실행 함수 호출
            return builtin[i].cmd(tokenNum, cmdTokens);
        }
    }
    
    int child;
    child = fork(); // 내장 명령이 아닌 경우 새 프로세스 생성
    if (child > 0) // 부모인 경우 자식의 실행을 기다린다.
    {
        act.sa_handler = SIG_IGN;
        sigaction(SIGINT, &act, NULL);

        // 부모 프로세스인 경우 자식 프로세스의 실행이 완료될 때까지 대기합니다.
        waitpid(child, &exitCode, 0);
        // 자식 프로세스의 종료 상태의 하위 1바이트가 부모 프로세스에서 넘긴 변수의 3바이트 위치에 저장됩니다.
        // 따라서 8비트 우측 시프트합니다.
        exitCode >>= 8;

        act.sa_handler = int_handler;
        sigaction(SIGINT, &act, NULL);
    }
    else if (child == 0) // 자식은 해당 명령을 실행한다.
    {
        // 해당 명령을 수행합니다.
        if (execvp(cmdTokens[0], cmdTokens) == -1);
        {
            fprintf(stderr, "%s: %s: command not found\n", PROGRAM_NAME, cmdTokens[0]);
            exitCode = 1;
        }
    }
    else
    {
        fprintf(stderr, "%s: fork failed: %s\n", PROGRAM_NAME, strerror(errno));
        exitCode = 1;
    }

    return exitCode;
}

#define MYSH_CMD_CD_OPT_UL (1 << 0) // -L
#define MYSH_CMD_CD_OPT_UP (1 << 1) // -P
#define MYSH_CMD_CD_OPT_LE (1 << 2) // -e
#define MYSH_CMD_CD_OPT_AT (1 << 3) // -@

int cmd_cd(int argc, char *argv[])
{
    int opt;
    int opt_flags = 0;

    char new_path[BUFFER_SIZE];

    optind = 1;

    // 옵션을 처리합니다.
    while ((opt = getopt(argc, argv, "LPe@")) != -1)
    {
        switch (opt)
        {
            // 심볼릭 링크를 따라가도록 강제하는 옵션입니다.
            // 심볼릭 링크 접근 후 ..시 옵션이 없으면 실제 디렉터리의 부모 디렉터리에, 있으면 심볼릭 링크가 위치한 디렉터리에 접근합니다.
            // bash에서는 기본으로 적용되는 옵션입니다.
            case 'L':
                opt_flags |= MYSH_CMD_CD_OPT_UL;
                fprintf(stderr, "cd: 옵션 -L은 아직 구현되지 않았습니다.\n");
                break;
            // 심볼릭 링크를 따라가지 않도록 강제하는 옵션입니다.
            // myshell에서는 -L 옵션의 구현이 되지 않아 bash와 다르게 기본으로 적용되는 옵션입니다.
            case 'P':
                opt_flags |= MYSH_CMD_CD_OPT_UP;
                break;
            // 심볼릭 링크의 대상이 없을 때 0이 아닌 상태 코드를 반환합니다.
            case 'e':
                opt_flags |= MYSH_CMD_CD_OPT_LE;
                fprintf(stderr, "cd: 옵션 -e은 아직 구현되지 않았습니다.\n");
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
                return 0;
        }
    }

    // 인자가 주어지지 않으면 홈 디렉터리로 이동합니다.
    if (argc - optind < 1)
    {
        getcwd(prev_path, BUFFER_SIZE);
        if (chdir(getenv("HOME")) == -1)
        {
            fprintf(stderr, "%s: %s: HOME not set\n", PROGRAM_NAME, argv[optind]);
            errno = 0;
        }
        getcwd(curr_path, BUFFER_SIZE);
    }
    // 인자가 둘 이상이면 인자가 너무 많다는 오류를 출력합니다.
    else if (argc - optind > 1)
    {
        fprintf(stderr, "%s: %s: too many arguments\n", PROGRAM_NAME, argv[0]);
    }
    // 인자가 하나면 인자의 디렉터리로 이동합니다.
    else
    {
        // 인자가 "~"이면 현재 디렉터리로 이동합니다.
        if (argv[optind][0] == '~')
        {
            getcwd(prev_path, BUFFER_SIZE);
            if (argv[optind][1] == '/' || argv[optind][1] == '\0')
            {
                if (chdir(getenv("HOME")) == -1)
                {
                    fprintf(stderr, "%s: %s: HOME not set\n", PROGRAM_NAME, argv[optind]);
                    errno = 0;
                    return 0;
                }
            }
            else
            {
                strcpy(new_path, "/home/");
                strcat(new_path, (argv[optind] + 1));
                if (chdir(new_path) == -1)
                {
                    fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, argv[optind], strerror(errno));
                    errno = 0;
                    return 0;
                }
            }
            getcwd(curr_path, BUFFER_SIZE);
        }
        // 인자가 "-"이면 이전 경로로 이동합니다.
        else if (argv[optind][0] == '-')
        {
            if (chdir(prev_path) == -1)
            {
                fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, argv[optind], strerror(errno));
                errno = 0;
                return 0;
            }
            strcpy(prev_path, curr_path);
            getcwd(curr_path, BUFFER_SIZE);
        }
        // 디렉터리를 이동합니다. 실패 시 오류를 출력합니다.
        else
        {
            getcwd(prev_path, BUFFER_SIZE);
            if (chdir(argv[optind]) == -1)
            {
                fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, argv[optind], strerror(errno));
                errno = 0;
                return 0;
            }
            getcwd(curr_path, BUFFER_SIZE);
        }
    }

    getcwd(curr_path, BUFFER_SIZE);
    setenv("PWD", curr_path, 1);

    return 0;
}

#define MYSH_CMD_CD_PWD_UL (1 << 0) // -L
#define MYSH_CMD_CD_PWD_UP (1 << 1) // -P

int cmd_pwd(int argc, char *argv[])
{
    int opt;
    int opt_flag = 0;

    optind = 1;

    while ((opt = getopt(argc, argv, "LP")) != -1)
    {
        switch (opt)
        {
            // 환경변수 $PWD의 값을 표시합니다. 이때 심볼릭 링크 위치를 표시합니다.
            // myshell에서는 cd에서 해당 기능을 아직 구현하지 않아 -P와 같은 값이 출력됩니다.
            case 'L':
                opt_flag = 0;
                break;
            // 심볼릭 링크로 접근 후 명령 시 실제 디렉터리의 경로를 표시합니다.
            case 'P':
                opt_flag = 1;
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
                return 0;
        }
    }

    if (!opt_flag)
    {
        char *env_path;
        if ((env_path = getenv("PWD")) == NULL)
        {
            fprintf(stderr, "%s: %s: PWD not set\n", PROGRAM_NAME, argv[optind]);
            if (getcwd(curr_path, BUFFER_SIZE) != NULL) 
            {
                fprintf(stdout, "%s\n", curr_path);
            }
            return 0;
        }
        fprintf(stderr, "%s\n", getenv("PWD"));
    }
    else
    {
        if (getcwd(curr_path, BUFFER_SIZE) != NULL) 
        {
            fprintf(stdout, "%s\n", curr_path);
        }
    }

    return 0;
}

int cmd_exit(int argc, char *argv[])
{
    // isExit를 활성화합니다.
    return 1;
}

int cmd_help(int argc, char *argv[])
{
    int opt;

    int i;             // 반복 횟수
    int cnt_print = 0; // 출력 횟수

    optind = 1;

    // 옵션을 처리합니다.
    while ((opt = getopt(argc, argv, "")) != -1)
    {
        // 옵션을 무시합니다.
    }

    // 인자가 주어지지 않으면 명령어 목록을 출력합니다.
    if (argc - optind < 1)
    {
        fprintf(stdout, "%s, version %s (%s)\n", PROGRAM_NAME, "0.0.1", "x86_64-pc-linux-gnu");
        fprintf(stdout, "These shell commands are defined internally. Type `help' to see this list.\n");
        fprintf(stdout, "Type `help name' to find out more about the function `name'\n");
        fprintf(stdout, "Use `man -k' or `info' to find out more about commands not in this list.\n");
        fprintf(stdout, "\n");

        for (i = 0; i < builtins; ++i)
        {
            fprintf(stdout, "  %s: %s\n", builtin[i].name, builtin[i].desc);
        }
    }
    // 인자가 하나 이상 있는 경우 내장 명령의 도움말을 출력합니다.
    else
    {
        // 해당하는 내장 명령이 있는지 살펴봅니다.
        for (; optind < argc; ++optind)
        {
            for (i = 0; i < builtins; ++i)
            {
                if (!strcmp(argv[optind], builtin[i].name))
                {
                    fprintf(stdout, "%s: %s\n", builtin[i].name, builtin[i].desc);
                    ++cnt_print;
                }
            }
        }

        // 내장 명령이 없으면 친절히 안내합니다.
        if (cnt_print == 0)
        {
            --optind;
            fprintf(stderr, "%s: %s: no help topics match `%s'. Try `help help' or `man -k %s', `info %s'.\n", PROGRAM_NAME, argv[0], argv[optind], argv[optind], argv[optind]);
        }
    }

    return 0;
}

void hist_enqueue(char* cmd)
{
    if (hist_size == HISTSIZE)
    {
        free(hist[hist_front]);
        hist[hist_front] = NULL;
        hist_front = (hist_front + 1) % HISTSIZE;
        --hist_size;
    }

    hist_rear = (hist_rear + 1) % HISTSIZE;
    hist[hist_rear] = cmd;
    ++hist_size;
}

char* hist_dequeue(void)
{
    if (hist_size == 0)
    {
        return NULL;
    }

    char* cmd = hist[hist_front];
    hist[hist_front] = NULL;
    hist_front = (hist_front + 1) % HISTSIZE;
    --hist_size;

    return cmd;
}

int load_hist(char* hist_file)
{
    FILE *ptr_hist_file = fopen(hist_file, "rb");
    if (ptr_hist_file == NULL)
    {
        return 1;
    }

    while ((fgets(buffer, BUFFER_SIZE, ptr_hist_file)) != NULL)
    {
        char *cmd = malloc(sizeof(char) * (strlen(buffer) + 1));
        strcpy(cmd, buffer);
        hist_enqueue(cmd);
    }

    fclose(ptr_hist_file);

    return 0;
}

int save_hist(char* hist_file)
{
    char *cmd = NULL;

    FILE *ptr_hist_file = fopen(hist_file, "wb");
    if (ptr_hist_file == NULL)
    {
        return 1;
    }

    while (hist_size > 0)
    {
        cmd = hist_dequeue();
        if (cmd == NULL) {
            break;
        }
        fputs(cmd, ptr_hist_file);
        free(cmd);
    }

    fclose(ptr_hist_file);

    return 0;
}

#define MYSH_CMD_HISTORY_LC (1 << 0) // -c
#define MYSH_CMD_HISTORY_LD (1 << 1) // -d
#define MYSH_CMD_HISTORY_LA (1 << 2) // -a
#define MYSH_CMD_HISTORY_LN (1 << 3) // -n
#define MYSH_CMD_HISTORY_LR (1 << 4) // -r
#define MYSH_CMD_HISTORY_LW (1 << 5) // -w
#define MYSH_CMD_HISTORY_LP (1 << 6) // -p
#define MYSH_CMD_HISTORY_LS (1 << 7) // -s

int cmd_history(int argc, char *argv[])
{
    int opt;
    int opt_flags = 0;
    int offset = 0;

    optind = 1;

    while ((opt = getopt(argc, argv, "cd:anrwps")) != -1)
    {
        switch (opt)
        {
            // 불러온 히스토리를 초기화합니다. 파일은 초기화되지 않습니다.
            case 'c':
                opt_flags |=  MYSH_CMD_HISTORY_LC;
                break;
            // 불러온 히스토리에서 offset만큼 지웁니다.
            case 'd':
                opt_flags |=  MYSH_CMD_HISTORY_LD;
                if ((offset = atoi(optarg)) == 0 || offset < 0 || offset > HISTSIZE)
                {
                    fprintf(stderr, "%s: history: %s: history position out of range\n", PROGRAM_NAME, optarg);
                    return 0;
                }
                break;
            // 히스토리 파일에 추가된 히스토리 줄을 추가합니다.
            case 'a':
                opt_flags |=  MYSH_CMD_HISTORY_LA;
                fprintf(stderr, "cd: 옵션 -a는 아직 구현되지 않았습니다.\n");
                break;
            // 히스토리 파일에서 아직 읽지 않은 줄을 불러옵니다.
            case 'n':
                opt_flags |=  MYSH_CMD_HISTORY_LN;
                fprintf(stderr, "cd: 옵션 -n은 아직 구현되지 않았습니다.\n");
                break;
            // 히스토리 파일을 불러와 추가합니다.
            case 'r':
                opt_flags |=  MYSH_CMD_HISTORY_LR;
                fprintf(stderr, "cd: 옵션 -r은 아직 구현되지 않았습니다.\n");
                break;
            // 현재 히스토리 내용을 파일에 씁니다.
            case 'w':
                opt_flags |=  MYSH_CMD_HISTORY_LW;
                break;
            case 'p':
                opt_flags |=  MYSH_CMD_HISTORY_LP;
                fprintf(stderr, "cd: 옵션 -p는 아직 구현되지 않았습니다.\n");
                break;
            case 's':
                opt_flags |=  MYSH_CMD_HISTORY_LS;
                fprintf(stderr, "cd: 옵션 -s는 아직 구현되지 않았습니다.\n");
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
                return 0;
        }
    }

    if (hist_size <= 0)
    {
        return 0;
    }
    
    if (argc - optind > 1)
    {
        fprintf(stderr, "%s: history: too many arguments\n", PROGRAM_NAME);
        return 0;
    }

    if (opt_flags & MYSH_CMD_HISTORY_LC)
    {
        char *cmd;
        while (hist_size > 0)
        {
            cmd = hist_dequeue();
            if (cmd == NULL) {
                break;
            }
            free(cmd);
        }

        hist_front = 0;
        hist_rear = -1;
        
        return 0;
    }
    if (opt_flags & MYSH_CMD_HISTORY_LD)
    {
        char *cmd;
        for (int i = 0; i < offset; ++i)
        {
            cmd = hist_dequeue();
            if (cmd == NULL) {
                break;
            }
            free(cmd);
        }
        return 0;
    }
    if (opt_flags & MYSH_CMD_HISTORY_LW)
    {
        save_hist(hist_file);
        load_hist(hist_file);
        return 0;
    }

    if (hist_size <= 0)
    {
        return 0;
    }

    int cnt_print = 1;
    int hist_view = hist_front;
    int hist_total = hist_size - 1;
    if (argc - optind == 1)
    {
        if (atoi(argv[optind]) == 0)
        {
            fprintf(stderr, "%s: history: %s: numeric argument required\n", PROGRAM_NAME, argv[optind]);
            return 0;
        }
        hist_view = (hist_rear - atoi(argv[optind]) + 1 + HISTSIZE) % HISTSIZE;
        cnt_print = hist_size - atoi(argv[optind]) + 1;
    }

    while (cnt_print <= hist_size)
    {
        fprintf(stdout, "  %2d %s", cnt_print++, hist[hist_view]);
        hist_view = (hist_view + 1) % HISTSIZE;
    }

    return 0;
}