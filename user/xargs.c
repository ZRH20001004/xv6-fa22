#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h" 
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if(argc <= 1){
        fprintf(2, "usage:xargs command ...\n");
        exit(1);
    }
    char *argcv[MAXARG];
    int p;
    for(p=1; p<argc; p++){
        argcv[p-1] = argv[p];
    }
    p--;
    char buf[1024];
    int t = 0;
    while(read(0, buf + t, 1)){
        t++;
    }
    int i = 0;
    int j = 0;
    int k = p;
    while(j < t){
        if(buf[j] == ' '){
            buf[j] = 0;
            argcv[k++] = buf+i;
            i = j + 1;
        }
        if(buf[j] == '\n'){
            buf[j] = 0;
            argcv[k++] = buf+i;
            argcv[k] = 0;
            if(fork() == 0){
                exec(argv[1], argcv);
                fprintf(2, "exec fail\n");
                exit(1);
            }
            wait((int *)0);
            k = p;
            i = j + 1;
        }
        j++;
    }
    exit(0);
}