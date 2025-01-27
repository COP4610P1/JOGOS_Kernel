#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "wrappers.h"

int rnd(int min, int max)
{
	return rand() % (max - min + 1) + min;
}

int main(int argc, char **argv)
{
	int type;
	int start;
	int dest;

	srand(time(0));

	if (argc != 1)
	{
		printf("wrong number of args\n");
		return -1;
	}

	type = rnd(0,1);
	start = rnd(1,10);
	do
	{
		dest = rnd(1,10);
	} while (dest == start);

	long ret = issue_request(start, dest, type);
	printf("Issue (%d, %d, %d) returned %ld\n", start, dest, type, ret);

	return 0;
}
