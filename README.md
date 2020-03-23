# PA3

**Author:** Liam Nestelroad
**Date:** March 15, 2020. Day 2 in state of national emergency

## Building and Running

To build and run this, you will need to have the following packages installed:
1. build-essential (for ubuntu)
2. cmake
3. gcc

Once you have navigated to the top directory (../PA3), you will first need to run the following:  
```[bash]
rm -rf build && cmake -H. -Bbuild
```  
This will first remove any preexisting build dirs then the second command will recreate a new build dir and run cmake. Once this set is complete and all of the make files are generated, you will simply need to run `make` in the build dir. The compiled files will be located in the build directory. To run the binaries, just type the following command:  
```[bash]
./multi-lookup <num of requestors> <num of resolvers>
```