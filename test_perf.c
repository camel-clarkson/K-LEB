#include <stdio.h>
#include <unistd.h>

int main(){
		//printf("my pid: %d\n", getpid());

    int i = 0;
    //while(i<1000000000){ ++i; }
    //while(i<100000000){ ++i; }
    //while(i<10000000){ ++i; }
		//system("echo \"Q\" | openssl s_client -connect google.com:443 -tls1_1");
		//execv("echo \"Q\" | openssl s_client -connect google.com:443 -tls1_1", (char*[]){"test", NULL});
		//system("ls");
		execv("ls", (char*[]){"test", NULL});
		//execv("./openssl.sh", (char*[]){"test", NULL});
}
