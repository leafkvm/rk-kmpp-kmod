#!/bin/bash

#################################################
# Detect kernel path and version
#################################################
KERNEL_SEARCH_PATH=(
    /home/pub/kernel
    ~/work/kernel
    ~/code/kernel
)

FOUND_KERNEL=0
FOUND_TOOLCHAIN=0

if [ -z "$KERNEL_DIR" ]; then
    # try find kernel path in CMakeCache.txt
    if [ -f "CMakeCache.txt" ]; then
        KERNEL_DIR=`grep KERNEL_DIR\: CMakeCache.txt | awk -F '=' '{ print $2 }'`

        if [ -d "${KERNEL_DIR}" ]; then
            echo "use kernel tree from CMakeCache.txt : ${KERNEL_DIR}"
            FOUND_KERNEL=1
        fi
    fi
else
    FOUND_KERNEL=1
fi

if [ -z "$TOOLCHAIN_DIR" ]; then
    # try find kernel toolchain path in CMakeCache.txt
    if [ -f "CMakeCache.txt" ]; then
        TOOLCHAIN_DIR=`grep TOOLCHAIN_DIR\: CMakeCache.txt | awk -F '=' '{ print $2 }'`

        if [ -d "${TOOLCHAIN_DIR}" ]; then
            echo "use toolchain from CMakeCache.txt : ${TOOLCHAIN_DIR}"
            FOUND_TOOLCHAIN=1
        fi
    fi
else
    FOUND_TOOLCHAIN=1
fi

#################################################
# search possible path to get kernel source tree
#################################################
KERN_OPTION=""
KERN_COUNT=0

if [ $FOUND_KERNEL -eq 0 ]; then
    echo "trying to find kernel source in the following paths:"
    for KERN_BASE in ${KERNEL_SEARCH_PATH[@]};
    do
        echo "${KERN_BASE}"
    done

    echo "find valid kernel source:"

    for KERN_BASE in ${KERNEL_SEARCH_PATH[@]};
    do
        if [ -d ${KERN_BASE} ]; then
            KERN=`ls -r -d ${KERN_BASE}/kernel*/`

            for KERN_PATH in ${KERN[@]};
            do
                if [ -d ${KERN_PATH} ]; then
                    KERN_COUNT=$[${KERN_COUNT}+1]
                    KERN_OPT="${KERN_COUNT} - ${KERN_PATH}"

                    echo ${KERN_OPT}

                    KERN_OPTION+="${KERN_PATH} "
                fi
            done
        fi
    done
fi


case ${KERN_COUNT} in
    0)
        ;;
    1)
        KERNEL_DIR=${KERN_PATH[0]}
        FOUND_KERNEL=1

        echo "use kernel: ${KERNEL_DIR}"
        ;;
    *)
        read -p "select [1-${KERN_COUNT}] kernel used for compiling: " -ra KERN_INTPUT

        KERN_INDEX=0

        for KERN_PATH in ${KERN_OPTION[@]};
        do
            KERN_INDEX=$[${KERN_INDEX}+1]

            if [ "${KERN_INDEX}" -eq "${KERN_INTPUT}" ]; then
                echo "${KERN_INTPUT} - ${KERN_PATH} selected as KERNEL_DIR"
                KERNEL_DIR=${KERN_PATH}
                FOUND_KERNEL=1
                break
            fi
        done

        if [ $FOUND_KERNEL -eq 0 ]; then
            echo "invalid input option ${KERN_INTPUT}"
        fi
esac

if [ $FOUND_KERNEL -eq 0 ]; then
    echo "can not found any valid kernel source"
    exit 1
fi

#################################################
# try to detect kernel version
#################################################
KERN_MAKEFILE=${KERNEL_DIR}/Makefile
KERN_VERSION=0
KERN_PATCHLEVEL=0
KERN_SUBLEVEL=0

get_version_info()
{
    RET=$(sed -n -e '1,10p' "$1" | grep -m 1 -o "$2[[:blank:]]*=[[:blank:]]*[0-9]\+" | awk '{print $3}')

    echo $RET
}

if [ -f "${KERN_MAKEFILE}" ]; then
    KERN_VERSION=$(get_version_info ${KERN_MAKEFILE} VERSION)
    KERN_PATCHLEVEL=$(get_version_info ${KERN_MAKEFILE} PATCHLEVEL)
    KERN_SUBLEVEL=$(get_version_info ${KERN_MAKEFILE} SUBLEVEL)
else
    # kernel directory without Makefile
    echo "can not find Makefile in ${KERN_VERSION}"
    exit 1
fi

if [ $KERN_VERSION -ge 6 ]; then
    FOUND_KERNEL=1
elif [ $KERN_VERSION -ge 3 ]; then
    FOUND_KERNEL=1
else
    echo "can not support kernel before 3.10"
    FOUND_KERNEL=0
fi

if [ $FOUND_KERNEL -eq 0 ]; then
    echo -e "\e[1;31m No kernel path found. You should add your kernel path\e[0m"
    exit 1
else
    echo "kernel path   : $KERNEL_DIR"
    echo "kernel version: ${KERN_VERSION}.${KERN_PATCHLEVEL}.${KERN_SUBLEVEL}"
fi

#################################################
# Detect kernel path and version
#################################################
COMPILER_NAMES=(
    clang
    gcc
)

#################################################
# search possible path to get toolchain
#################################################
TOOLCHAIN_OPTION=""
TOOLCHAIN_COUNT=0
TOOLCHAIN_CROSS=""

if [ $FOUND_TOOLCHAIN -eq 0 ]; then
    echo "trying to find toolchain directory in the following paths:"
    for TOOLCHAIN_BASE in ${KERNEL_SEARCH_PATH[@]};
    do
        echo "${TOOLCHAIN_BASE}/"
    done

    echo "find valid toolchain directory:"

    for TOOLCHAIN_BASE in ${KERNEL_SEARCH_PATH[@]};
    do
        if [[ -d "${TOOLCHAIN_BASE}" && -d "${TOOLCHAIN_BASE}/prebuilts" ]]; then
            # search all directory with bin
            TOOLCHAIN_DIRS=$(find ${TOOLCHAIN_BASE}/prebuilts -maxdepth 5 -type d -name 'bin' ! -path '*python*' ! -path '*test*' ! -path '*usr*' -print)

            for TOOLCHAIN_PATH in ${TOOLCHAIN_DIRS[@]};
            do
                if [ ! -d ${TOOLCHAIN_PATH} ]; then
                    continue
                fi

                FOUND_COMPILER=0
                for COMPILER_NAME in ${COMPILER_NAMES[@]};
                do
                    COMPILER_BINS=$(find ${TOOLCHAIN_PATH} -type f -name "*${COMPILER_NAME}")
                    for COMPILER_BIN in ${COMPILER_BINS[@]};
                    do
                        if [ -f "${COMPILER_BIN}" ]; then
                            FOUND_COMPILER=1
                            break
                        fi
                    done

                    if [ "${FOUND_COMPILER}" = "1" ]; then
                        break
                    fi
                done
                if [ "${FOUND_COMPILER}" = "0" ]; then
                    continue
                fi

                TOOLCHAIN_CROSS=$(basename ${COMPILER_BIN})

                # check cross name
                TOOLCHAIN_COUNT=$[${TOOLCHAIN_COUNT}+1]
                TOOLCHAIN_OPT="${TOOLCHAIN_COUNT} - ${TOOLCHAIN_PATH} - ${TOOLCHAIN_CROSS}"

                echo ${TOOLCHAIN_OPT}

                TOOLCHAIN_OPTION+="${TOOLCHAIN_PATH} "
            done
        fi
    done
fi

case ${TOOLCHAIN_COUNT} in
    0)
        ;;
    1)
        TOOLCHAIN_DIR=${KERN_PATH[0]}
        FOUND_TOOLCHAIN=1

        echo "use toolchain: ${TOOLCHAIN_DIR}"
        ;;
    *)
        read -p "select [1-${TOOLCHAIN_COUNT}] toolchain used for compiling: " -ra TOOLCHAIN_INTPUT

        TOOLCHAIN_INDEX=0

        for TOOLCHAIN_PATH in ${TOOLCHAIN_OPTION[@]};
        do
            TOOLCHAIN_INDEX=$[${TOOLCHAIN_INDEX}+1]

            if [ "${TOOLCHAIN_INDEX}" -eq "${TOOLCHAIN_INTPUT}" ]; then
                echo "${TOOLCHAIN_INTPUT} - ${TOOLCHAIN_PATH} selected as TOOLCHAIN_DIR"
                TOOLCHAIN_DIR=${TOOLCHAIN_PATH}
                FOUND_TOOLCHAIN=1
                break
            fi
        done

        if [ $FOUND_TOOLCHAIN -eq 0 ]; then
            echo "invalid input option ${TOOLCHAIN_INTPUT}"
        fi
esac

CROSS_COMPILER_PREFIX=
KERNEL_MAKE_OPT=
if [ $FOUND_TOOLCHAIN -eq 0 ]; then
    echo "can not found any valid toolchain"
    exit 1
else
    echo "toolchain     : $TOOLCHAIN_DIR"

    CLANG_PATH=$(find "$TOOLCHAIN_DIR" -type f -name "clang" -print -quit)
    if [ -n "$CLANG_PATH" ]; then
        KERNEL_MAKE_OPT+="LLVM=1;LLVM_IAS=1"
        CROSS_COMPILER_PREFIX="aarch64-linux-gnu-"
        echo "cross_compile : $CROSS_COMPILER_PREFIX $KERNEL_MAKE_OPT"
    else
        GCC_PATH=$(find "$TOOLCHAIN_DIR" -type f -name "*-gcc" -print -quit)
        if [ -n "$GCC_PATH" ]; then
            CROSS_COMPILER_PREFIX=$(basename "$GCC_PATH" gcc)
            echo "cross_compile : $CROSS_COMPILER_PREFIX"
        else
            echo "cannot find any compiler in $TOOLCHAIN_DIR"
        fi
    fi
fi
