#!/bin/bash
BUILD_TYPE="Release"
MODULE_TYPE=${MODULE_TYPE:-"multi"}
VERBOSE_MAKEFILE="OFF"

for ARG in "$@"; do
  if [[ "$ARG" == "-c" ]]; then
    clear
  fi
done

source ../tools/env_setup.sh
source ../tools/kernel_setup.sh

if [ -z "${FOUND_KERNEL}" ]; then
    exit 1
fi
if [ -z "${FOUND_TOOLCHAIN}" ]; then
    exit 1
fi

${CMAKE_PROGRAM} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCROSS_COMPILER_PREFIX=${CROSS_COMPILER_PREFIX} \
    -DKERNEL_DIR=${KERNEL_DIR} \
    -DKERNEL_MAKE_OPT=${KERNEL_MAKE_OPT} \
    -DTOOLCHAIN_DIR=${TOOLCHAIN_DIR} \
    -DARCH_TYPE=arm \
    -DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM} \
    -DCMAKE_VERBOSE_MAKEFILE=${VERBOSE_MAKEFILE} \
    -DMODULE_BUILD_MODE=${MODULE_TYPE} \
    ../../

if [ "${CMAKE_PARALLEL_ENABLE}" = "0" ]; then
    ${CMAKE_PROGRAM} --build .
else
    ${CMAKE_PROGRAM} --build . -j
fi
