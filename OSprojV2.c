#include <linux/module.h>       // Needed by all modules
#include <linux/kernel.h>       // Needed for KERN_INFO, printk()
#include <linux/init.h>         // Needed for the macros __init and __exit
#include <linux/proc_fs.h>      // Needed for proc filesystem
#include <linux/seq_file.h>     // Needed for seq_file operations
#include <linux/timer.h>        // Needed for kernel timers
#include <linux/mm.h>           // Needed for si_meminfo()
#include <linux/sched.h>        // Needed for CPU load (potentially) - Not fully implemented here
#include <linux/slab.h>         // Needed for kmalloc/kfree (though not strictly needed for this basic version)

// --- Group Identification ---
#define GROUP_NAME "YARA"
#define MEMBER_NAMES "Ethan" 
// --------------------------

MODULE_LICENSE("GPL");
MODULE_AUTHOR(MEMBER_NAMES);
MODULE_DESCRIPTION("A simple system health monitor kernel module");
MODULE_VERSION("0.1");

// --- Module Parameters ---
static int mem_threshold = 80; // Default threshold (percentage of free memory)
module_param(mem_threshold, int, 0644);
MODULE_PARM_DESC(mem_threshold, "Memory usage threshold percentage (free memory < X % triggers alert). Default: 80");

// --- Global Variables ---
static struct timer_list health_timer; // Kernel timer
static struct proc_dir_entry *proc_file_entry; // /proc entry pointer
static const char *proc_filename = "sys_health"; // /proc filename

// Variables to store the latest metrics for /proc
static unsigned long last_total_mem = 0;
static unsigned long last_free_mem = 0;
// Placeholder for other metrics
// static unsigned long last_cpu_load = 0; // Example
// static unsigned long last_disk_io = 0;  // Example

// --- Function Prototypes ---
static void collect_and_check_metrics(struct timer_list *t);
static int health_proc_show(struct seq_file *m, void *v);
static int health_proc_open(struct inode *inode, struct file *file);

// --- /proc File Operations ---
// Starting with kernel 5.6, proc_ops is recommended
// Use file_operations for older kernels if needed
static const struct proc_ops health_proc_ops = {
    .proc_open    = health_proc_open,
    .proc_read    = seq_read, // Use seq_read for seq_file
    .proc_lseek   = seq_lseek,
    .proc_release = single_release, // Use single_release for simple seq_file
};

// --- Timer Callback Function ---
static void collect_and_check_metrics(struct timer_list *t)
{
    struct sysinfo info;

    // --- 1. Collect Metrics ---
    // Memory Usage
    si_meminfo(&info);
    last_total_mem = info.totalram * info.mem_unit / (1024 * 1024); // Convert to MB
    last_free_mem = info.freeram * info.mem_unit / (1024 * 1024); // Convert to MB

    // CPU Load (Basic Placeholder - More complex in reality)
    // Accessing load averages (like avenrun) requires care and knowing kernel specifics.
    // printk(KERN_INFO "[%s] CPU Load: Not implemented in this version.\n", GROUP_NAME);
    // last_cpu_load = ...;

    // Disk I/O (Basic Placeholder - Very complex, involves block layer stats)
    // printk(KERN_INFO "[%s] Disk I/O: Not implemented in this version.\n", GROUP_NAME);
    // last_disk_io = ...;


    // --- 2. Check Thresholds ---
    if (last_total_mem > 0) {
        unsigned long free_percentage = (last_free_mem * 100) / last_total_mem;
        if (free_percentage < mem_threshold) {
            printk(KERN_WARNING "[%s] Alert: Free Memory (%lu%%) is below threshold (%d%%)!\n",
                   GROUP_NAME, free_percentage, mem_threshold);
        }
    }

    // Check other thresholds here...

    // --- 3. Rearm Timer ---
    // Rerun timer every 5 seconds
    mod_timer(&health_timer, jiffies + msecs_to_jiffies(5000));
}

// --- /proc Show Function (Called when /proc file is read) ---
static int health_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "--- System Health Monitor (%s) ---\n", GROUP_NAME);
    seq_printf(m, "Group Members: %s\n", MEMBER_NAMES);
    seq_printf(m, "Memory Total: %lu MB\n", last_total_mem);
    seq_printf(m, "Memory Free:  %lu MB\n", last_free_mem);
    // seq_printf(m, "CPU Load Avg (1min): %lu (Not Implemented)\n", last_cpu_load);
    // seq_printf(m, "Disk I/O (sectors read): %lu (Not Implemented)\n", last_disk_io);
    seq_printf(m, "--------------------------------------\n");
    seq_printf(m, "Memory Alert Threshold (Free %% < ): %d%%\n", mem_threshold);
    return 0;
}

// --- /proc Open Function ---
static int health_proc_open(struct inode *inode, struct file *file)
{
    // Use single_open for simple seq_file usage
    // It allocates the seq_file structure and associates it with health_proc_show
    return single_open(file, health_proc_show, NULL);
}

// --- Module Initialization Function ---
static int __init sys_health_init(void)
{
    printk(KERN_INFO "[%s] Loading System Health Monitor Module (Members: %s).\n", GROUP_NAME, MEMBER_NAMES);

    // Create /proc entry
    proc_file_entry = proc_create(proc_filename, 0444, NULL, &health_proc_ops);
    if (proc_file_entry == NULL) {
        printk(KERN_ERR "[%s] Error creating /proc/%s entry!\n", GROUP_NAME, proc_filename);
        return -ENOMEM; // Return error if proc creation fails
    }
    printk(KERN_INFO "[%s] /proc/%s entry created.\n", GROUP_NAME, proc_filename);

    // Setup and start the timer
    timer_setup(&health_timer, collect_and_check_metrics, 0);
    // Start timer to run after 5 seconds initially
    mod_timer(&health_timer, jiffies + msecs_to_jiffies(5000));
    printk(KERN_INFO "[%s] Health check timer started (runs every 5 seconds).\n", GROUP_NAME);

    // Set initial values immediately for first /proc read if desired
    collect_and_check_metrics(NULL); // Optional: Run once immediately

    return 0; // Success
}

// --- Module Cleanup Function ---
static void __exit sys_health_exit(void)
{
    printk(KERN_INFO "[%s] Unloading System Health Monitor Module (Members: %s).\n", GROUP_NAME, MEMBER_NAMES);

    // Delete the timer
    del_timer_sync(&health_timer); // Wait for timer callback to finish if running
    printk(KERN_INFO "[%s] Health check timer stopped.\n", GROUP_NAME);

    // Remove the /proc entry
    if (proc_file_entry) {
        proc_remove(proc_file_entry);
        printk(KERN_INFO "[%s] /proc/%s entry removed.\n", GROUP_NAME, proc_filename);
    }
}

// --- Register Init and Exit Functions ---
module_init(sys_health_init);
module_exit(sys_health_exit);
