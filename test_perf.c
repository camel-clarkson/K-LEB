#include <stdio.h>
#include <unistd.h>

int main(){
	printf("my pid: %d\n", getpid());

    int i = 0;
    //while(i<1000000000){ ++i; }
    //while(i<100000000){ ++i; }
    while(i<10000000){ ++i; }
}
