import psutil
import time
import subprocess
import os

# --- Configuration & User Input ---
DEFAULT_FILESYSTEM_TO_MONITOR = '/'
LOG_FILE_PATH = "/tmp/system_monitor_popup.log" # Using /tmp for easier permissions

def get_user_config():
    """Gets monitoring configuration from the user."""
    print("--- System Monitor Configuration (Pop-up Alerts) ---")
    
    while True:
        try:
            cpu_thresh = float(input("Enter CPU threshold (e.g., 80.0 for 80%): "))
            if 0 <= cpu_thresh <= 100:
                break
            else:
                print("CPU threshold must be between 0 and 100.")
        except ValueError:
            print("Invalid input. Please enter a number.")

    while True:
        try:
            ram_thresh = float(input("Enter RAM threshold (e.g., 85.0 for 85%): "))
            if 0 <= ram_thresh <= 100:
                break
            else:
                print("RAM threshold must be between 0 and 100.")
        except ValueError:
            print("Invalid input. Please enter a number.")

    while True:
        try:
            disk_thresh = float(input(f"Enter Disk threshold for '{DEFAULT_FILESYSTEM_TO_MONITOR}' (e.g., 90.0 for 90%): "))
            if 0 <= disk_thresh <= 100:
                break
            else:
                print("Disk threshold must be between 0 and 100.")
        except ValueError:
            print("Invalid input. Please enter a number.")
    
    while True:
        try:
            check_interval_sec = int(input("Enter check interval in seconds (e.g., 60): "))
            if check_interval_sec > 0:
                break
            else:
                print("Interval must be a positive number.")
        except ValueError:
            print("Invalid input. Please enter a number.")
            
    return cpu_thresh, ram_thresh, disk_thresh, check_interval_sec

# --- Pop-up Alert Function ---
def send_popup_alert(summary, body, urgency="critical"):
    """Sends a desktop pop-up notification."""
    try:
        # Using subprocess.run for better control and error handling
        command = ['notify-send', '-u', urgency, summary, body]
        result = subprocess.run(command, check=True, capture_output=True, text=True)
        log_event(f"Pop-up alert sent: Summary='{summary}'")
    except FileNotFoundError:
        error_message = "Error: 'notify-send' command not found. Please install libnotify-bin or equivalent."
        print(error_message)
        log_event(error_message)
    except subprocess.CalledProcessError as e:
        error_message = f"Failed to send pop-up alert. Command '{e.cmd}' returned non-zero exit status {e.returncode}.\nStderr: {e.stderr}"
        print(error_message)
        log_event(error_message)
    except Exception as e:
        error_message = f"An unexpected error occurred while sending pop-up alert: {e}"
        print(error_message)
        log_event(error_message)

# --- Logging Function ---
def log_event(message):
    """Logs an event to the console and a log file."""
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
    # Get current date for log file name
    current_date_str = time.strftime("%Y-%m-%d", time.localtime())
    dated_log_file_path = f"/tmp/system_monitor_popup_{current_date_str}.log"

    log_message = f"{timestamp} - {message}"
    print(log_message) # Also print to console for real-time feedback
    try:
        with open(dated_log_file_path, "a") as f:
            f.write(log_message + "\n")
    except Exception as e:
        print(f"Could not write to log file {dated_log_file_path}: {e}")

# --- Main Monitoring Logic ---
def monitor_system(cpu_thresh, ram_thresh, disk_thresh):
    """Monitors system resources and sends pop-up alerts if thresholds are exceeded."""
    hostname = os.uname()[1]

    # CPU Usage
    current_cpu_usage = psutil.cpu_percent(interval=1)
    log_event(f"Current CPU Usage: {current_cpu_usage:.2f}%")
    if current_cpu_usage >= cpu_thresh:
        summary = f"ALERT: High CPU Usage on {hostname}"
        body = (f"CPU usage is at {current_cpu_usage:.2f}%, "
                f"exceeding threshold of {cpu_thresh:.2f}%.")
        send_popup_alert(summary, body)

    # RAM Usage
    memory_info = psutil.virtual_memory()
    current_ram_usage = memory_info.percent
    log_event(f"Current RAM Usage: {current_ram_usage:.2f}%")
    if current_ram_usage >= ram_thresh:
        summary = f"ALERT: High RAM Usage on {hostname}"
        body = (f"RAM usage is at {current_ram_usage:.2f}%, "
                f"exceeding threshold of {ram_thresh:.2f}%.")
        send_popup_alert(summary, body)

    # Disk Usage
    disk_info = psutil.disk_usage(DEFAULT_FILESYSTEM_TO_MONITOR)
    current_disk_usage = disk_info.percent
    log_event(f"Current Disk Usage for '{DEFAULT_FILESYSTEM_TO_MONITOR}': {current_disk_usage:.2f}%")
    if current_disk_usage >= disk_thresh:
        summary = f"ALERT: High Disk Usage on {hostname} ({DEFAULT_FILESYSTEM_TO_MONITOR})"
        body = (f"Disk usage for '{DEFAULT_FILESYSTEM_TO_MONITOR}' is at {current_disk_usage:.2f}%, "
                f"exceeding threshold of {disk_thresh:.2f}%.")
        send_popup_alert(summary, body)

# --- Main Execution ---
if __name__ == "__main__":
    try:
        (CPU_THRESHOLD, RAM_THRESHOLD, DISK_THRESHOLD, CHECK_INTERVAL) = get_user_config()

        log_event("System Monitor Started (Pop-up Alerts).")
        log_event(f"Monitoring: CPU > {CPU_THRESHOLD}%, RAM > {RAM_THRESHOLD}%, Disk ('{DEFAULT_FILESYSTEM_TO_MONITOR}') > {DISK_THRESHOLD}%")
        log_event(f"Check interval: {CHECK_INTERVAL} seconds.")
        log_event(f"Log files will be in /tmp/system_monitor_popup_YYYY-MM-DD.log")

        while True:
            monitor_system(CPU_THRESHOLD, RAM_THRESHOLD, DISK_THRESHOLD)
            log_event(f"Waiting for {CHECK_INTERVAL} seconds before next check...")
            time.sleep(CHECK_INTERVAL)

    except KeyboardInterrupt:
        log_event("System Monitor stopped by user.")
    except Exception as e:
        log_event(f"An unexpected error occurred in the main loop: {e}")
    finally:
        log_event("System Monitor shutting down.")
