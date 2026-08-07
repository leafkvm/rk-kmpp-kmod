# vim: syntax=cmake

FUNCTION(ADD_MODULE_CFLAGS)
    SET(args ${ARGN})
    GET_PROPERTY(current_cflags GLOBAL PROPERTY KMPP_MODULE_CFLAGS)

    FOREACH(flag IN LISTS args)
        SET(current_cflags "${current_cflags} ${flag}")
    ENDFOREACH()

    ADD_DEFINITIONS(${ARGN})
    SET_PROPERTY(GLOBAL PROPERTY KMPP_MODULE_CFLAGS "${current_cflags}")
ENDFUNCTION()

# Function: KMPP_OPTIONAL_MODULE
# Args: CONFIG_NAME (First Arg), SOURCE_FILES (Remaining Args)
FUNCTION(KMPP_OPTIONAL_MODULE CONFIG_NAME)
    SET(CONFIG_VALUE ${${CONFIG_NAME}})
    # Check if the feature is enabled
    IF(CONFIG_VALUE STREQUAL "y")
        SET(DEFINITION_FLAG "-D${CONFIG_NAME}")
        ADD_MODULE_CFLAGS(${DEFINITION_FLAG})
        SET(SOURCE_FILES ${ARGN})
        SET(KMPP_SRV_SRC ${KMPP_SRV_SRC} ${SOURCE_FILES} PARENT_SCOPE)
    ENDIF()
ENDFUNCTION()

FUNCTION(ADD_KBUILD MODULE_NAME)
    SET(args ${ARGN})
    SET(add_src 0)
    SET(add_dep 0)
    SET(add_ldflag 0)

    SET(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

    SET(MODULE_KO "${MODULE_NAME}.ko")
    SET(MODULE_DIR "${CMAKE_CURRENT_BINARY_DIR}/build")
    SET(MODULE_SRCS "")
    SET(MODULE_DEPS "")
    SET(MODULE_LDFLAG "")
    SET(MODULE_OBJS "")
    SET(MODULE_CFLAG "")
    SET(MODULE_EXTRA_SYM "")
    SET(MODULE_BYPRODUCTS "")
    SET(MODULE_OBJS_INFO "")

    FOREACH(arg IN LISTS args)
        IF(arg STREQUAL "SRC")
            SET(add_src 1)
            SET(add_dep 0)
            SET(add_ldflag 0)
        ELSEIF(arg STREQUAL "DEPS")
            SET(add_src 0)
            SET(add_dep 1)
            SET(add_ldflag 0)
        ELSEIF(arg STREQUAL "LDFLAG")
            SET(add_src 0)
            SET(add_dep 0)
            SET(add_ldflag 1)
        ELSE()
            IF(add_src)
                LIST(APPEND MODULE_SRCS "${arg}")
            ELSEIF(add_dep)
                LIST(APPEND MODULE_DEPS "${arg}")
            ELSEIF(add_ldflag)
                LIST(APPEND MODULE_LDFLAG "${arg}")
            ENDIF()
        ENDIF()
    ENDFOREACH()

    STRING(STRIP "${MODULE_SRCS}" MODULE_SRCS)
    STRING(STRIP "${MODULE_DEPS}" MODULE_DEPS)

    FILE(MAKE_DIRECTORY ${MODULE_DIR})

    GET_DIRECTORY_PROPERTY(DIR_DEFS COMPILE_DEFINITIONS)
    FOREACH(d ${DIR_DEFS})
        SET(MODULE_CFLAG "${MODULE_CFLAG} -D${d}")
    ENDFOREACH()

    GET_DIRECTORY_PROPERTY(DIR_DEFS INCLUDE_DIRECTORIES )
    FOREACH(d ${DIR_DEFS})
        SET(MODULE_CFLAG "${MODULE_CFLAG} -I${d}")
    ENDFOREACH()

    GET_PROPERTY(GLOBAL_MODULE_CFLAGS GLOBAL PROPERTY KMPP_MODULE_CFLAGS)
    IF(GLOBAL_MODULE_CFLAGS)
        SET(MODULE_CFLAG "${MODULE_CFLAG} ${GLOBAL_MODULE_CFLAGS}")
    ENDIF()

    IF(${ARCH_TYPE} STREQUAL "arm64")
        SET(MODULE_CFLAG "${MODULE_CFLAG} -mno-outline-atomics ")
    ENDIF()

    FOREACH(src ${MODULE_SRCS})
        IF(NOT IS_ABSOLUTE ${src})
            SET(src "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        ENDIF()

        GET_FILENAME_COMPONENT(name ${src} NAME)

        FILE(RELATIVE_PATH dst_rel_path ${SOURCE_DIR} ${src})
        GET_FILENAME_COMPONENT(rel_dir ${dst_rel_path} DIRECTORY)

        IF(NOT EXISTS "${MODULE_DIR}/${rel_dir}")
            FILE(MAKE_DIRECTORY ${MODULE_DIR}/${rel_dir})
        ENDIF()

        SET(dst "${MODULE_DIR}/${dst_rel_path}")

        EXECUTE_PROCESS(COMMAND bash -c "ln -sf ${src} ${dst}")

        GET_FILENAME_COMPONENT(name_we ${dst} NAME_WE)
        SET(obj "./${rel_dir}/${name_we}.o")

        STRING(APPEND MODULE_OBJS "${obj} ")

        LIST(APPEND MODULE_BYPRODUCTS
            "${MODULE_DIR}/${obj}"
            "${MODULE_DIR}/${obj}.cmd"
        )
    ENDFOREACH()

    LIST(LENGTH MODULE_SRCS OBJS_FILE_COUNT)
    IF(${OBJS_FILE_COUNT} GREATER 1)
        SET(MODULE_OBJS_INFO "${MODULE_NAME}-objs := ${MODULE_OBJS}")
    ENDIF()

    FOREACH(d ${MODULE_DEPS})
        GET_PROPERTY(dep_dir TARGET ${d} PROPERTY KMPP_LOCATION)
        STRING(APPEND MODULE_EXTRA_SYM "${dep_dir}/Module.symvers ")
    ENDFOREACH()

    LIST(APPEND MODULE_BYPRODUCTS
        "${MODULE_DIR}/.${MODULE_KO}.cmd"
        "${MODULE_DIR}/.${MODULE_NAME}.mod.o.cmd"
        "${MODULE_DIR}/.${MODULE_NAME}.o.cmd"
        "${MODULE_DIR}/.built-in.o.cmd"
        "${MODULE_DIR}/${MODULE_NAME}.mod.c"
        "${MODULE_DIR}/${MODULE_NAME}.mod.o"
        "${MODULE_DIR}/${MODULE_NAME}.o"
        "${MODULE_DIR}/built-in.o"
        "${MODULE_DIR}/Module.symvers"
        "${MODULE_DIR}/modules.order"
        )

    CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/build/cmake/Kbuild.in ${MODULE_DIR}/Kbuild @ONLY)

    SET(KBUILD_CMD
        PATH=$ENV{PATH}
        ${CMAKE_MAKE_PROGRAM}
        modules
        V=1
        -C ${KERNEL_DIR}
        ARCH=${ARCH_TYPE}
        CROSS_COMPILE=${CROSS_COMPILER_PREFIX}
        ${MODULE_LDFLAG}
        ${KERNEL_MAKE_OPT}
        M=${MODULE_DIR}
        src=${MODULE_DIR}
        )

    ADD_CUSTOM_COMMAND(
        COMMAND ${KBUILD_CMD}
        COMMENT "Building ${MODULE_KO}"
        OUTPUT ${MODULE_DIR}/${MODULE_KO}
        DEPENDS ${MODULE_SRCS}
        WORKING_DIRECTORY ${MODULE_DIR}
        BYPRODUCTS ${MODULE_BYPRODUCTS}
        VERBATIM
        )

    ADD_CUSTOM_TARGET(
        ${MODULE_NAME} ALL
        COMMAND ${KBUILD_CMD}
        DEPENDS ${MODULE_DIR}/${MODULE_KO}
        DEPENDS ${MODULE_DEPS}
        )

    SET_TARGET_PROPERTIES(
        ${MODULE_NAME}
        PROPERTIES
        OUTPUT_NAME         ${MODULE_NAME}
        KMPP_LOCATION       ${MODULE_DIR}
        MODULE_KO           ""
        MODULE_DIR          ""
        MODULE_SRCS         ""
        MODULE_DEPS         ""
        MODULE_LDFLAG       ""
        MODULE_OBJS         ""
        MODULE_CFLAG        ""
        MODULE_EXTRA_SYM    ""
        MODULE_BYPRODUCTS   ""
        )
ENDFUNCTION()

FUNCTION(ADD_KMPP_SRC)
    SET(args ${ARGN})
    GET_PROPERTY(ko_src GLOBAL PROPERTY "KMPP_KO_SRC")

    FOREACH (f IN LISTS args)
        IF(NOT IS_ABSOLUTE ${f})
            SET(abs ${CMAKE_CURRENT_SOURCE_DIR}/${f})
        ELSE()
            SET(abs ${f})
        ENDIF()
        LIST(APPEND ko_src "${abs}")
    ENDFOREACH()

    SET_PROPERTY(GLOBAL PROPERTY KMPP_KO_SRC ${ko_src})
ENDFUNCTION()

FUNCTION(ADD_KMPP_LDFLAG)
    SET(args ${ARGN})
    GET_PROPERTY(ko_ldflag GLOBAL PROPERTY "KMPP_KO_LDFLAG")

    FOREACH (flag IN LISTS args)
        LIST(APPEND ko_ldflag ${flag})
    ENDFOREACH()

    SET_PROPERTY(GLOBAL PROPERTY KMPP_KO_LDFLAG ${ko_ldflag})
ENDFUNCTION()

FUNCTION(CHECK_KERNEL_CONFIG KERNEL_DIR CONFIG_NAME OUTPUT_VAR)
    SET(KERNEL_CONFIG_PATH "${KERNEL_DIR}/.config")

    IF(NOT EXISTS "${KERNEL_CONFIG_PATH}")
        MESSAGE(WARNING "Kernel config file not found: ${KERNEL_CONFIG_PATH}")
        SET(${OUTPUT_VAR} FALSE PARENT_SCOPE)
        RETURN()
    ENDIF()

    FILE(STRINGS "${KERNEL_CONFIG_PATH}" CONFIG_LINES)
    SET(IS_ENABLED FALSE)
    FOREACH(line IN LISTS CONFIG_LINES)
        IF(line MATCHES "^${CONFIG_NAME}=(y|m)$")
            SET(IS_ENABLED TRUE)
            ADD_DEFINITIONS(-DCONFIG_${CONFIG_NAME})
            BREAK()
        ENDIF()
    ENDFOREACH()

    SET(${OUTPUT_VAR} ${IS_ENABLED} PARENT_SCOPE)
ENDFUNCTION()