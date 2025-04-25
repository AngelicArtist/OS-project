/*
 * Group Name: CyberGuardians
 * Group Members: Alice Smith, Bob Johnson, Charlie Lee
 * Course: SCIA 360 - Operating System Security
 * Project: Linux Kernel Module for Real-Time Health Monitoring
 */

#include <linux/module.h>       // Core module definitions
#include <linux/kernel.h>       // KERN_INFO, KERN_WARNING, etc.
#include <linux/init.h>         // __init, __exit macros
#include <linux/proc_fs.h>      // procfs API (proc_create, remove_proc_entry)
#include <linux/seq_file.h>     // seq_file API for /proc output
#include <linux/timer.h>        // Kernel timers
#include <linux/jiffies.h>      // jiffies and msecs_to_jiffies
#include <linux/sysinfo.h>      // si_meminfo()
#include <linux/sched/loadavg.h> // avenrun for load average
#include <linux/moduleparam.h>  // module_param()
#include <linux/spinlock.h>     // Spinlocks for protecting shared data
#include <linux/slab.h>         // kmalloc/kfree (though not strictly needed here now)
#include <linux/uaccess.h>      // copy_to_user (used internally by seq_file)

// --- Group Identification ---
#define GROUP_NAME "CyberGuardians"
#define GROUP_MEMBERS "Alice Smith, Bob Johnson, Charlie Lee"
#define LOG_PREFIX "[" GROUP_NAME "] SCIA 360: "

// --- Module Parameters (Thresholds) ---
static int mem_threshold_percent = 80; // Default memory usage threshold (percentage)
module_param(mem_threshold_percent, int, 0644);
MODULE_PARM_DESC(mem_threshold_percent, "Memory usage threshold percentage (0-100)");

// LOAD_INT(1.0) = 100. Threshold for 1-minute load average (scaled). E.g. 200 means 2.00
static int cpu_load_threshold_scaled = 200;
module_param(cpu_load_threshold_scaled, int, 0644);
MODULE_PARM_DESC(cpu_load_threshold_scaled, "Scaled 1-min CPU load average threshold (e.g., 100 means 1.00)");

// --- Global Variables ---
static struct timer_list sys_health_timer;
static const char *proc_filename = "sys_health";
#define TIMER_INTERVAL_MS 5000 // 5 seconds

// Structure to hold the latest stats
struct system_health_stats {
    unsigned long total_mem_mb;
    unsigned long free_mem_mb;
    unsigned long used_mem_percent;
    unsigned long load_avg_1min_scaled; // Scaled load average (like in /proc/loadavg * 100)
    // Add fields for Disk I/O here if implemented
};

static struct system_health_stats current_stats;
static DEFINE_SPINLOCK(stats_lock); // Lock to protect access to current_stats

// --- Timer Callback Function ---
static void timer_callback_func(struct timer_list *timer)
{
    struct sysinfo si;
    unsigned long flags;
    unsigned long total_mem;
    unsigned long avail_mem; // Use available memory if possible, otherwise free
    unsigned long used_mem;
    unsigned long mem_used_pcent;
    unsigned long load_avg_scaled;

    // 1. Collect Metrics
    // Memory Usage
    si_meminfo(&si);
    total_mem = si.totalram * si.mem_unit;
    // si.freeram is just free, si.bufferram + si.sharedram + si.freeswap etc also relevant
    // A better measure of truly "available" memory is often needed, but requires more complex checks
    // For simplicity, let's calculate used based on total and free (as per prompt example)
    // Or better: Use available memory if kernel provides it easily (newer kernels might have si.avialram or similar)
    // Let's stick to a simple total - free calculation for now.
    avail_mem = si.freeram * si.mem_unit; // Basic free RAM
    used_mem = total_mem - avail_mem;
    mem_used_pcent = 0;
    if (total_mem > 0) {
        mem_used_pcent = (100 * used_mem) / total_mem;
    }

    // CPU Load (1-minute average, scaled)
    // LOAD_INT scales the load average by 100 (e.g., 1.23 becomes 123)
    // avenrun is already scaled by FSHIFT (usually 11, factor 2048)
    // To get a value comparable to LOAD_INT, we adjust:
    load_avg_scaled = LOAD_INT(avenrun[0]); //avenrun[0] is 1-min avg

    // Disk I/O (Placeholder - requires significant extra code)
    // collect_disk_io_stats();

    // 2. Store Metrics (Protected by Spinlock)
    spin_lock_irqsave(&stats_lock, flags);
    current_stats.total_mem_mb = total_mem / (1024 * 1024);
    current_stats.free_mem_mb = avail_mem / (1024 * 1024);
    current_stats.used_mem_percent = mem_used_pcent;
    current_stats.load_avg_1min_scaled = load_avg_scaled;
    // Update disk stats here if implemented
    spin_unlock_irqrestore(&stats_lock, flags);

    // 3. Check Thresholds and Log Alerts
    if (current_stats.used_mem_percent > mem_threshold_percent) {
        printk(KERN_WARNING LOG_PREFIX "Alert: Memory usage (%lu%%) exceeds threshold (%d%%)\n",
               current_stats.used_mem_percent, mem_threshold_percent);
    }
    if (current_stats.load_avg_1min_scaled > cpu_load_threshold_scaled) {
         // Divide by LOAD_FACTOR (100) for printing human-readable load avg
        printk(KERN_WARNING LOG_PREFIX "Alert: CPU Load Average (1-min: %lu.%02lu) exceeds threshold (%d.%02d)\n",
               current_stats.load_avg_1min_scaled / LOAD_FACTOR,
               current_stats.load_avg_1min_scaled % LOAD_FACTOR,
               cpu_load_threshold_scaled / LOAD_FACTOR,
               cpu_load_threshold_scaled % LOAD_FACTOR);
    }
    // Check disk I/O thresholds here if implemented

    // 4. Rearm Timer
    mod_timer(&sys_health_timer, jiffies + msecs_to_jiffies(TIMER_INTERVAL_MS));
}

// --- /proc File Read Functionality ---

// This function is called when the /proc file is read
static int sys_health_proc_show(struct seq_file *m, void *v)
{
    unsigned long flags;
    struct system_health_stats stats_copy;

    // Safely copy stats under lock
    spin_lock_irqsave(&stats_lock, flags);
    stats_copy = current_stats; // Copy structure
    spin_unlock_irqrestore(&stats_lock, flags);

    seq_printf(m, "--- System Health Monitor (%s) ---\n", GROUP_NAME);
    seq_printf(m, "Team Members: %s\n", GROUP_MEMBERS);
    seq_printf(m, "Memory Total: %lu MB\n", stats_copy.total_mem_mb);
    seq_printf(m, "Memory Free:  %lu MB\n", stats_copy.free_mem_mb);
    seq_printf(m, "Memory Used:  %lu%%\n", stats_copy.used_mem_percent);
    seq_printf(m, "CPU Load (1m): %lu.%02lu\n",
                stats_copy.load_avg_1min_scaled / LOAD_FACTOR,
                stats_copy.load_avg_1min_scaled % LOAD_FACTOR);
    // Add Disk I/O output here if implemented
    seq_printf(m, "--- Thresholds ---\n");
    seq_printf(m, "Memory Usage Alert Threshold: > %d%%\n", mem_threshold_percent);
    seq_printf(m, "CPU Load (1m) Alert Threshold: > %d.%02d\n",
               cpu_load_threshold_scaled / LOAD_FACTOR,
               cpu_load_threshold_scaled % LOAD_FACTOR);

    return 0;
}

// This function is called when the /proc file is opened
static int sys_health_proc_open(struct inode *inode, struct file *file)
{
    // Use seq_file helper to manage buffer/output for /proc reads
    return single_open(file, sys_health_proc_show, NULL);
}

// Define file operations for the /proc entry (using modern proc_ops)
static const struct proc_ops sys_health_proc_fops = {
    .proc_open    = sys_health_proc_open,
    .proc_read    = seq_read,         // Use standard seq_file read
    .proc_lseek   = seq_lseek,        // Use standard seq_file seek
    .proc_release = single_release,   // Use standard seq_file release
};


// --- Module Initialization and Cleanup ---

static int __init sys_health_init(void)
{
    printk(KERN_INFO LOG_PREFIX "Loading module... Team: %s\n", GROUP_MEMBERS);

    // Initialize spinlock
    // spin_lock_init(&stats_lock); // Already initialized statically with DEFINE_SPINLOCK

    // Create /proc entry
    if (!proc_create(proc_filename, 0444, NULL, &sys_health_proc_fops)) {
        printk(KERN_ERR LOG_PREFIX "Failed to create /proc/%s entry\n", proc_filename);
        return -ENOMEM; // Return error if proc creation failed
    }
    printk(KERN_INFO LOG_PREFIX "Created /proc/%s entry\n", proc_filename);

    // Setup and start the timer
    timer_setup(&sys_health_timer, timer_callback_func, 0);
    mod_timer(&sys_health_timer, jiffies + msecs_to_jiffies(TIMER_INTERVAL_MS)); // Start timer

    printk(KERN_INFO LOG_PREFIX "Module loaded successfully.\n");
    return 0; // Success
}

static void __exit sys_health_exit(void)
{
    printk(KERN_INFO LOG_PREFIX "Unloading module... Team: %s\n", GROUP_MEMBERS);

    // Stop the timer (synchronously ensuring callback is not running)
    del_timer_sync(&sys_health_timer);

    // Remove the /proc entry
    remove_proc_entry(proc_filename, NULL);
    printk(KERN_INFO LOG_PREFIX "Removed /proc/%s entry\n", proc_filename);

    printk(KERN_INFO LOG_PREFIX "Module unloaded.\n");
}

// Register module entry and exit points
module_init(sys_health_init);
module_exit(sys_health_exit);

// Module metadata
MODULE_LICENSE("GPL");
MODULE_AUTHOR(GROUP_MEMBERS);
MODULE_DESCRIPTION("SCIA 360 Project: Real-time system health monitoring kernel module");
MODULE_VERSION("0.1");
