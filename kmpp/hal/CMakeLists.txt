# vim: syntax=cmake

ADD_SUBDIRECTORY(srv)

SET(HAL_SRC
    kmpp_hal.c
)

ADD_KMPP_SRC(${HAL_SRC})
