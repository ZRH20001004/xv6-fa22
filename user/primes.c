#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


void prime(int* p0){
    close(p0[1]);
    int d;
    if(read(p0[0], &d, 4)){
        printf("prime %d\n", d);
    }else{
        close(p0[0]);
        exit(0);
    }
    int p1[2];
    pipe(p1);
    if(fork() == 0){
        prime(p1);
    }else{
        int n;
        close(p1[0]);
        while(read(p0[0], &n, 4)){
            if(n % d){
                write(p1[1], &n, 4);
            }
        }
        close(p1[1]);
        close(p0[0]);
        wait((int*)0);
    }
    exit(0);
}

int
main(int argc, char *argv[])
{
    int p0[2];
    pipe(p0);
    if(fork() == 0){
        prime(p0);
    }else{
        close(p0[0]);
        int n;
        for(n=2; n<=35; n++){
            write(p0[1], &n, 4);
        }
        close(p0[1]);
        wait((int*)0);
    }
    exit(0);
}