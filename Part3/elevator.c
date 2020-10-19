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

#define ENTRY_NAME "elevator"
#define ENTRY_SIZE 2000
#define PERMS 0644
#define PARENT NULL
#define CNT_SIZE 20

//functions

//prints the elevator
void printElevator(void);
// utility function to append a string to the message output
void appendToMessage(char *appendToMessage);

//function to load the elevator
void loading_elevator(void *data);

//function to unload the elevator
void unloading_elevator(void *data);

//append the floor status to the message output
void get_floor_status(int floor);

//variables
//use to send a message to the user buffer/proc entry
static char *message;

//utility variable use to copy a string
static char copyMessage[ENTRY_SIZE];
//use for the proc entry
static struct file_operations fops;
static int read_p;

//utility variable use for storing string version of ints
char strInt[15];
//utility variable use for incrementing or decrementing loops
int count = 0;

//keep count of passengers on the elevator
int passenger_count = 0;
//keep count of passengers waiting for the elevator
int queued_passenger_count = 0;

//keep count of passengers serviced by the elevator
int total_passenger_service = 0;

// store the number of passengers waiting on each floor
int waiting_on_floor[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

//use to either move the elevator up or down
//stores either 1 or -1
int elevator_move = 1;

//use to check if the elevator already started
bool service_start = false;

//use to check if the elevator is loading
bool elevator_loading = false;
//use to check if the elevator has been infected
bool elevator_infected = false;

//use to check if the elevator has stopped
bool elevator_stop = false;

//indicates that the elevator started for the first time
bool initial_start = true;

//passenger_struct to store info on the passenger
typedef struct passenger
{
	int type;
	int destination_floor;
	int start_floor;
	struct list_head list;

} Passenger;

//list to store all the passenger in the elevator
struct list_head passenger_list = LIST_HEAD_INIT(passenger_list);

//list to store all the passenger waiting for the elevator
struct list_head passenger_queue_list = LIST_HEAD_INIT(passenger_queue_list);

//keeps the thread info
struct elevator_thread_parameter
{
	int id;
	int cnt;
	int level;
	char state[8];
	struct task_struct *kthread;
	struct mutex mutex;
};

// thread that manipulates the eleavtor position and loading
//and unloading of passengers
struct elevator_thread_parameter elevator_thread;

//function to load the elevator
void loading_elevator(void *data)
{
	struct elevator_thread_parameter *parm = data;

	if (queued_passenger_count != 0 && !list_empty_careful(&passenger_queue_list))
	{
		Passenger *p;
		struct list_head *temp;
		struct list_head *dummy;

		//looping throught the queue to load passengers if the
		//their floor and elevator floor equals
		list_for_each_safe(temp, dummy, &passenger_queue_list)
		{
			//only load passenger if there is space (<10 passenger)
			//in elevator
			if (passenger_count < 10)
			{
				if (temp != NULL)
				{
					p = list_entry(temp, Passenger, list);

					if (p->start_floor == elevator_thread.level)
					{

						elevator_loading = true;
						//changing state
						sprintf(parm->state, "LOADING");

						//if zombie exist the elevator is now infected
						if (p->type == 1)
						{
							elevator_infected = true;
						}

						//only load humans if the elevator is not infected
						//Or the passenger is a zombie
						if ((p->type == 0 && !elevator_infected) || p->type == 1)
						{
							//moving the entry in the queue to the passenger list(elevator)
							list_move(temp, &passenger_list);
							passenger_count++;
							queued_passenger_count--;

							//decreasing the amount waiting on each floor
							waiting_on_floor[p->start_floor - 1] -= 1;
						}
					}
				}
			}
		}
	}
}

//function to unload the elevator
void unloading_elevator(void *data)
{
	struct elevator_thread_parameter *parm = data;

	if (passenger_count != 0 && !list_empty_careful(&passenger_list))
	{
		Passenger *p;
		struct list_head *temp;
		struct list_head *dummy;

		//looping throught the passengers on the elevator for unloading
		//if their floor and elevator floor equals
		list_for_each_safe(temp, dummy, &passenger_list)
		{
			if (temp != NULL)
			{
				p = list_entry(temp, Passenger, list);

				if (p->destination_floor == elevator_thread.level)
				{
					//changing state
					sprintf(parm->state, "LOADING");
					elevator_loading = true;
					//removes passenger from passenger list (elevator)
					list_del(temp);
					passenger_count--;

					total_passenger_service += 1;
				}
			}
		}
	}

	// elevator is no longer infected if
	//elevator doesn't has passengers
	if (passenger_count == 0)
	{
		elevator_infected = false;
	}
}

//function use to run the thread
int thread_run(void *data)
{
	struct elevator_thread_parameter *parm = data;

	// runs thread unless stop is triggered
	while (!kthread_should_stop())
	{
		// changes state to offline or idle for start or stop
		if (queued_passenger_count == 0 && passenger_count == 0)
		{
			if (elevator_stop)
			{

				sprintf(parm->state, "OFFLINE");
			}
			else
			{
				sprintf(parm->state, "IDLE");
			}
		}

		//if there is passenger to service
		if (queued_passenger_count > 0 || passenger_count > 0)
		{
			ssleep(2);

			if (parm != NULL)
			{
				unloading_elevator(parm);

				//doesn't load passengers if the the elevator stops
				if (!elevator_stop)
				{
					loading_elevator(parm);
				}

				if (mutex_lock_interruptible(&parm->mutex) == 0)
				{
					// if changes the direction of the elevator
					if (parm->level == 10)
					{
						elevator_move = -1; // down
					}
					else if (parm->level == 1)
					{
						elevator_move = 1; //up
					}

					//stop elevator from moving if the elevator stop function called
					//and no more passenger to service
					if (!elevator_stop || passenger_count > 0)
					{
						if (elevator_loading)
						{
							ssleep(1);
							elevator_loading = false;
						}

						//changing elevator levels
						parm->level += elevator_move;

						//displays up or down base on the direction
						if (elevator_move == 1)
						{
							sprintf(parm->state, "UP");
						}
						else if (elevator_move == -1)
						{
							sprintf(parm->state, "DOWN");
						}
					}
					else
					{
						sprintf(parm->state, "OFFLINE");
					}
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

		kfree(parm);
	}

	return 0;
}

//function use to initialize the thread
void thread_init_parameter(struct elevator_thread_parameter *parm)
{

	static int id = 1;

	parm->id = id++;
	parm->cnt = 0;

	// initializing the mutex for locking
	mutex_init(&parm->mutex);

	//starting the thread
	parm->kthread = kthread_run(thread_run, parm, "thread  %d", parm->id);
}

//linked function to system call to start elevator
extern int (*STUB_start_elevator)(void);
int start_elevator(void)
{
	//checks if elevator is running
	if (service_start == true)
	{
		return 1;
	}

	//checks if elevator already started
	if (initial_start == true)
	{
		elevator_stop = false;

		sprintf(elevator_thread.state, "IDLE");
		elevator_thread.level = 1;

		// initialize thread
		thread_init_parameter(&elevator_thread);

		if (IS_ERR(elevator_thread.kthread))
		{
			printk(KERN_WARNING "error spawning thread");
			remove_proc_entry(ENTRY_NAME, NULL);
			return PTR_ERR(elevator_thread.kthread);
		}
	}
	else
	{
		elevator_stop = false;
	}

	service_start = true;
	return 0;
}

//linked function to system call to issue request to elevator
extern int (*STUB_issue_request)(int, int, int);
int issue_request(int start_floor, int destination_floor, int type)
{
	Passenger *queued_passenger;

	// doesn't accept request if elevator stop
	if (!elevator_stop)
	{
		// bad request
		if (start_floor > 10 && destination_floor > 10 && type > 1)
			return 1;

		//allocating memory for a new passenger to wait for the elevator
		queued_passenger = kmalloc(sizeof(Passenger), __GFP_RECLAIM);

		if (queued_passenger == NULL)
		{
			printk(KERN_WARNING "List Error");
			return -ENOMEM;
		}

		//creating a new passenger to wait for the elevator
		queued_passenger->type = type;
		queued_passenger->destination_floor = destination_floor;
		queued_passenger->start_floor = start_floor;

		//adding passenger to the list
		list_add_tail(&queued_passenger->list, &passenger_queue_list);
		queued_passenger_count++;

		// adding to the passenger waiting on each floor
		waiting_on_floor[start_floor - 1] += 1;
	}

	return 0;
}

//linked function to system call to stop elevator
extern int (*STUB_stop_elevator)(void);
int stop_elevator(void)
{
	//elevator already stop
	if (elevator_stop)
	{
		return 1;
	}

	//stop elevator
	elevator_stop = true;
	initial_start = false;
	service_start = false;
	return 0;
}

//append the floor status to the message output
void get_floor_status(int floor)
{

	Passenger *p;
	struct list_head *temp;

	//loop to check passengers waiting for zombies
	//then append it to output
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

//prints the elevator
// This entire function append to output (message) for the proc
void printElevator(void)
{

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

// utility function to append a string to the message output
void appendToMessage(char *appendToMessage)
{

	strcat(copyMessage, appendToMessage);
}

// opens the proc entry
int thread_proc_open(struct inode *sp_inode, struct file *sp_file)
{

	read_p = 1;

	//allocating memory for the proc message
	message = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);

	if (message == NULL)
	{
		printk(KERN_WARNING "elevator_proc_open");
		return -ENOMEM;
	}

	//prints the elevator
	printElevator();

	return 0;
}

//reads the proc entry
ssize_t thread_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset)
{

	int len = strlen(message);

	read_p = !read_p;
	if (read_p)
		return 0;

	//copy the message created to the user buffer
	copy_to_user(buf, message, len);

	return len;
}

//clear memory and releases proc
int thread_proc_release(struct inode *sp_inode, struct file *sp_file)
{
	//clear the message space
	kfree(message);
	return 0;
}

//initalize the module
static int elevator_module_init(void)
{
	//link system calls to functions
	STUB_start_elevator = start_elevator;
	STUB_issue_request = issue_request;
	STUB_stop_elevator = stop_elevator;

	//link proc calls to functions
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

//exit the module
//cleans up state
static void elevator_module_exit(void)
{

	printk(KERN_WARNING "remove proc");
	remove_proc_entry(ENTRY_NAME, NULL);

	if (elevator_thread.kthread != NULL)
		kthread_stop(elevator_thread.kthread);

	printk(KERN_WARNING "remove mutex");
	mutex_destroy(&elevator_thread.mutex);

	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_stop_elevator = NULL;

	printk(KERN_NOTICE "Removing /proc/%s\n", ENTRY_NAME);
}

module_exit(elevator_module_exit);
