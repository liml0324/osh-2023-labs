#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#define hello 548
#define buf_len 10

int main()
{
    char buf[buf_len];
    int result = syscall(hello, buf, buf_len);
    if(result == 0)
    {
        printf("%s\n", buf);
    }
    else if(result == -1)
    {
        printf("Buffer is not large enough.\n");
    }
    else
    {
        printf("Error!\n");
    }
    while (1) {}
}