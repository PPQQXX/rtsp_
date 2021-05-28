#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <string.h>

// 功能：获取进程已打开文件的fd
// 方法：解析/proc/xxx/fd目录

int openfile_to_fd(const char *target, int pid) {
    char fdpath[32];
    sprintf(fdpath, "/proc/%d/fd", pid);

    DIR *dir = opendir(fdpath);
    if (dir == NULL) {
        printf("failed to open %s\n", fdpath);
        return -1;
    }

    int res = -1;
    struct dirent *ptr = NULL;
    while ( (ptr = readdir(dir)) != NULL) {
        // 跳过文件"."和".."
        if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0) {
            continue;
        }
        else if (ptr->d_type == 10) {   
            char linkpath[128], filepath[256];
            sprintf(linkpath, "%s/%s", fdpath, ptr->d_name);
            if (readlink(linkpath, filepath, sizeof(filepath)) < 0) {// 获取fd对应的文件路径
                printf("failed to readlink %s\n", linkpath);
                continue;
            }
            char *filename = basename(filepath);  // 解析路径，获取文件名 
            if (strcmp(filename, target) == 0) {
                sscanf(ptr->d_name, "%d", &res);  // /proc/xxx/fd下的链接文件名其实就是fd大小 
                break;                    
            } 
        }
    }
    closedir(dir);
    return res;
}


int main(int argc, char const* argv[])
{
    int fd1 = open(argv[1], O_RDONLY);
    int fd2 = open(argv[2], O_RDONLY);
    printf("pid:%d fd1:%d fd2:%d\n", getpid(), fd1, fd2);
    printf("openfile_to_fd:\n");
    printf("%s --> %d\n", argv[1], openfile_to_fd(argv[1], getpid()));
    printf("%s --> %d\n", argv[2], openfile_to_fd(argv[2], getpid()));
    return 0;
}

