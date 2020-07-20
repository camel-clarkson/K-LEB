/*** Simple Test Program for K-LEB ***/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

int sum(int n, int m)
  {
          int sum ;
          sum = n+m;
          return sum;
  }
  int sub(int n, int m)
  {
          int sub;
          sub = n-m;
          return sub;
  }
  int mul(int n, int m)
  {
          int mul;
          mul = n*m;
          return mul;
  }
  int divi(int n, int m)
  {
          int div;
          div = n/m;
          return div;
  }
  void printall(int sum, int sub, int mul, int div)
  {
          printf("Sum = %d\n", sum);
          printf("Sub = %d\n", sub);
          printf("Mul = %d\n", mul);
          printf("Div = %d\n", div);
  }
  void calculate()
  {
		int i,j;
          int asum,asub,amul,adiv;
			srand(time(0));
          i=rand();
          j=rand();
          asum=sum(i,j);
          asub=sub(i,j);
          amul=mul(i,j);
          adiv=divi(i,j);
          //printall(asum,asub,amul,adiv);

  }
void main()
  {
		  printf("K-LEB test program start with PID: %d\nPerforming task...\n",getpid());
		  for(int i = 0; i < 1000; ++i)
			calculate();
		  printf("Task finish\nK-LEB test program exit\n");
}

