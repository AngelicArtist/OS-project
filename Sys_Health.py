import psutil
import time
import smtplib
from email.mime.text import MIMEText
import os
import getpass # For securely getting password

# --- Configuration & User Input ---
DEFAULT_FILESYSTEM_TO_MONITOR = '/'
LOG_FILE_PATH = "/tmp/system_monitor.log" # Using /tmp for easier permissions

def get_user_config():
    """Gets monitoring configuration from the user."""
    print("--- System Monitor Configuration ---")
    
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

    alert_email_to = input("Enter email address for alerts: ")
    
    print("\n--- Gmail Configuration for Alerts ---")
    smtp_user = input("Enter your Gmail address (e.g., your_email@gmail.com): ")
    smtp_pass = getpass.getpass(prompt="Enter your Gmail App Password (or password if not using 2-Step Verification): ")
    
    while True:
        try:
            check_interval_sec = int(input("Enter check interval in seconds (e.g., 60): "))
            if check_interval_sec > 0:
                break
            else:
                print("Interval must be a positive number.")
        except ValueError:
            print("Invalid input. Please enter a number.")
            
    return cpu_thresh, ram_thresh, disk_thresh, alert_email_to, smtp_user, smtp_pass, check_interval_sec

# --- Email Sending Function ---
def send_alert_email(subject, body, to_email, from_email, smtp_server, smtp_port, smtp_username, smtp_password):
    """Sends an email alert."""
    try:
        msg = MIMEText(body)
        msg['Subject'] = subject
        msg['From'] = from_email
        msg['To'] = to_email

        print(f"Attempting to send email: To={to_email}, Subject={subject}")
        with smtplib.SMTP(smtp_server, smtp_port) as server:
            server.starttls()
            server.login(smtp_username, smtp_password)
            server.sendmail(from_email, to_email, msg.as_string())
        print("Alert email sent successfully.")
        log_event(f"Alert email sent: To={to_email}, Subject={subject}")
    except Exception as e:
        error_message = f"Failed to send alert email: {e}"
        print(error_message)
        log_event(error_message)

# --- Logging Function ---
def log_event(message):
    """Logs an event to the console and a log file."""
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
    log_message = f"{timestamp} - {message}"
    print(log_message)
    try:
        with open(LOG_FILE_PATH, "a") as f:
            f.write(log_message + "\n")
    except Exception as e:
        print(f"Could not write to log file {LOG_FILE_PATH}: {e}")

# --- Main Monitoring Logic ---
def monitor_system(cpu_thresh, ram_thresh, disk_thresh, alert_email_to, smtp_user, smtp_pass):
    """Monitors system resources and sends alerts if thresholds are exceeded."""
    hostname = os.uname()[1]
    smtp_server_config = 'smtp.gmail.com'
    smtp_port_config = 587

    # CPU Usage
    current_cpu_usage = psutil.cpu_percent(interval=1)
    log_event(f"Current CPU Usage: {current_cpu_usage:.2f}%")
    if current_cpu_usage >= cpu_thresh:
        subject = f"ALERT: High CPU Usage on {hostname}"
        body = (f"CPU usage on server '{hostname}' is at {current_cpu_usage:.2f}%, "
                f"which exceeds the threshold of {cpu_thresh:.2f}%.")
        send_alert_email(subject, body, alert_email_to, smtp_user, smtp_server_config, smtp_port_config, smtp_user, smtp_pass)

    # RAM Usage
    memory_info = psutil.virtual_memory()
    current_ram_usage = memory_info.percent
    log_event(f"Current RAM Usage: {current_ram_usage:.2f}%")
    if current_ram_usage >= ram_thresh:
        subject = f"ALERT: High RAM Usage on {hostname}"
        body = (f"RAM usage on server '{hostname}' is at {current_ram_usage:.2f}%, "
                f"which exceeds the threshold of {ram_thresh:.2f}%.")
        send_alert_email(subject, body, alert_email_to, smtp_user, smtp_server_config, smtp_port_config, smtp_user, smtp_pass)

    # Disk Usage
    disk_info = psutil.disk_usage(DEFAULT_FILESYSTEM_TO_MONITOR)
    current_disk_usage = disk_info.percent
    log_event(f"Current Disk Usage for '{DEFAULT_FILESYSTEM_TO_MONITOR}': {current_disk_usage:.2f}%")
    if current_disk_usage >= disk_thresh:
        subject = f"ALERT: High Disk Usage on {hostname} ({DEFAULT_FILESYSTEM_TO_MONITOR})"
        body = (f"Disk usage for '{DEFAULT_FILESYSTEM_TO_MONITOR}' on server '{hostname}' is at {current_disk_usage:.2f}%, "
                f"which exceeds the threshold of {disk_thresh:.2f}%.")
        send_alert_email(subject, body, alert_email_to, smtp_user, smtp_server_config, smtp_port_config, smtp_user, smtp_pass)

# --- Main Execution ---
if __name__ == "__main__":
    try:
        (CPU_THRESHOLD, RAM_THRESHOLD, DISK_THRESHOLD,
         ALERT_EMAIL_TO, SMTP_USERNAME, SMTP_PASSWORD, CHECK_INTERVAL) = get_user_config()

        log_event("System Monitor Started.")
        log_event(f"Monitoring: CPU > {CPU_THRESHOLD}%, RAM > {RAM_THRESHOLD}%, Disk ('{DEFAULT_FILESYSTEM_TO_MONITOR}') > {DISK_THRESHOLD}%")
        log_event(f"Alerts will be sent to: {ALERT_EMAIL_TO} via {SMTP_USERNAME}")
        log_event(f"Check interval: {CHECK_INTERVAL} seconds.")
        log_event(f"Log file: {LOG_FILE_PATH}")


        while True:
            monitor_system(CPU_THRESHOLD, RAM_THRESHOLD, DISK_THRESHOLD,
                           ALERT_EMAIL_TO, SMTP_USERNAME, SMTP_PASSWORD)
            log_event(f"Waiting for {CHECK_INTERVAL} seconds before next check...")
            time.sleep(CHECK_INTERVAL)

    except KeyboardInterrupt:
        log_event("System Monitor stopped by user.")
    except Exception as e:
        log_event(f"An unexpected error occurred: {e}")
    finally:
        log_event("System Monitor shutting down.")
