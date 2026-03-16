#!/bin/bash

design_dir=""
output_dir=""
log_file="$output_dir/execution_$(date).log"  # Main log file for script events

# Redirect all script output to both console and log file
exec > >(tee -a "$log_file") 2>&1

echo "Script started at $(date)"
echo "Design directory: $design_dir"
echo "Output directory: $output_dir"
echo "----------------------------------------"

# Loop through each .aig file in the design directory
for filename in "$design_dir"/*.aig; do
    # Check if the file exists and is indeed a .aig file
    if [[ -f "$filename" && "$filename" =~ \.aig$ ]]; then
        # Extract just the filename without path
        base_filename=$(basename "$filename")
        output_file="${base_filename%.aig}.out"
        
        # Check if output file already exists in output_dir
        if [[ -f "$output_dir/$output_file" ]]; then
            echo "$(date): Skipping..${base_filename} (output already exists)."
            continue
        fi
        
        echo "$(date): Running..${base_filename}."
        
        # Run the command and capture any signals/errors
        {
            ~/abc/abc -c "read $filename; bmc3 -a -g -T 3600" 
        } > "$output_dir/$output_file" 2>&1
        
        # Check the exit status
        exit_status=$?
        if [ $exit_status -ne 0 ]; then
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
        fi
        
        sleep 1
    fi
done

echo "----------------------------------------"
echo "Script completed at $(date)"