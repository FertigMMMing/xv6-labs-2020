#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

int main(int argc,char **argv )
{
    int pp2c[2];
    int pc2p[2];
    pipe(pp2c);
    pipe(pc2p);
    int pid = fork();
    if(pid < 0)
    {
        printf("子进程创建失败");
    }

    if(pid == 0) // 在子进程当中
    {
        char buf;
        
        read(pp2c[0],&buf,1);
        printf("%d: received pong\n",getpid());
        write(pc2p[1],"?",1);
        close(pc2p[1]);
        close(pp2c[0]);
    }else
    {
        char buf;
        write(pp2c[1],".",1);
        close(pp2c[1]);
        
        read(pc2p[0],&buf,1);
        printf("%d: received ping\n",getpid());
        close(pc2p[0]);
        wait(0); // 表示父进程不关心子进程的退出状态，只想等待子进程结束
    }
    
    
    exit(0);
}