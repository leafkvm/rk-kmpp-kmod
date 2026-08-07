# ----------------------------------------------------------------------------
# Create compile time information
# ----------------------------------------------------------------------------
STRING(TIMESTAMP current_year "%Y" UTC)
STRING(TIMESTAMP current_time "%Y-%m-%d %H:%M:%S")
MESSAGE(STATUS "current time: ${current_time}")

# ----------------------------------------------------------------------------
# Create git version information
# ----------------------------------------------------------------------------
SET(VERSION_CNT         0)
SET(VERSION_MAX_CNT     9)
SET(VERSION_INFO        "\"unknown mpp version for missing VCS info\"")
FOREACH(CNT RANGE ${VERSION_MAX_CNT})
    SET(VERSION_HISTORY_${CNT} "NULL")
ENDFOREACH(CNT)

IF(EXISTS "${PROJECT_SOURCE_DIR}/.git")
    FIND_PACKAGE(Git)
    IF(GIT_FOUND)
        # get current version info
        SET(GIT_LOG_FORMAT "%h author: %<|(30)%an %cd %s")

        EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} log -1 --oneline --date=short --pretty=format:${GIT_LOG_FORMAT}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE EXEC_OUT
            ERROR_VARIABLE EXEC_ERROR
            RESULT_VARIABLE EXEC_RET
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE)

        IF(NOT EXEC_RET)
            SET(VERSION_INFO ${EXEC_OUT})
            MESSAGE(STATUS "current version:")
            MESSAGE(STATUS "${VERSION_INFO}")
            STRING(REPLACE "\"" "\\\"" VERSION_INFO ${VERSION_INFO})
            SET(VERSION_INFO "\"${VERSION_INFO}\"")
        ELSE(NOT EXEC_RET)
            MESSAGE(STATUS "git ret ${EXEC_RET}")
            MESSAGE(STATUS "${EXEC_ERROR}")
        ENDIF(NOT EXEC_RET)

        SET(GIT_LOG_FORMAT "%h author: %<|(30)%an %cd %s %d")

        # get history version information
        # setup logs
        MESSAGE(STATUS "git version history:")
        FOREACH(CNT RANGE ${VERSION_MAX_CNT})
            EXECUTE_PROCESS(COMMAND ${GIT_EXECUTABLE} log HEAD~${CNT} -1 --oneline --date=short --pretty=format:${GIT_LOG_FORMAT}
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                OUTPUT_VARIABLE EXEC_OUT
                ERROR_VARIABLE EXEC_ERROR
                RESULT_VARIABLE EXEC_RET
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_STRIP_TRAILING_WHITESPACE)

            IF(NOT EXEC_RET)
                SET(VERSION_LOG ${EXEC_OUT})
                STRING(REPLACE "\"" "\\\"" VERSION_LOG ${VERSION_LOG})
                MESSAGE(STATUS ${VERSION_LOG})
                SET(VERSION_HISTORY_${CNT} "\"${VERSION_LOG}\"")
                math(EXPR VERSION_CNT "${VERSION_CNT}+1")
            ENDIF(NOT EXEC_RET)
        ENDFOREACH(CNT)
        MESSAGE(STATUS "total ${VERSION_CNT} git version recorded")
    ENDIF(GIT_FOUND)
ENDIF(EXISTS "${PROJECT_SOURCE_DIR}/.git")

CONFIGURE_FILE(
    "${PROJECT_SOURCE_DIR}/build/cmake/version.in"
    "${PROJECT_SOURCE_DIR}/include/kmpp_version.h"
    @ONLY
)
