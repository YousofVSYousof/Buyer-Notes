mkdir build
cd build
clear
cmake ..
cmake --build . -j$(nproc)
./main
