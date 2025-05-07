#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For sleep()
#include <sys/statvfs.h> // For disk usage
#include <time.h>   // For timestamps

// ANSI Color Codes
#define RESET   "\033[0m"
#define RED     "\033[0;31m"
#define GREEN   "\033[0;32m"
#define YELLOW  "\033[0;33m"

// Global variables for CPU calculation
static unsigned long long prev_total_cpu_time = 0;
static unsigned long long prev_idle_cpu_time = 0;

// --- Function Prototypes ---
float get_cpu_usage();
float get_ram_usage();
float get_disk_usage(const char *path);
void print_with_timestamp(const char *message, const char *color);

// --- CPU Usage Function ---
float get_cpu_usage() {
    FILE *fp;
    char line[256];
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
    unsigned long long current_total_cpu_time, current_idle_cpu_time;
    unsigned long long total_diff, idle_diff;
    float cpu_percentage = 0.0f;

    fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        perror(RED "Error opening /proc/stat" RESET);
        return -1.0f; // Error
    }

    if (fgets(line, sizeof(line), fp) == NULL) {
        perror(RED "Error reading /proc/stat" RESET);
        fclose(fp);
        return -1.0f;
    }
    fclose(fp);

    // Format: cpu  user nice system idle iowait irq softirq steal guest guest_nice
    sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);

    current_idle_cpu_time = idle + iowait;
    current_total_cpu_time = user + nice + system + idle + iowait + irq + softirq + steal;

    if (prev_total_cpu_time == 0) { // First run, just store values
        prev_total_cpu_time = current_total_cpu_time;
        prev_idle_cpu_time = current_idle_cpu_time;
        // Cannot calculate percentage on first run accurately without a previous point
        return 0.0f; // Or a specific code indicating first run
    }

    total_diff = current_total_cpu_time - prev_total_cpu_time;
    idle_diff = current_idle_cpu_time - prev_idle_cpu_time;

    if (total_diff > 0) { // Avoid division by zero
        cpu_percentage = (float)(total_diff - idle_diff) * 100.0f / (float)total_diff;
    } else {
        cpu_percentage = 0.0f; // Or handle as error/unchanged if total_diff is 0
    }
    

    prev_total_cpu_time = current_total_cpu_time;
    prev_idle_cpu_time = current_idle_cpu_time;

    return cpu_percentage;
}

// --- RAM Usage Function ---
float get_ram_usage() {
    FILE *fp;
    char line[256];
    unsigned long mem_total = 0, mem_available = 0, mem_free = 0, buffers = 0, cached = 0;
    float ram_percentage = 0.0f;
    int found_mem_available = 0;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        perror(RED "Error opening /proc/meminfo" RESET);
        return -1.0f;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %lu kB", &mem_total) == 1) {}
        else if (sscanf(line, "MemFree: %lu kB", &mem_free) == 1) {}
        else if (sscanf(line, "MemAvailable: %lu kB", &mem_available) == 1) {
            found_mem_available = 1;
        }
        else if (sscanf(line, "Buffers: %lu kB", &buffers) == 1) {}
        else if (sscanf(line, "Cached: %lu kB", &cached) == 1) {} // Simple cached, could also include SReclaimable
    }
    fclose(fp);

    if (mem_total == 0) {
        fprintf(stderr, RED "Could not parse MemTotal from /proc/meminfo\n" RESET);
        return -1.0f;
    }

    unsigned long used_mem;
    if (found_mem_available && mem_available > 0) { // Preferred method
        used_mem = mem_total - mem_available;
    } else { // Fallback for older kernels or if MemAvailable is not found/parsed
        used_mem = mem_total - mem_free - buffers - cached;
    }

    ram_percentage = (float)used_mem * 100.0f / (float)mem_total;
    return ram_percentage;
}

// --- Disk Usage Function ---
float get_disk_usage(const char *path) {
    struct statvfs stat;
    float disk_percentage = 0.0f;

    if (statvfs(path, &stat) != 0) {
        perror(RED "Error getting disk usage" RESET);
        return -1.0f;
    }

    unsigned long long total_blocks = stat.f_blocks;
    unsigned long long free_blocks_for_user = stat.f_bavail; // Available to non-root
    // unsigned long long free_blocks_total = stat.f_bfree; // Total free blocks

    if (total_blocks == 0) {
         fprintf(stderr, RED "Total blocks for path %s is zero.\n" RESET, path);
        return -1.0f;
    }
    
    unsigned long long used_blocks = total_blocks - free_blocks_for_user;
    disk_percentage = (float)used_blocks * 100.0f / (float)total_blocks;

    return disk_percentage;
}

// --- Print with Timestamp ---
void print_with_timestamp(const char *message, const char *color) {
    time_t now = time(NULL);
    char time_str[20]; // "YYYY-MM-DD HH:MM:SS\0"
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    if (color) {
        printf("%s%s - %s%s\n", color, time_str, message, RESET);
    } else {
        printf("%s - %s\n", time_str, message);
    }
    fflush(stdout); // Ensure it's printed immediately
}


// --- Main Function ---
int main() {
    float cpu_threshold, ram_threshold, disk_threshold;
    int interval_seconds;
    const char *disk_path_to_monitor = "/"; // Monitor root filesystem

    printf("--- System Monitor Configuration ---\n");

    // Get CPU Threshold
    do {
        printf("Enter CPU threshold (0-100, e.g., 80.0 for 80%%): ");
        if (scanf("%f", &cpu_threshold) != 1 || cpu_threshold < 0 || cpu_threshold > 100) {
            printf(RED "Invalid input. Please enter a number between 0 and 100.\n" RESET);
            while (getchar() != '\n'); // Clear invalid input
            cpu_threshold = -1; // Force re-loop
        }
    } while (cpu_threshold < 0);
    while (getchar() != '\n'); // Clear trailing newline

    // Get RAM Threshold
    do {
        printf("Enter RAM threshold (0-100, e.g., 85.0 for 85%%): ");
        if (scanf("%f", &ram_threshold) != 1 || ram_threshold < 0 || ram_threshold > 100) {
            printf(RED "Invalid input. Please enter a number between 0 and 100.\n" RESET);
            while (getchar() != '\n');
            ram_threshold = -1;
        }
    } while (ram_threshold < 0);
    while (getchar() != '\n');

    // Get Disk Threshold
    do {
        printf("Enter Disk threshold for '%s' (0-100, e.g., 90.0 for 90%%): ", disk_path_to_monitor);
        if (scanf("%f", &disk_threshold) != 1 || disk_threshold < 0 || disk_threshold > 100) {
            printf(RED "Invalid input. Please enter a number between 0 and 100.\n" RESET);
            while (getchar() != '\n');
            disk_threshold = -1;
        }
    } while (disk_threshold < 0);
    while (getchar() != '\n');

    // Get Interval
    do {
        printf("Enter monitoring interval in seconds (e.g., 5): ");
        if (scanf("%d", &interval_seconds) != 1 || interval_seconds <= 0) {
            printf(RED "Invalid input. Please enter a positive integer.\n" RESET);
            while (getchar() != '\n');
            interval_seconds = 0;
        }
    } while (interval_seconds <= 0);
    while (getchar() != '\n');


    char info_msg[200];
    sprintf(info_msg, "Monitoring started. CPU > %.1f%%, RAM > %.1f%%, Disk ('%s') > %.1f%%. Interval: %ds",
            cpu_threshold, ram_threshold, disk_path_to_monitor, disk_threshold, interval_seconds);
    print_with_timestamp(info_msg, GREEN);
    
    // Initial call to CPU usage to establish baseline (it returns 0.0f first time)
    // We need a small delay before the first *real* CPU usage calculation
    print_with_timestamp("Calibrating CPU usage (initial reading)...", YELLOW);
    get_cpu_usage(); // First call to populate static vars
    sleep(1);        // Wait 1 second for a delta to calculate CPU %

    while (1) {
        float current_cpu = get_cpu_usage();
        float current_ram = get_ram_usage();
        float current_disk = get_disk_usage(disk_path_to_monitor);

        char status_msg[256];
        sprintf(status_msg, "Current Stats: CPU: %.2f%%, RAM: %.2f%%, Disk ('%s'): %.2f%%",
                current_cpu, current_ram, disk_path_to_monitor, current_disk);
        print_with_timestamp(status_msg, NULL); // No specific color for normal status

        // Check thresholds and alert
        if (current_cpu >= 0 && current_cpu > cpu_threshold) {
            char alert_msg[100];
            sprintf(alert_msg, "ALERT: CPU usage (%.2f%%) exceeds threshold (%.1f%%)!", current_cpu, cpu_threshold);
            print_with_timestamp(alert_msg, RED);
        }
        if (current_ram >= 0 && current_ram > ram_threshold) {
            char alert_msg[100];
            sprintf(alert_msg, "ALERT: RAM usage (%.2f%%) exceeds threshold (%.1f%%)!", current_ram, ram_threshold);
            print_with_timestamp(alert_msg, RED);
        }
        if (current_disk >= 0 && current_disk > disk_threshold) {
            char alert_msg[100];
            sprintf(alert_msg, "ALERT: Disk usage for '%s' (%.2f%%) exceeds threshold (%.1f%%)!", disk_path_to_monitor, current_disk, disk_threshold);
            print_with_timestamp(alert_msg, RED);
        }

        sleep(interval_seconds > 0 ? interval_seconds : 1); // Ensure sleep is at least 1 if user enters 0 interval for cpu
    }

    return 0;
}
