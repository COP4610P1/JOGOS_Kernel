#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/time.h>

MODULE_LICENSE("Dual BSD/GPL");

#define BUF_LEN 100 //max length of read/write message

static struct proc_dir_entry *proc_entry; //pointer to proc entry

static char msg[BUF_LEN];  //buffer to store read/write message
static int procfs_buf_len; //variable to hold length of message

//storing the time for the last call
static struct timespec last_time;

//storing the time for the amount of time passed since the last call
static struct timespec elasped_time;

static ssize_t procfile_read(struct file *file, char *ubuf, size_t count, loff_t *ppos)
{
	//getting the current time
	struct timespec current_time = current_kernel_time();

	//if the last time is zero display only the current time
	if ((long)last_time.tv_sec == 0)
	{

		sprintf(msg, "The current time is : %lld.%.9ld\n",
				(long long)current_time.tv_sec, current_time.tv_nsec);
		//setting the last time to current time
		last_time = current_time;
	}
	else
	{
		//getting the elasped time by substracting current time and last time
		elasped_time = timespec_sub(current_time, last_time);

		sprintf(msg, "The current time: %lld.%.9ld\nelasped time: %lld.%.9ld\n",
				(long long)current_time.tv_sec, current_time.tv_nsec,
				(long long)elasped_time.tv_sec, elasped_time.tv_nsec);

		last_time = current_time;
	}

	printk(KERN_INFO "proc_read\n");
	procfs_buf_len = strlen(msg);

	//check if data already read and if space in user buffer
	if (*ppos > 0 || count < procfs_buf_len)
		return 0;

	//send data to user buffer
	if (copy_to_user(ubuf, msg, procfs_buf_len))
		return -EFAULT;

	*ppos = procfs_buf_len; //update position

	printk(KERN_INFO "gave to user %s\n", msg);

	return procfs_buf_len; //return number of characters read
}

static ssize_t procfile_write(struct file *file, const char *ubuf, size_t count, loff_t *ppos)
{
	printk(KERN_INFO "proc_write\n");

	//write min(user message size, buffer length) characters
	if (count > BUF_LEN)
		procfs_buf_len = BUF_LEN;
	else
		procfs_buf_len = count;

	// copying mes to user buffer for display
	copy_from_user(msg, ubuf, procfs_buf_len);

	printk(KERN_INFO "got from user: %s\n", msg);

	return procfs_buf_len;
}

static struct file_operations procfile_fops = {
	.owner = THIS_MODULE,
	.read = procfile_read, //fill in callbacks to read/write functions
	.write = procfile_write,
};

static int my_timer_init(void)
{
	//proc_create(filename, permissions, parent, pointer to fops)
	//creating the proc entry
	proc_entry = proc_create("timer", 0666, NULL, &procfile_fops);
	if (proc_entry == NULL)
		return -ENOMEM;

	return 0;
}

static void my_timer_exit(void)
{
	//removing the proc entry
	proc_remove(proc_entry);
	return;
}

module_init(my_timer_init);
module_exit(my_timer_exit);
