#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define Disk "/dev/sda1" // Disk path
#define CPU_threshold 80 // CPU threshold
#define Memory_threshold 80 // Memory threshold
#define Disk_threshold 50000 // Disk threshold in KB

/* 
 * Group Name: CyberGuardians
 * Group Members: Alice Smith, Bob Johnson, Charlie Lee
 * Course: SCIA 360 - Operating System Security
 * Project: Linux Kernel Module for Real-Time Health Monitoring
 */

char cpu[5];
long long user, nice, system, idle_time, iowait, irq, softirq, steal; // Variables to store CPU times
char label[256];
long long value;
long long total, free, available, buffers, cached; // Variables to store memory info
long long read, write, flushes, read_time, write_time, io_time; // Variables to store disk info
double cpu_usage, memory_usage, disk_usage; // Variables to store CPU, memory, and disk usage
int choice; // Variable to store user choice

// Function to get CPU times
void get_cpu_times(long long *idle, long long *total) {
    FILE *fp = fopen("/proc/stat", "r"); // Open file in read mode
    if (!fp) { // If file pointer is NULL
        perror("Couldn't open /proc/stat");
        exit(EXIT_FAILURE);
    }

    fscanf(fp, "%s %lld %lld %lld %lld %lld %lld %lld %lld", cpu, &user, &nice, &system, &idle_time, &iowait, &irq, &softirq, &steal); // Read from file
    fclose(fp);

    *idle = idle_time + iowait;
    *total = user + nice + system + idle_time + iowait + irq + softirq + steal;
}

// Function to get memory info
void get_memory_info() {
    FILE *fp = fopen("/proc/meminfo", "r"); // Open file in read mode
    if (!fp) { // If file pointer is NULL
        perror("Couldn't open /proc/meminfo");
        exit(EXIT_FAILURE);
    }
    // Read from file
    while (fscanf(fp, "%s %lld", label, &value) != EOF) { // Read from file
        if (strcmp(label, "MemTotal:") == 0) { // If label is MemTotal
            printf("Total Memory: %lld KB\n", value); // Print total memory
        } else if (strcmp(label, "MemFree:") == 0) { // If label is MemFree
            printf("Free Memory: %lld KB\n", value); // Print free memory
        } else if (strcmp(label, "MemAvailable:") == 0) { // If label is MemAvailable
            printf("Available Memory: %lld KB\n", value); // Print available memory
        } else if (strcmp(label, "Buffers:") == 0) { // If label is Buffers
            printf("Buffers: %lld KB\n", value); // Print buffers
        } else if (strcmp(label, "Cached:") == 0) { // If label is Cached
            printf("Cached: %lld KB\n", value); // Print cached
        }
    }
    fclose(fp);
}

// Function to get disk info
void get_disk_info() {
    FILE *fp = fopen("/proc/diskstats", "r"); // Open file in read mode
    if (!fp) { // If file pointer is NULL
        perror("Couldn't open /proc/diskstats");
        exit(EXIT_FAILURE);
    }
    if (fscanf(fp, "%lld %lld %s %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld", &read, &write, label, &flushes, &read_time, &write_time, &io_time) != EOF) { // Read from file
        if (strcmp(label, Disk) == 0) { // If label is Disk
            printf("Read: %lld\n", read); // Print read
            printf("Write: %lld\n", write); // Print write
            printf("Flushes: %lld\n", flushes); // Print flushes
            printf("Read Time: %lld\n", read_time); // Print read time
            printf("Write Time: %lld\n", write_time); // Print write time
            printf("IO Time: %lld\n", io_time); // Print IO time
        }
    }
    fclose(fp);
}

// Function to check alerts
void check_alerts() {
    long long idle, total;
    long long idle1, total1;
    long long idle_diff, total_diff;
    double cpu_usage;
    // Get CPU times
    get_cpu_times(&idle, &total);
    sleep(1);

    get_cpu_times(&idle1, &total1); // Get CPU times after 1 second
    idle_diff = idle1 - idle;
    total_diff = total1 - total;
    cpu_usage = 100.0 * (total_diff - idle_diff) / total_diff;

    printf("CPU: %.2f\n", cpu_usage); // Print CPU usage
    if (cpu_usage > CPU_threshold) { // If CPU usage is greater than threshold
        printf("CPU usage is greater than threshold\n"); // Print alert
    }
    //--------------------------------
    // Get memory info
    get_memory_info();
    //--------------------------------
    // Get disk info
    get_disk_info();
    //--------------------------------
    if (total < Memory_threshold) { // If total memory is less than threshold
        printf("Memory usage is greater than threshold\n"); // Print alert
    }
    if (read > Disk_threshold) { // If read is greater than threshold
        printf("Disk usage is greater than threshold\n"); // Print alert
    }
}

// Function to monitor system
void monitor_system() {
    while (1) {
        check_alerts(); // Check alerts
        sleep(1); // Sleep for 1 second
    }
}

// Main function
int main() {
    long long idle, total;
    long long idle1, total1;
    long long idle_diff, total_diff;
    double cpu_usage;
    // Menu
    while (choice != 3) {
        printf("1. Monitor System\n");
        printf("2. Check Alerts\n");
        printf("3. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                monitor_system(); // Monitor system
                break;
            case 2:
                check_alerts(); // Check alerts
                break;
            case 3:
                break;
            default:
                printf("Invalid choice\n");
        }
    }
    // Get CPU times
    get_cpu_times(&idle, &total);
    sleep(1);
    get_cpu_times(&idle1, &total1); // Get CPU times after 1 second
    idle_diff = idle1 - idle;
    total_diff = total1 - total;
    cpu_usage = 100.0 * (total_diff - idle_diff) / total_diff;
    printf("CPU: %.2f\n", cpu_usage); // Print CPU usage
    printf("[CyberGuardians] SCIA 360: Module loaded successfully. Team Members: Alice Smith, Bob Johnson, Charlie Lee\n");
    return 0;
}

