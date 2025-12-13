# clean
rm -rf build
# Create dir if needed
mkdir -p build
# Set up a build dir and generate build files
cd build && cmake ..
# run make to rebuild the pass
make
