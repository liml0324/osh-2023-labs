#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#define hello 548
#define buf_len 10  //通过修改buf_len常数来改变buffer的大小

int main()
{
    char buf[buf_len];
    int result = syscall(hello, buf, buf_len);
    if(result == 0)
    {
        printf("%s", buf);
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
