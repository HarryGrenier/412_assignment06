#!/bin/bash

# Check if the correct number of arguments is provided
if [ "$#" -ne 4 ]; then
    echo "Usage: $0 <path_to_executable> <num_threads> <output_image_path> <path_to_image_folder>"
    exit 1
fi

# Assign arguments to variables
EXECUTABLE_PATH=$1
NUM_THREADS=$2
OUTPUT_IMAGE_PATH=$3
IMAGE_FOLDER_PATH=$4

# Construct the command
COMMAND="$EXECUTABLE_PATH $NUM_THREADS $OUTPUT_IMAGE_PATH $IMAGE_FOLDER_PATH/*"

# Execute the command
$COMMAND
