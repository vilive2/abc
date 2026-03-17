#!/bin/bash

design_dir="/home/vivek/designs/multi"
output_dir=""
selected_file="selected_designs.txt"  # File containing list of designs
log_file="$output_dir/execution_$(date +%Y%m%d_%H%M%S).log"  # Fixed date format

# Check if selected designs file exists
if [[ ! -f "$selected_file" ]]; then
    echo "Error: $selected_file not found!"
    exit 1
fi

# Create output directory if it doesn't exist
mkdir -p "$output_dir"

# Redirect all script output to both console and log file
exec > >(tee -a "$log_file") 2>&1

echo "Script started at $(date)"
echo "Design directory: $design_dir"
echo "Output directory: $output_dir"
echo "Reading designs from: $selected_file"
echo "----------------------------------------"

# Initialize counters
total_designs=0
processed=0
skipped=0
failed=0
not_found=0

# Read each line from the selected designs file
while IFS= read -r line || [[ -n "$line" ]]; do
    # Skip empty lines and comments
    if [[ -z "$line" || "$line" =~ ^[[:space:]]*# ]]; then
        continue
    fi
    
    # Trim whitespace
    base_filename=$(echo "$line" | xargs)
    total_designs=$((total_designs + 1))
    
    # Construct full path
    filename="$design_dir/$base_filename"
    output_file="${base_filename%.aig}.out"
    
    # Check if file exists in design directory
    if [[ ! -f "$filename" ]]; then
        echo "$(date): ERROR - File not found: $filename"
        not_found=$((not_found + 1))
        continue
    fi
    
    # Check if output file already exists in output_dir
    if [[ -f "$output_dir/$output_file" ]]; then
        echo "$(date): Skipping..${base_filename} (output already exists)."
        skipped=$((skipped + 1))
        continue
    fi
    
    echo "$(date): Running..${base_filename}."
    processed=$((processed + 1))
    
    # Run the command and capture any signals/errors
    {
        # ~/abc/abc -c "read $filename; bmc3 -a -g -T 3600" 
        ~/abc/abc -c "read $filename; poem -T 3600"
    } > "$output_dir/$output_file" 2>&1
    
    # Check the exit status
    exit_status=$?
    if [ $exit_status -ne 0 ]; then
        failed=$((failed + 1))
        echo "$(date): WARNING - $base_filename exited with status $exit_status" 
        echo "Check $output_file for details" 
        
        # Log specific signals if caught
        case $exit_status in
            130) echo "  -> Process interrupted (Ctrl+C)" ;;
            137|143) echo "  -> Process killed (SIGKILL/SIGTERM)" ;;
            139) echo "  -> Segmentation fault (SIGSEGV)" ;;
            134) echo "  -> Abort (SIGABRT)" ;;
            *) echo "  -> Exit code: $exit_status" ;;
        esac
    else
        echo "$(date): Completed successfully - $base_filename"
    fi
    
    sleep 1
    
done < "$selected_file"

echo "----------------------------------------"
echo "Script completed at $(date)"
echo ""
echo "Summary:"
echo "  Total designs in file: $total_designs"
echo "  Successfully processed: $((processed - failed))"
echo "  Failed: $failed"
echo "  Skipped (output exists): $skipped"
echo "  Not found in design_dir: $not_found"
echo ""
echo "Log file: $log_file"