#!/bin/bash

# Script to run 5 shell scripts sequentially with logging and timing

# Configuration
LOG_FILE="execution.log"
scripts=("run_abc.sh" "run_alg1.sh" "run_alg2.sh" "run_etb.sh" "run_hybrid.sh")

# Function to log messages
log_message() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" | tee -a "$LOG_FILE"
}

# Start execution
log_message "Starting sequential execution of ${#scripts[@]} scripts"
log_message "=========================================="

# Initialize counters
total_start_time=$(date +%s)
success_count=0

# Loop through each script
for script in "${scripts[@]}"; do
    # Check if script exists and is executable
    if [ ! -x "./$script" ]; then
        log_message "ERROR: $script not found or not executable"
        exit 1
    fi
    
    log_message "Running $script..."
    script_start_time=$(date +%s)
    
    # Execute the script
    ./"$script" 2>&1 | tee -a "$LOG_FILE"
    exit_code=${PIPESTATUS[0]}
    
    script_end_time=$(date +%s)
    script_duration=$((script_end_time - script_start_time))
    
    if [ $exit_code -eq 0 ]; then
        log_message "✓ $script completed successfully (Duration: ${script_duration}s)"
        ((success_count++))
    else
        log_message "✗ $script failed with exit code $exit_code (Duration: ${script_duration}s)"
        log_message "Stopping execution due to failure..."
        exit $exit_code
    fi
    
    log_message "-----------------------------------"
done

total_end_time=$(date +%s)
total_duration=$((total_end_time - total_start_time))

log_message "=========================================="
log_message "All scripts completed successfully!"
log_message "Total execution time: ${total_duration}s"
log_message "Successful scripts: $success_count/${#scripts[@]}"