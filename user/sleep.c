#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"

int main(int args,char ** argc)
{
    if(args <2)
    {
        printf("need more paga, ./sleep time");
    }
    sleep(atoi(argc[1]));
    exit(0);
}