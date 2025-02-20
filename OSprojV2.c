#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/sysinfo.h>
#include <linux/jiffies.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/seq_file.h>

#define PROC_FILE "sys_health"
#define PROC_DIR "sys"
#define PROC_PATH PROC_DIR "/" PROC_FILE

// Thresholds
unsigned long mem_threshold = 100; // in MB
module_param(mem_threshold, ulong, 0644);
MODULE_PARM_DESC(mem_threshold, "Memory threshold in MB to trigger alerts");

static struct timer_list sys_health_timer;
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_file;
static char sys_health_output[4096];

// Function to get CPU load averages
void get_cpu_load(char *output) {
    double loadavg[3];
    if (getloadavg(loadavg, 3) != -1) {
        sprintf(output + strlen(output), "CPU Load Averages: 1 min: %.2f, 5 min: %.2f, 15 min: %.2f\n", loadavg[0], loadavg[1], loadavg[2]);
    } else {
        printk(KERN_ERR "getloadavg failed\n");
    }
}

// Function to get memory usage
void get_memory_usage(char *output) {
    struct sysinfo info;
    si_meminfo(&info);

    sprintf(output + strlen(output), "Total RAM: %lu MB\n", (info.totalram * info.mem_unit) / (1024 * 1024));
    sprintf(output + strlen(output), "Free RAM: %lu MB\n", (info.freeram * info.mem_unit) / (1024 * 1024));
    sprintf(output + strlen(output), "Shared RAM: %lu MB\n", (info.sharedram * info.mem_unit) / (1024 * 1024));
    sprintf(output + strlen(output), "Buffer RAM: %lu MB\n", (info.bufferram * info.mem_unit) / (1024 * 1024));

    // Check memory threshold
    if ((info.freeram * info.mem_unit) / (1024 * 1024) < mem_threshold) {
        sprintf(output + strlen(output), "ALERT: Free RAM below threshold!\n");
        printk(KERN_ALERT "sys_health: Free RAM below threshold!\n");
    }
}

// Function to get disk I/O statistics
void get_disk_io(char *output) {
    struct file *f;
    char buf[128];
    mm_segment_t fs;
    int bytes;

    f = filp_open("/proc/diskstats", O_RDONLY, 0);
    if (IS_ERR(f)) {
        printk(KERN_ERR "filp_open failed\n");
        return;
    }

    fs = get_fs();
    set_fs(KERNEL_DS);
    bytes = kernel_read(f, buf, sizeof(buf), &f->f_pos);
    set_fs(fs);

    if (bytes >= 0) {
        buf[bytes] = '\0';
        strcat(output, buf);
    } else {
        printk(KERN_ERR "kernel_read failed\n");
    }

    filp_close(f, NULL);
}

// Function to read system health metrics
void read_sys_health(char *output) {
    get_cpu_load(output);
    get_memory_usage(output);
    get_disk_io(output);
}

// Timer callback function
void sys_health_timer_callback(struct timer_list *t) {
    memset(sys_health_output, 0, sizeof(sys_health_output));
    read_sys_health(sys_health_output);

    // Update /proc entry
    proc_set_size(proc_file, strlen(sys_health_output));
    proc_set_user(proc_file, KUIDT_INIT(0), KGIDT_INIT(0));

    mod_timer(&sys_health_timer, jiffies + msecs_to_jiffies(5000)); // 5 seconds interval
}

// /proc read function
ssize_t proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    return simple_read_from_buffer(buf, count, ppos, sys_health_output, strlen(sys_health_output));
}

// File operations for /proc entry
static const struct file_operations proc_fops = {
    .owner = THIS_MODULE,
    .read = proc_read,
};

// Initialization function
static int __init sys_health_init(void) {
    printk(KERN_INFO "sys_health module loaded by Group XYZ\n");

    // Create /proc directory
    proc_dir = proc_mkdir(PROC_DIR, NULL);
    if (!proc_dir) {
        printk(KERN_ERR "Failed to create /proc directory\n");
        return -ENOMEM;
    }

    // Create /proc entry
    proc_file = proc_create(PROC_FILE, 0644, proc_dir, &proc_fops);
    if (!proc_file) {
        printk(KERN_ERR "Failed to create /proc entry\n");
        proc_remove(proc_dir);
        return -ENOMEM;
    }

    // Initialize timer
    timer_setup(&sys_health_timer, sys_health_timer_callback, 0);
    mod_timer(&sys_health_timer, jiffies + msecs_to_jiffies(5000)); // 5 seconds interval

    return 0;
}

// Cleanup function
static void __exit sys_health_exit(void) {
    printk(KERN_INFO "sys_health module unloaded by Group XYZ\n");

    // Remove /proc entry
    proc_remove(proc_file);
    proc_remove(proc_dir);

    // Cleanup timer
    del_timer(&sys_health_timer);
}

module_init(sys_health_init);
module_exit(sys_health_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group XYZ");
MODULE_DESCRIPTION("System Health Monitoring Module");
