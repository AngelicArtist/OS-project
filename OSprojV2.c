#include <linux/module.h>       // Needed by all modules
#include <linux/kernel.h>       // Needed for KERN_INFO, printk()
#include <linux/init.h>         // Needed for the macros __init and __exit
#include <linux/proc_fs.h>      // Needed for proc filesystem
#include <linux/seq_file.h>     // Needed for seq_file operations
#include <linux/timer.h>        // Needed for kernel timers
#include <linux/mm.h>           // Needed for si_meminfo()
#include <linux/sched.h>        // Needed for load average (avenrun) and FSHIFT
#include <linux/vmstat.h>       // Needed for VM event counters (PGPGIN/PGPGOUT)
#include <linux/slab.h>         // Needed for kmalloc/kfree (though not strictly needed for this basic version)

// --- Group Identification ---
#define GROUP_NAME "CyberGuardians" // <<<--- REPLACE WITH YOUR GROUP NAME
#define MEMBER_NAMES "Alice, Bob, Charlie" // <<<--- REPLACE WITH YOUR MEMBER NAMES
// --------------------------

MODULE_LICENSE("GPL");
MODULE_AUTHOR(MEMBER_NAMES);
MODULE_DESCRIPTION("A simple system health monitor kernel module (CPU, Mem, Disk I/O)");
MODULE_VERSION("0.3"); // Version bump

// --- Module Parameters ---
static int mem_threshold = 80; // Default: alert if free memory < 80%
module_param(mem_threshold, int, 0644);
MODULE_PARM_DESC(mem_threshold, "Memory usage threshold percentage (free memory < X % triggers alert). Default: 80");

// cpu_load_threshold * 100 represents the scaled load average threshold (e.g., 150 means alert if load > 1.50)
static int cpu_load_threshold = 150; // Default: alert if 1-min load avg > 1.50
module_param(cpu_load_threshold, int, 0644);
MODULE_PARM_DESC(cpu_load_threshold, "CPU load threshold * 100 (1-min average > X / 100 triggers alert). Default: 150 (for 1.50)");

// disk_io_threshold represents pages paged in+out per interval
static int disk_io_threshold = 1000; // Default: alert if > 1000 pages paged in+out in 5 seconds
module_param(disk_io_threshold, int, 0644);
MODULE_PARM_DESC(disk_io_threshold, "Disk I/O threshold (pages paged in+out per interval > X triggers alert). Default: 1000");

// --- Global Variables ---
static struct timer_list health_timer; // Kernel timer
static struct proc_dir_entry *proc_file_entry; // /proc entry pointer
static const char *proc_filename = "sys_health"; // /proc filename
static unsigned long timer_interval_ms = 5000; // Timer interval in milliseconds

// Variables to store the latest metrics for /proc
static unsigned long last_total_mem = 0;
static unsigned long last_free_mem = 0;
static unsigned long last_cpu_load_scaled = 0; // Scaled 1-min load average
static unsigned long last_pgpgin_delta = 0;    // Pages paged in since last check
static unsigned long last_pgpgout_delta = 0;   // Pages paged out since last check

// Variables to store previous values for calculating deltas
static unsigned long prev_pgpgin = 0;
static unsigned long prev_pgpgout = 0;
static bool first_run = true; // Flag to handle first run for disk I/O delta

// --- Accessing Load Average ---
// avenrun holds scaled 1, 5, 15 min load averages
// We need to declare it as extern as it's defined elsewhere in the kernel
extern unsigned long avenrun[]; // Needs <linux/sched.h>

// --- Function Prototypes ---
static void collect_and_check_metrics(struct timer_list *t);
static int health_proc_show(struct seq_file *m, void *v);
static int health_proc_open(struct inode *inode, struct file *file);

// --- /proc File Operations ---
static const struct proc_ops health_proc_ops = {
    .proc_open    = health_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

// --- Timer Callback Function ---
static void collect_and_check_metrics(struct timer_list *t)
{
    struct sysinfo info;
    unsigned long current_pgpgin, current_pgpgout;
    unsigned long current_load_scaled;
    unsigned long free_percentage = 100; // Default to 100% if total_mem is 0
    unsigned long total_io_delta = 0;

    // --- 1. Collect Metrics ---

    // Memory Usage
    si_meminfo(&info);
    last_total_mem = info.totalram * info.mem_unit / (1024 * 1024); // Convert to MB
    last_free_mem = info.freeram * info.mem_unit / (1024 * 1024); // Convert to MB
    if (last_total_mem > 0) {
        free_percentage = (last_free_mem * 100) / last_total_mem;
    }

    // CPU Load (1-minute average)
    current_load_scaled = avenrun[0]; // Read the scaled 1-min load average
    last_cpu_load_scaled = current_load_scaled;

    // Disk I/O (Page Ins/Outs Rate)
    // Get current global counts for pages paged in/out using VM event counters
    current_pgpgin = count_vm_event(PGPGIN);   // Requires <linux/vmstat.h>
    current_pgpgout = count_vm_event(PGPGOUT); // Requires <linux/vmstat.h>

    if (first_run) {
        // On the first run, we can't calculate a delta
        last_pgpgin_delta = 0;
        last_pgpgout_delta = 0;
        first_run = false;
    } else {
        // Calculate delta since last check
        last_pgpgin_delta = current_pgpgin - prev_pgpgin;
        last_pgpgout_delta = current_pgpgout - prev_pgpgout;
    }
    // Store current values for next calculation
    prev_pgpgin = current_pgpgin;
    prev_pgpgout = current_pgpgout;
    total_io_delta = last_pgpgin_delta + last_pgpgout_delta;

    // --- 2. Check Thresholds ---

    // Memory Alert
    if (free_percentage < mem_threshold) {
        printk(KERN_WARNING "[%s] Alert: Free Memory (%lu%%) is below threshold (%d%%)!\n",
               GROUP_NAME, free_percentage, mem_threshold);
    }

    // CPU Load Alert
    // Compare scaled load average * 100 with threshold
    // Use (1UL << FSHIFT) instead of LOAD_SCALE. FSHIFT is from <linux/sched.h>
    if ((current_load_scaled * 100) > (cpu_load_threshold * (1UL << FSHIFT))) {
        // Format load average for printing: load = current_load_scaled / (1UL << FSHIFT)
        printk(KERN_WARNING "[%s] Alert: CPU Load Avg (1m) (%lu.%02lu) exceeds threshold (> %d.%02d)!\n",
               GROUP_NAME,
               current_load_scaled / (1UL << FSHIFT),
               (current_load_scaled % (1UL << FSHIFT)) * 100 / (1UL << FSHIFT),
               cpu_load_threshold / 100, cpu_load_threshold % 100);
    }

    // Disk I/O Alert
    if (total_io_delta > disk_io_threshold) {
        printk(KERN_WARNING "[%s] Alert: Disk I/O pages (%lu pg/in + %lu pg/out = %lu pgs/%lums) exceeds threshold (> %d)!\n",
               GROUP_NAME, last_pgpgin_delta, last_pgpgout_delta, total_io_delta, timer_interval_ms, disk_io_threshold);
    }


    // --- 3. Rearm Timer ---
    mod_timer(&health_timer, jiffies + msecs_to_jiffies(timer_interval_ms));
}

// --- /proc Show Function (Called when /proc file is read) ---
static int health_proc_show(struct seq_file *m, void *v)
{
    unsigned long load_avg_int, load_avg_frac;

    // Calculate integer and fractional parts of the load average for printing
    // Use (1UL << FSHIFT) instead of LOAD_SCALE
    load_avg_int = last_cpu_load_scaled / (1UL << FSHIFT);
    load_avg_frac = (last_cpu_load_scaled % (1UL << FSHIFT)) * 100 / (1UL << FSHIFT);

    seq_printf(m, "--- System Health Monitor (%s) ---\n", GROUP_NAME);
    seq_printf(m, "Group Members: %s\n", MEMBER_NAMES);
    seq_printf(m, "--------------------------------------\n");
    seq_printf(m, "Memory Total: %lu MB\n", last_total_mem);
    seq_printf(m, "Memory Free:  %lu MB\n", last_free_mem);
    seq_printf(m, "CPU Load Avg (1m): %lu.%02lu\n", load_avg_int, load_avg_frac);
    seq_printf(m, "Disk I/O Rate (last %lums): %lu pg/in, %lu pg/out\n",
               timer_interval_ms, last_pgpgin_delta, last_pgpgout_delta);
    seq_printf(m, "--------------------------------------\n");
    seq_printf(m, "Alert Thresholds:\n");
    seq_printf(m, "  Memory Free < %d %%\n", mem_threshold);
    seq_printf(m, "  CPU Load > %d.%02d\n", cpu_load_threshold / 100, cpu_load_threshold % 100);
    seq_printf(m, "  Disk I/O > %d pages/%lums\n", disk_io_threshold, timer_interval_ms);
    seq_printf(m, "--------------------------------------\n");
    return 0;
}

// --- /proc Open Function ---
static int health_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, health_proc_show, NULL);
}

// --- Module Initialization Function ---
static int __init sys_health_init(void)
{
    printk(KERN_INFO "[%s] Loading System Health Monitor Module v0.3 (Members: %s).\n", GROUP_NAME, MEMBER_NAMES);

    // Create /proc entry
    proc_file_entry = proc_create(proc_filename, 0444, NULL, &health_proc_ops);
    if (proc_file_entry == NULL) {
        printk(KERN_ERR "[%s] Error creating /proc/%s entry!\n", GROUP_NAME, proc_filename);
        return -ENOMEM;
    }
    printk(KERN_INFO "[%s] /proc/%s entry created.\n", GROUP_NAME, proc_filename);

    // Setup the timer
    timer_setup(&health_timer, collect_and_check_metrics, 0);

    // Set initial values and arm timer
    first_run = true; // Ensure first run flag is set
    // Run collection once immediately to populate initial values (especially for disk I/O prev values)
    collect_and_check_metrics(NULL);
    // Now arm the timer for the next regular interval
    mod_timer(&health_timer, jiffies + msecs_to_jiffies(timer_interval_ms));

    printk(KERN_INFO "[%s] Health check timer started (runs every %lu ms).\n", GROUP_NAME, timer_interval_ms);

    return 0; // Success
}

// --- Module Cleanup Function ---
static void __exit sys_health_exit(void)
{
    printk(KERN_INFO "[%s] Unloading System Health Monitor Module v0.3 (Members: %s).\n", GROUP_NAME, MEMBER_NAMES);

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
