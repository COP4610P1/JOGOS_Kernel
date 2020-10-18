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

#define ENTRY_NAME "elevator3"
#define ENTRY_SIZE 2000
#define PERMS 0644
#define PARENT NULL
#define CNT_SIZE 20
//functions
void printElevator(void);
void appendToMessage(char *appendToMessage);
void loading_elevator(void *data);
void unloading_elevator(void *data);
void get_floor_status(int floor);

static char *message;
static char copyMessage[ENTRY_SIZE];
static const int MAXPASSENGER = 10;
static const int MAXFLOOR = 10;
static struct file_operations fops;
static int read_p;
char strInt[15];
int count = 0;
int passenger_count = 0;
int queued_passenger_count = 0;
int total_passenger_service = 0;
int waiting_on_floor[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int elevator_move = 1;

bool stop = false;
bool service_start = false;
bool elevator_loading = false;
bool elevator_infected = false;

//passenger_struct
typedef struct passenger
{
	int type;
	int destination_floor;
	int start_floor;
	struct list_head list;

} Passenger;

struct list_head passenger_list = LIST_HEAD_INIT(passenger_list);
struct list_head passenger_queue_list = LIST_HEAD_INIT(passenger_queue_list);

struct elevator_thread_parameter
{
	int id;
	int cnt;
	int level;
	char state[8];
	struct task_struct *kthread;
	struct mutex mutex;
};

struct elevator_thread_parameter elevator_thread;

void loading_elevator(void *data)
{
	struct elevator_thread_parameter *parm = data;
	//stop = true;
	printk(KERN_WARNING "enters loading elevator");

	if (queued_passenger_count != 0 && !list_empty_careful(&passenger_queue_list))
	{
		Passenger *p;
		Passenger *ptest;
		struct list_head *temp;
		struct list_head *dummy;

		list_for_each_safe(temp, dummy, &passenger_queue_list)
		{
			if (passenger_count < 10)
			{
				printk(KERN_WARNING "foreach looping punk ass");
				if (temp != NULL)
				{
					p = list_entry(temp, Passenger, list);

					printk(KERN_WARNING "p destination: %d \nstart : %d",
						   p->destination_floor, p->start_floor);

					if (p->start_floor == elevator_thread.level)
					{

						elevator_loading = true;
						sprintf(parm->state, "LOADING");
						printk(KERN_WARNING "p2 destination: %d \nstart : %d",
							   p->destination_floor, p->start_floor);
						if (p->type == 1)
						{

							elevator_infected = true;
						}
						if ((p->type == 0 && !elevator_infected) || p->type == 1)
						{
							list_move(temp, &passenger_list);
							passenger_count++;
							queued_passenger_count--;
							waiting_on_floor[p->start_floor - 1] -= 1;
						}

						ptest = list_last_entry(&passenger_list, Passenger, list);

						printk(KERN_WARNING "ptest destination: %d \nstart : %d",
							   ptest->destination_floor, ptest->start_floor);
						printk(KERN_WARNING "kfree 3");
					}

					printk(KERN_WARNING "kfree 2");
				}
			}
		}

		printk(KERN_WARNING "kfree 1");
	}
	stop = false;
}

void unloading_elevator(void *data)
{
	struct elevator_thread_parameter *parm = data;
	stop = true;

	printk(KERN_WARNING "enters unloading elevator");

	if (passenger_count != 0 && !list_empty_careful(&passenger_list))
	{
		Passenger *p;
		struct list_head *temp;
		struct list_head *dummy;

		list_for_each_safe(temp, dummy, &passenger_list)
		{
			printk(KERN_WARNING "foreach looping punk ass");
			if (temp != NULL)
			{
				p = list_entry(temp, Passenger, list);

				if (p->destination_floor == elevator_thread.level)
				{
					sprintf(parm->state, "LOADING");
					elevator_loading = true;
					list_del(temp); //removes from linked list
					passenger_count--;
					total_passenger_service += 1;
				}
			}
		}
	}

	if (passenger_count == 0)
	{
		elevator_infected = false;
	}
}
/******************************************************************************/
int thread_run(void *data)
{
	struct elevator_thread_parameter *parm = data;

	while (!kthread_should_stop())
	{
		if(queued_passenger_count == 0 && passenger_count == 0)
		{
			sprintf(elevator_thread.state, "IDLE");
		}
		if(queued_passenger_count > 0 || passenger_count > 0)
		{
			ssleep(2);
			if (stop)
			{
				// if (parm != NULL)
				// sprintf(parm->state, "LOADING");

				unloading_elevator(parm);
				loading_elevator(parm);
			}
			else if (parm != NULL)
			{

				if (mutex_lock_interruptible(&parm->mutex) == 0)
				{
					//parm->cnt++;

					if (parm->level == 10)
					{
						elevator_move = -1;
					}
					else if (parm->level == 1)
					{
						elevator_move = 1;
					}

					if (stop == false && parm != NULL)
					{
						if (elevator_loading)
						{
							ssleep(1);
							elevator_loading = false;
						}

						parm->level += elevator_move;

						if (elevator_move == 1)
						{
							//sprintf(elevator_thread.state, "UP");
							sprintf(parm->state, "UP");
						}
						else if (elevator_move == -1)
						{
							sprintf(parm->state, "DOWN");
						}
					}
					printk(KERN_WARNING "level ++");
					stop = true;
				}

				mutex_unlock(&parm->mutex);
			}
			else
			{
				break;
			}
		}
	}

	if (parm != NULL)
	{
		printk(KERN_WARNING "kfree 66");

		kfree(parm);
	}
	return 0;
}

void thread_init_parameter(struct elevator_thread_parameter *parm)
{

	static int id = 1;

	parm->id = id++;
	parm->cnt = 0;
	//sprintf(parm->state, "OFFLINE");
	mutex_init(&parm->mutex);
	parm->kthread = kthread_run(thread_run, parm, "thread example %d", parm->id);
}

extern int (*STUB_start_elevator)(void);
int start_elevator(void)
{

	if (!service_start)
	{
		stop = false;

		printk(KERN_NOTICE "%s: start elevator module\n", __FUNCTION__);
		sprintf(elevator_thread.state, "IDLE");
		elevator_thread.level = 1;
		thread_init_parameter(&elevator_thread);
		if (IS_ERR(elevator_thread.kthread))
		{
			printk(KERN_WARNING "error spawning thread");
			remove_proc_entry(ENTRY_NAME, NULL);
			return PTR_ERR(elevator_thread.kthread);
		}
	}

	service_start = true;
	return 1;
}

extern int (*STUB_issue_request)(int, int, int);
int issue_request(int start_floor, int destination_floor, int type)
{
	Passenger *queued_passenger;
	Passenger *test;
	stop = true;

	printk(KERN_WARNING "enters issue request start %d, \ndes %d \ntype %d",
		   start_floor, destination_floor, type);

	queued_passenger = kmalloc(sizeof(Passenger), __GFP_RECLAIM);
	if (queued_passenger == NULL)
	{
		printk(KERN_WARNING "hello_proc_open");
		return -ENOMEM;
	}

	queued_passenger->type = type;
	queued_passenger->destination_floor = destination_floor;
	queued_passenger->start_floor = start_floor;

	list_add_tail(&queued_passenger->list, &passenger_queue_list);
	queued_passenger_count++;
	waiting_on_floor[start_floor - 1] += 1;

	test = list_last_entry(&passenger_queue_list, Passenger, list);

	printk(KERN_WARNING "enters issue request start %d, \ndes %d \ntype %d",
		   test->start_floor, test->destination_floor, test->type);

	return 0;
}

extern int (*STUB_stop_elevator)(void);
int stop_elevator(void)
{
	if(service_start)
	{
		return 1;
	}
	service_start = true;

	return 0;
}

void get_floor_status(int floor)
{

	Passenger *p;
	struct list_head *temp;

	list_for_each(temp, &passenger_queue_list)
	{
		p = list_entry(temp, Passenger, list);
		if (floor == p->start_floor)
		{
			if (p->type == 0)
			{

				appendToMessage("|");
			}
			else
			{

				appendToMessage("X");
			}
		}
	}
}
void printElevator(void)
{
	//int i = 0;
	sprintf(copyMessage, " ");
	appendToMessage("\nElevator state: ");

	appendToMessage(elevator_thread.state);

	if (elevator_infected)
	{
		appendToMessage("\nElevator status: Infected");
	}
	else
	{
		appendToMessage("\nElevator status: Not Infected");
	}
	appendToMessage("\nCurrent floor: ");
	sprintf(strInt, "%d ", elevator_thread.level);
	appendToMessage(strInt);
	appendToMessage("\nNumber of passengers: ");
	sprintf(strInt, "%d ", passenger_count);
	appendToMessage(strInt);
	appendToMessage("\nNumber of passengers waiting: ");
	sprintf(strInt, "%d ", queued_passenger_count);
	appendToMessage(strInt);
	appendToMessage("\nNumber passengers serviced: ");
	sprintf(strInt, "%d ", total_passenger_service);
	appendToMessage(strInt);

	count = 10;

	while (count != 0)
	{
		// stop = true;
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
		sprintf(strInt, "%d : ", count);
		appendToMessage(strInt);
		sprintf(strInt, " %d ", waiting_on_floor[count - 1]);
		appendToMessage(strInt);
		get_floor_status(count);
		count--;
	}
	appendToMessage("\n\n( “|” for human, “X” for zombie )\n");
	sprintf(message, copyMessage);
}

void appendToMessage(char *appendToMessage)
{

	strcat(copyMessage, appendToMessage);
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

	printElevator();

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
	sprintf(elevator_thread.state, "OFFLINE");

	return 0;
}

module_init(elevator_module_init);

static void elevator_module_exit(void)
{

	stop = true;
	printk(KERN_WARNING "remove proc");
	remove_proc_entry(ENTRY_NAME, NULL);
	printk(KERN_WARNING "kfree 44");
	if (elevator_thread.kthread != NULL)
		kthread_stop(elevator_thread.kthread);

	printk(KERN_WARNING "remove mutex");
	mutex_destroy(&elevator_thread.mutex);

	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_stop_elevator = NULL;

	// kfree(test);
	// kfree(queued_passenger);
	// kfree(message);
	printk(KERN_NOTICE "Removing /proc/%s\n", ENTRY_NAME);
}

module_exit(elevator_module_exit);
