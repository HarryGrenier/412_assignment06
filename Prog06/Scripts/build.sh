#!/bin/bash

# Detecting the platform
platform=$(uname)
echo "Detected platform: $platform"

# Check if Builds directory exists, if not, create it
if [ ! -d "../Builds" ]; then
    echo "Builds directory not found. Creating..."
    mkdir ../Builds
fi

g++ -Wall -std=c++20 ./../Programs/C++_thread/Version1/*.cpp -lGL -lglut -o ./../Builds/C++_Version1
g++ -Wall -std=c++20 ./../Programs/C++_thread/Version2/*.cpp -lGL -lglut -o ./../Builds/C++_Version2
g++ -Wall -std=c++20 ./../Programs/C++_thread/Version3/*.cpp -lGL -lglut -o ./../Builds/C++_Version3
g++ -Wall -std=c++20 ./../Programs/pthread/Version1/*.cpp -lGL -lglut -o ./../Builds/P_Version1
g++ -Wall -std=c++20 ./../Programs/pthread/Version2/*.cpp -lGL -lglut -o ./../Builds/P_Version2
g++ -Wall -std=c++20 ./../Programs/pthread/Version3/*.cpp -lGL -lglut -o ./../Builds/P_Version3

chmod +x ./../Builds

