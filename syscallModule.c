#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");

#define ENTRY_NAME "elevator_thread"
#define ENTRY_SIZE 2000
#define PERMS 0644
#define PARENT NULL
#define CNT_SIZE 20
//functions
void printElevator(void);
void appendToMessage(char *appendToMessage);

static char *message;
static char copyMessage[ENTRY_SIZE];
static const int MAXPASSENGER = 10;
static const int MAXFLOOR = 10;
static struct file_operations fops;
static int read_p;
char strInt[5];
int count = 0;

int elevator_move = 1; //

//passenger_struct
typedef struct passenger
{
	int type;
	int destination_floor;
	int start_floor;
	struct list_head list;

} Passenger;

Passenger *queued_passenger;
Passenger *test;

struct list_head passenger_list = LIST_HEAD_INIT(passenger_list);
struct list_head passenger_queue_list = LIST_HEAD_INIT(passenger_queue_list);

struct elevator_thread_parameter
{
	int id;
	int cnt;
	int level;
	struct task_struct *kthread;
};

struct elevator_thread_parameter elevator_thread;

extern int (*STUB_start_elevator)(void);
int start_elevator(void)
{
	printk(KERN_NOTICE "%s: start elevator module\n", __FUNCTION__);
	return 1;
}

extern int (*STUB_issue_request)(int, int, int);
int issue_request(int start_floor, int destination_floor, int type)
{

	queued_passenger = kmalloc(sizeof(Passenger), __GFP_RECLAIM);
	//if (que == null)
	queued_passenger->type = type;
	queued_passenger->destination_floor = destination_floor;
	queued_passenger->start_floor = start_floor;

	list_add_tail(&queued_passenger->list, &passenger_queue_list);
	test = kmalloc(sizeof(Passenger), __GFP_RECLAIM);
	test = list_first_entry(&passenger_queue_list, Passenger, list);

	printk(KERN_NOTICE "Test Start floor : %d \nDestination floor : %d \nIssue Request : %d",
		   test->start_floor, test->destination_floor, test->type);

	kfree(queued_passenger);

	return 0;
}

extern int (*STUB_stop_elevator)(void);
int stop_elevator(void)
{
	printk(KERN_NOTICE "%s: stop elevator module\n", __FUNCTION__);

	return 3;
}

void printElevator(void)
{

	appendToMessage("\nElevator state: UP");
	appendToMessage("\nElevator status: Infected");
	appendToMessage("\nCurrent floor: 4");
	appendToMessage("\nNumber of passengers: 6");
	appendToMessage("\nNumber of passengers waiting: 10");
	appendToMessage("\nNumber passengers serviced: 61");
	//int i;
	while (!kthread_should_stop())

		appendToMessage("\n\n[");
	if (count == elevator_thread.level)
	{
		appendToMessage("*");
	}
	else
	{
		appendToMessage(" ");
	}

	appendToMessage("] Floor ");
	sprintf(strInt, "%d :", count);
	appendToMessage(strInt);
	sprintf(strInt, " %d ", 3);
	appendToMessage(strInt);
	appendToMessage(" | | X");
}

appendToMessage("\n\n( “|” for human, “X” for zombie )\n");
}
void appendToMessage(char *appendToMessage)
{
	strcat(copyMessage, appendToMessage);
}

/******************************************************************************/
int thread_run(void *data)
{
	struct elevator_thread_parameter *parm = data;

	while (!kthread_should_stop())
	{
		ssleep(2);
		parm->cnt++;

		if (parm->level == 10)
		{
			elevator_move = -1;
		}
		else if (parm->level == 1)
		{
			elevator_move = 1;
		}
		parm->level += elevator_move;
	}

	return 0;
}

void thread_init_parameter(struct elevator_thread_parameter *parm)
{
	static int id = 1;

	parm->id = id++;
	parm->cnt = 0;
	parm->kthread = kthread_run(thread_run, parm, "thread example %d", parm->id);
}

int thread_proc_open(struct inode *sp_inode, struct file *sp_file)
{
	read_p = 1;
	message = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);

	if (message == NULL)
	{
		printk(KERN_WARNING "hello_proc_open");
		return -ENOMEM;
	}

	//appendToMessage("tesssssssst");
	printElevator();
	sprintf(message, copyMessage);

	//sprintf(message, "Thread %d has blocked %d times\n", elevator_thread.id, elevator_thread.cnt);

	return 0;
}

ssize_t thread_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset)
{
	int len = strlen(message);

	read_p = !read_p;
	if (read_p)
		return 0;

	copy_to_user(buf, message, len);
	return len;
}

int thread_proc_release(struct inode *sp_inode, struct file *sp_file)
{
	kfree(message);
	return 0;
}

/******************************************************************************/

static int elevator_module_init(void)
{
	STUB_start_elevator = start_elevator;
	STUB_issue_request = issue_request;
	STUB_stop_elevator = stop_elevator;

	fops.open = thread_proc_open;
	fops.read = thread_proc_read;
	fops.release = thread_proc_release;

	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops))
	{
		printk(KERN_WARNING "thread_init");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}

	thread_init_parameter(&elevator_thread);
	if (IS_ERR(elevator_thread.kthread))
	{
		printk(KERN_WARNING "error spawning thread");
		remove_proc_entry(ENTRY_NAME, NULL);
		return PTR_ERR(elevator_thread.kthread);
	}

	return 0;
}

module_init(elevator_module_init);

static void elevator_module_exit(void)
{

	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_stop_elevator = NULL;

	//kfree(message);
	kthread_stop(elevator_thread.kthread);
	remove_proc_entry(ENTRY_NAME, NULL);
	printk(KERN_NOTICE "Removing /proc/%s\n", ENTRY_NAME);
}

module_exit(elevator_module_exit);
