# vim: syntax=cmake

SET(SRV_SRC
    kmpp_service.c
    kmpp_session_legacy.c
    kmpp_session.c
)

ADD_KMPP_SRC(${SRV_SRC})
