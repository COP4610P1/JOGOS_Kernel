#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#define __NR_START_ELEVATOR 335
#define __NR_ISSUE_REQUEST 336
#define __NR_STOP_ELEVATOR 337

int start_elevator()
{
    return syscall(__NR_START_ELEVATOR);
}

int issue_request(int start_floor, int destination_floor, int type)
{
    return syscall(__NR_ISSUE_REQUEST, start_floor, destination_floor, type);
}

int stop_elevator()
{
    return syscall(__NR_STOP_ELEVATOR);
}

int main(int argc, char **argv)
{


    int start_elevator_test = start_elevator();
int issue_request_test = issue_request(2, 10, 1);
     issue_request_test = issue_request(3, 10, 0);
    issue_request_test = issue_request(3, 8, 0);

//    int stop_elevator_test = stop_elevator();

//    if (start_elevator_test < 0 || stop_elevator_test < 0 || issue_request_test < 0)
  //      perror("system call error");
    //else
//	printf("Function successful. passed in: start_elevator_test : %d, stop_elevator_test : %d, issue_request_test: %d \n ",
  //             start_elevator_test,
    //           stop_elevator_test, issue_request_test);


    //printf("start_elevator_test value: %d\n", start_elevator_test);
    //printf("stop_elevator_test value: %d\n", stop_elevator_test);
    //printf("issue_request_test value: %d\n", issue_request_test);

    return 0;
}

