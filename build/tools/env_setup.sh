#!/bin/bash

#################################################
# Arguments process
#################################################
while [ $# -gt 0 ]; do
    case $1 in
        --help | -h)
            echo "Execute make-xxx.sh in *arm/* or *aarch64/* with some args."
            echo "  use --cmake to specify which cmake to use"
            echo "  use --kernel to set KERNEL_DIR"
            echo "  use --ndk to set ANDROID_NDK"
            echo "  use --debug to enable debug build"
            echo "  use --one_ko to enable one ko build"
            echo "  use --trace to enable verbose make process log"
            echo "  use -B or --rebuild to rebuild after clean"
            exit 1
            ;;
        --cmake)
            CMAKE_PROGRAM=$2
            shift
            ;;
        --kernel)
            KERNEL_DIR=$2
            shift
            ;;
        --toolchain)
            TOOLCHAIN_DIR=$2
            shift
            ;;
        --ndk)
            ANDROID_NDK=$2
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            ;;
        --one_ko)
            MODULE_TYPE="one"
            ;;
        --trace)
            VERBOSE_MAKEFILE="ON"
            ;;
        -B)
            if [ -f "CMakeCache.txt" ]; then
                rm CMakeCache.txt
            fi
            ;;
        --rebuild)
            ${MAKE_PROGRAM} clean
            if [ -f "CMakeCache.txt" ]; then
                rm CMakeCache.txt
            fi
            shift
            ;;
    esac
    shift
done

#################################################
# Detect make
#################################################
if [ -z $MAKE_PROGRAM ]; then
    MAKE_PROGRAM=`which make`
fi

if [ -z ${MAKE_PROGRAM} ]; then
    echo -e "\e[1;31m make is invalid, please check!\e[0m"
    exit 1
fi

#################################################
# Detect cmake version
#################################################
if [ -z $CMAKE_PROGRAM ]; then
    CMAKE_PROGRAM=`which cmake`
fi

CMAKE_VERSION=$(${CMAKE_PROGRAM} --version | grep "version" | cut -d " " -f 3)
CMAKE_MAJOR_VERSION=`echo ${CMAKE_VERSION} | cut -d "." -f 1`
CMAKE_MINOR_VERSION=`echo ${CMAKE_VERSION} | cut -d "." -f 2`

if [ -z ${CMAKE_VERSION} ]; then
    echo -e "\e[1;31m cmake in ${CMAKE_PROGRAM} is invalid, please check!\e[0m"
    exit 1
else
    echo "Found cmake in ${CMAKE_PROGRAM}, version: ${CMAKE_VERSION}"
fi

CMAKE_PARALLEL_ENABLE=0
if [ ${CMAKE_MAJOR_VERSION} -ge 3 ] && [ ${CMAKE_MINOR_VERSION} -ge 12 ]; then
    CMAKE_PARALLEL_ENABLE=1
fi
