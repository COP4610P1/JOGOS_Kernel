#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Elevator Scheduler: Humans v. Zombies");

#define MODULE_NAME "elevator2"
#define MODULE_PERMISSIONS 0644
#define MODULE_PARENT NULL

#define IDLE 0
#define UP 1
#define DOWN 2
#define LOADING 3
#define OFFLINE 4

#define NUMFLOORS 10

#define MALLOCFLAGS (__GFP_RECLAIM | __GFP_IO | __GFP_FS)

void elevator_syscalls_create(void);
void elevator_syscalls_remove(void);
void initQueue(void);
void queuePassenger(int type, int start, int end);
void PrintQueue(void);
char* queueToString(void);
int passengWeights(int type);
int passengerQueueSize(int floor);
int passengerQueueWeight(int floor);
int elevatorMove(int floor);
int elevWeight(void);
int elevListSize(void);
int elevatorRun(void *data);
int ifLoad(void);
int ifUnload(void);
void loadPassengers(int floor);
void unloadPassengers(void);

struct queueEntries
{
	struct list_head list;
	int m_type;
	int m_startFloor;
	int m_destFloor;
};

struct list_head passengerQueue[NUMFLOORS];
struct list_head elevList;


int stop_s;
int mainDirection;
int nextDirection;
int currFloor;
int nextFloor;
int numPassengers;
int numWeight;
int waitPassengers;
int passengersServiced;
int passengersServFloor[NUMFLOORS];
char status[10];

char *str;
int i;
int rp;
char *message;

struct mutex passengerQueueMutex;
struct mutex elevatorListMutex;

struct task_struct* elevator_thread;

static struct file_operations fileOperations;

int OpenModule(struct inode * sp_inode, struct file *sp_file)
{
	printk(KERN_NOTICE "OpenModule Called\n");
	rp = 1;
	message = kmalloc(sizeof(char) * 2048, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	if (message == NULL)
	{
		printk("Error: OpenModule");
		return -ENOMEM;
	}
	return 0;
}

char *directionToString (int mainDirection)
{
	static char str[32];

	switch (mainDirection)
	{
		case OFFLINE:
			sprintf(str, "OFFLINE");
			break;
		case IDLE:
                        sprintf(str, "IDLE");
                        break;
                case UP:
                        sprintf(str, "UP");
                        break;
                case DOWN:
                        sprintf(str, "DOWN");
                        break;
                case LOADING:
                        sprintf(str, "LOADING");
                        break;
                default:
                        sprintf(str, "ERROR");
                        break;
	}
	return str;
}

ssize_t ReadModule(struct file *sp_file, char __user *buff, size_t size, loff_t *offset)
{
	int n;
	numPassengers = elevListSize();
	numWeight = elevWeight();
	n = numWeight % 1;
	if (n)
	{
		//sprintf(message, "Main elevator direction: %s \nCurrent floor: %d \nNext floor: %d \nCurrent passengers: %d \nCurrent Weight: %d.5 units\nPassengers serviced: %d \nPassengers waiting: %s \n",
		//directionToString(mainDirection), currFloor, nextFloor, numWeight, numPassengers, passengersServiced, queueToString());

		//sprintf(message, "Elevator state: %s \nElevator status:  \nCurrent floor: %d \nNumber of passengers: %d \nNumber of passengers waiting: %s \nNumber of Passengers serviced: %d  \n",
		//directionToString(mainDirection), currFloor, numWeight, queueToString(), passengersServiced);

		sprintf(message, "Elevator state: %s \nElevator status: \nCurrent floor: %d \nNumber of passengers: %d \nNumber of passengers serviced: %d \nNumber of passengers waiting: %s \n",
		directionToString(mainDirection), currFloor, numWeight, passengersServiced, queueToString());
	}
	else
	{
		//sprintf(message, "Main elevator direction: %s \nCurrent floor: %d \nNext floor: %d \nCurrent passengers: %d\nCurrent Weight: %d units\nPassengers serviced %d\nPassengers waiting: %s \n",
		//directionToString(mainDirection), currFloor, nextFloor, numWeight, numPassengers, passengersServiced, queueToString());

		//sprintf(message, "Elevator state: %s \nElevator status:  \nCurrent floor: %d \nNumber of passengers: %d \nNumber of passengers waiting: %s \nNumber of passengers serviced: %d \n",
		//directionToString(mainDirection), currFloor, numWeight, queueToString(), passengersServiced);

		sprintf(message, "Elevator state: %s \nElevator status: \nCurrent floor: %d \nNumber of passengers: %d \nNumber of passengers serviced: %d \nNumber of passengers waiting: %s \n",
		directionToString(mainDirection), currFloor, numWeight, passengersServiced, queueToString());
	}
	rp = !rp;
	if (rp)
	{
		return 0;
	}

	printk(KERN_NOTICE "ReadModule() called.\n");
	copy_to_user(buff, message, strlen(message));

	return strlen(message);
}

int ReleaseModule(struct inode *sp_inode, struct file *sp_file)
{
	printk(KERN_NOTICE "ReleaseModule() called.\n");
	kfree(message);
	return 0;
}

static int InitializeModule(void)
{
	printk(KERN_NOTICE "Creating /proc/%s.\n", MODULE_NAME);

	fileOperations.open = OpenModule;
	fileOperations.read = ReadModule;
	fileOperations.release = ReleaseModule;

	mainDirection = OFFLINE;
	nextDirection = UP;
	stop_s = 0;
	currFloor = 1;
	nextFloor = 1;
	numPassengers = 0;
	numWeight = 0;
	waitPassengers = 0;
	for (i = 0; i < 10; i++)
	{
		passengersServFloor[i] = 0;
	}
	initQueue();
	elevator_syscalls_create();
	mutex_init(&passengerQueueMutex);
	mutex_init(&elevatorListMutex);
	elevator_thread = kthread_run(elevatorRun, NULL, "Elevator Thread");
	if(IS_ERR(elevator_thread))
	{
		printk("Error: ElevatorRun\n");
		return PTR_ERR(elevator_thread);
	}
	if(!proc_create(MODULE_NAME, MODULE_PERMISSIONS, NULL, &fileOperations))
	{
		printk("Error: proc_create\n");
		remove_proc_entry(MODULE_NAME, NULL);
		return -ENOMEM;
	}
	return 0;
}

static void ExitModule(void)
{
	int r;
	remove_proc_entry(MODULE_NAME, NULL);
	elevator_syscalls_remove();
	printk(KERN_NOTICE "Removing /proc/%s.\n", MODULE_NAME);
	r = kthread_stop(elevator_thread);
	if(r != -EINTR)
	{
		printk("Elevator stopped...\n");
	}
}

extern int (*STUB_start_elevator)(void);
int start_elevator(void)
{
	if(stop_s)
	{
		stop_s = 0;
		printk("stop_s\n");
		return 0;
	}
	else if (mainDirection == OFFLINE)
	{
		printk("Starting elevator\n");
		mainDirection = IDLE;
		return 0;
	}
	else
	{
		return 1;
	}
}

extern int (*STUB_issue_request)(int,int,int);
int issue_request(int start_floor, int destination_floor, int passenger_type)
{
	printk("New request: %d, %d => %d\n", start_floor, destination_floor, passenger_type);
	if (start_floor == destination_floor)
	{
		passengersServiced++;
		passengersServFloor[start_floor - 1]++;
	}
	else
	{
		queuePassenger(passenger_type, start_floor, destination_floor);
	}
	return 0;
}

extern int (*STUB_stop_elevator)(void);
int stop_elevator(void)
{
	printk("Stopping elevator\n");
	if (stop_s == 1)
	{
		return 1;
	}
	stop_s = 1;
	return 0;
}

void elevator_syscalls_create(void)
{
	STUB_start_elevator = &(start_elevator);
	STUB_issue_request = &(issue_request);
	STUB_stop_elevator = &(stop_elevator);
}

void elevator_syscalls_remove(void)
{
        STUB_start_elevator = NULL;
        STUB_issue_request = NULL;
        STUB_stop_elevator = NULL;
}

void initQueue(void)
{
	int i = 0;
	while (i < NUMFLOORS)
	{
		INIT_LIST_HEAD(&passengerQueue[i]);
		i++;
	}
	INIT_LIST_HEAD(&elevList);
}

void queuePassenger(int type, int start, int end)
{
	struct queueEntries *newEntry;
	newEntry = kmalloc(sizeof(struct queueEntries), MALLOCFLAGS);
	newEntry->m_type = type;
        newEntry->m_startFloor = start;
        newEntry->m_destFloor = end;
	mutex_lock_interruptible(&passengerQueueMutex);
	list_add_tail(&newEntry->list, &passengerQueue[start - 1]);
	mutex_unlock(&passengerQueueMutex);
	PrintQueue();
}

void PrintQueue(void)
{
	struct list_head *pos;
	struct queueEntries* entry;
	int currentPos = 0;
	int i = 0;
	printk("Passenger Queue:\n");
	mutex_lock_interruptible(&passengerQueueMutex);
	while (i < NUMFLOORS)
	{
		printk("Floor: %d\n", i+1);
		list_for_each(pos, &passengerQueue[i])
		{
			entry = list_entry(pos, struct queueEntries, list);
			printk("Queue pos: %d\nType: %d\nStart Floor: %d\nDest Floor: %d\n", currentPos, entry->m_type, entry->m_startFloor, entry->m_destFloor);
			++currentPos;
		}
		i++;
	}
	mutex_unlock(&passengerQueueMutex);
	printk("\n");
}

char* queueToString(void)
{
	static char str1[2048];
	static char str2[256];
	int passQueueSize;
	int passQueueWeight;
	int passQueueServed;
	int i = 11, odd = 0;
	//sprintf(str1, "\n\nIn the passenger queue:\n");
	while (i > 0)
	{
		//sprintf(str2, "\nFloor: %d", i-1);
		//strcat(str1, str2);
		passQueueSize = passengerQueueSize(i);
		passQueueWeight = passengerQueueWeight(i);
		passQueueServed = passengersServFloor[i-1];
		odd = passQueueWeight % 2;
		if(odd)
		{
			//sprintf(str2, "Numeber of passengers in the queue: %d\nWeight of the queue: %d.5\nPassengers served: %d\n", passQueueSize, passQueueWeight/2, passQueueServed);
			sprintf(str2, "[ ]");
			strcat(str1, str2);
		}
		else
		{
			//sprintf(str2, "Numeber of passengers in the queue: %d\nWeight of the queue: %d\nPassengers served: %d\n", passQueueSize, passQueueWeight/2, passQueueServed);
			sprintf(str2, "[ ]");
			strcat(str1, str2);
		}
		sprintf(str2, "\nFloor: %d", i-1);
		strcat(str1, str2);
		i++;
	}
	strcat(str1, "\n");
	return str1;
}

int passengWeights(int type)
{
	if (type == 0)
		return 2;
	else if (type == 1)
		return 1;
	else if (type == 2 || type == 3)
		return 4;
	else
		return 0;
}

int elevatorMove(int floor)
{
	if (floor == currFloor)
	{
		return 0;
	}
	else
	{
		printk("Now moving floor to %d\n", floor);
		ssleep(2);
		currFloor = floor;
		return 1;
	}
}

int elevatorRun(void *data)
{
	while(!kthread_should_stop())
	{
		switch(mainDirection)
		{
			case OFFLINE:
			break;

			case IDLE:
			nextDirection = UP;
			if(ifLoad() && !stop_s)
			{
				mainDirection = LOADING;
			}
			else
			{
				mainDirection = UP;
				nextFloor = currFloor + 1;
			}
			break;

			case UP:
			elevatorMove(nextFloor);
			if (currFloor == 10)
			{
				nextDirection = DOWN;
				mainDirection = DOWN;
			}
			if ((ifLoad() && !stop_s) || ifUnload())
			{
				mainDirection = LOADING;
			}
			else if (currFloor == 10)
			{
				nextFloor = currFloor - 1;
			}
			else
			{
				nextFloor = currFloor + 1;
			}
			break;

			case DOWN:
			elevatorMove(nextFloor);
			if (currFloor == 1)
                        {
                                nextDirection = UP;
                                mainDirection = UP;
                        }
                        if (stop_s && !elevListSize() && currFloor == 1)
                        {
                                mainDirection = OFFLINE;
				stop_s = 0;
				nextDirection = UP;
                        }
			else if ((ifLoad() && !stop_s) || ifUnload())
                        {
                                mainDirection = LOADING;
                        }
                        else if (currFloor == 1)
                        {
                                nextFloor = currFloor + 1;
                        }
                        else
                        {
                                nextFloor = currFloor - 1;
                        }
                        break;

			case LOADING:
			ssleep(1);
			unloadPassengers();
			while (ifLoad() && !stop_s)
			{
				loadPassengers(currFloor);
			}
			mainDirection = nextDirection;
			if (mainDirection == DOWN)
			{
				if (currFloor == 1)
				{
					nextDirection = UP;
					mainDirection = UP;
					nextFloor = currFloor + 1;
				}
				else
				{
					nextFloor = currFloor - 1;
				}
			}
			else
			{
				if (currFloor == 10)
				{
					nextDirection = DOWN;
					mainDirection = DOWN;
					nextFloor = currFloor - 1;
				}
				else
				{
					nextFloor = currFloor + 1;
				}
			}
			break;
		}
	}
	return 0;
}

int ifLoad(void)
{
	struct queueEntries *entry;
	struct list_head *pos;
	int limit = elevListSize();
	if (limit == 8)
	{
		return 0;
	}
	mutex_lock_interruptible(&passengerQueueMutex);
	list_for_each(pos, &passengerQueue[currFloor - 1])
	{
		entry = list_entry(pos, struct queueEntries, list);
		if ((passengWeights(entry->m_type) + elevWeight() <= 16) && ((entry->m_destFloor > currFloor && nextDirection == UP) || (entry->m_destFloor < currFloor && nextDirection == DOWN)))
		{
			mutex_unlock(&passengerQueueMutex);
			return 1;
		}
	}
	mutex_unlock(&passengerQueueMutex);
	return 0;
}

int ifUnload(void)
{
	struct queueEntries *entry;
        struct list_head *pos;
	mutex_lock_interruptible(&elevatorListMutex);
	list_for_each(pos, &elevList)
	{
		entry = list_entry(pos, struct queueEntries, list);
		if(entry->m_destFloor == currFloor)
		{
			mutex_unlock(&elevatorListMutex);
			return 1;
		}
	}
	mutex_unlock(&elevatorListMutex);
	return 0;
}

void unloadPassengers(void)
{
	struct queueEntries *entry;
        struct list_head *pos, *q;
        mutex_lock_interruptible(&elevatorListMutex);
        list_for_each_safe(pos, q, &elevList)
        {
                entry = list_entry(pos, struct queueEntries, list);
                if(entry->m_destFloor == currFloor)
                {
			passengersServiced++;
			passengersServFloor[entry->m_startFloor - 1]++;
			list_del(pos);
			kfree(entry);
		}
	}
	mutex_unlock(&elevatorListMutex);
}

void loadPassengers(int floor)
{
	int weight = elevWeight();
	struct queueEntries *entry;
	struct list_head *pos, *q;
	int i = floor - 1;

	if (floor > 10 || floor < 1)
        {
                printk(KERN_NOTICE "Error: Invalid Floor\n");
                return;
        }

        mutex_lock_interruptible(&passengerQueueMutex);
        list_for_each_safe(pos, q, &passengerQueue[i])
        {
                entry = list_entry(pos, struct queueEntries, list);
		if ((entry->m_startFloor == floor) && ((passengWeights(entry->m_type) + weight) <= 16))
		{
			struct queueEntries *n;
			n = kmalloc(sizeof(struct queueEntries), MALLOCFLAGS);
			n->m_type = entry->m_type;
			n->m_startFloor = entry->m_startFloor;
			n->m_destFloor = entry->m_destFloor;
			mutex_lock_interruptible(&elevatorListMutex);
			list_add_tail(&n->list, &elevList);
			list_del(pos);
			kfree(entry);
			mutex_unlock(&elevatorListMutex);
			mutex_unlock(&passengerQueueMutex);
			return;
		}
	}
	mutex_unlock(&passengerQueueMutex);
}

int passengerQueueWeight(int floor)
{
	struct queueEntries *entry;
        struct list_head *pos;
	int weight = 0;
	mutex_lock_interruptible(&passengerQueueMutex);
	list_for_each(pos, &passengerQueue[floor - 1])
	{
		entry = list_entry(pos, struct queueEntries, list);
		weight += passengWeights(entry->m_type);
	}
	mutex_unlock(&passengerQueueMutex);
	return weight;
}

int passengerQueueSize(int floor)
{
	struct queueEntries *entry;
        struct list_head *pos;
        int i = 0;
        mutex_lock_interruptible(&passengerQueueMutex);
	list_for_each(pos, &passengerQueue[floor - 1])
	{
		entry = list_entry(pos, struct queueEntries, list);
		if (entry->m_type == 2)
		{
			i += 2;
		}
		else
		{
			i++;
		}
	}
	mutex_unlock(&passengerQueueMutex);
	return i;
}

int elevWeight(void)
{
        struct queueEntries *entry;
        struct list_head *pos;
        int weight = 0;
	mutex_lock_interruptible(&elevatorListMutex);
	list_for_each(pos, &elevList)
	{
		entry = list_entry(pos, struct queueEntries, list);
		weight += passengWeights(entry->m_type);
	}
	mutex_unlock(&elevatorListMutex);
	return weight;
}

int elevListSize(void)
{
	struct queueEntries *entry;
	struct list_head *pos;
	int i = 0;
	mutex_lock_interruptible(&elevatorListMutex);
	list_for_each(pos, &elevList)
        {
                entry = list_entry(pos, struct queueEntries, list);
		if (entry->m_type == 2)
		{
			i += 2;
		}
		else
		{
			i++;
		}
	}
	mutex_unlock(&elevatorListMutex);
        return i;
}


module_init(InitializeModule);
module_exit(ExitModule);
