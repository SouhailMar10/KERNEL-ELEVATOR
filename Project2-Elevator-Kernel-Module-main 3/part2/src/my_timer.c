#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/timekeeping.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group27");
MODULE_DESCRIPTION("Kernel module for timer");

#define ENTRY_NAME "my_timer"
#define PERMS 0644
#define PARENT NULL

static struct proc_dir_entry *timer_entry;
struct timespec64 last_time; // last time
static int entry_count = 0;

static ssize_t timer_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
    struct timespec64 now_time; // current time
    char buf[256];              // max length of message
    int len = 0;                // length of message

    ktime_get_real_ts64(&now_time);

    if (entry_count < 2)
    { // first time >reading
        len = snprintf(buf, sizeof(buf), "current time: %lld.%lld\n", (long long)now_time.tv_sec, (long long)now_time.tv_nsec);
    }
    else
    { // not first time reading
        long long elapsed_sec = now_time.tv_sec - last_time.tv_sec;
        long long elapsed_nsec = now_time.tv_nsec - last_time.tv_nsec;
        if (elapsed_nsec < 0)
        {
            elapsed_sec -= 1;
            elapsed_nsec += 1000000000;
        }
        len = snprintf(buf, sizeof(buf), "current time: %lld.%lld\nelapsed time: %lld.%lld\n", (long long)now_time.tv_sec, (long long)now_time.tv_nsec, elapsed_sec, elapsed_nsec);
    }

    last_time.tv_sec = now_time.tv_sec;
    last_time.tv_nsec = now_time.tv_nsec;
    entry_count += 1;

    return simple_read_from_buffer(ubuf, count, ppos, buf, len); // better than copy_to_user
}

// file operations
static const struct proc_ops timer_fops = {.proc_read = timer_read};

static int __init timer_init(void)
{ // create entry
    timer_entry = proc_create(ENTRY_NAME, PERMS, PARENT, &timer_fops);
    if (timer_entry == NULL)
    {
        return -ENOMEM;
    }
    return 0;
}

static void __exit timer_exit(void)
{ // remove entry
    proc_remove(timer_entry);
}

module_init(timer_init);
module_exit(timer_exit);
