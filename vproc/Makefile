# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

$(VEPU_CORE)_PP_ENABLE = y
EXTRA_CFLAGS += -D$(VEPU_CORE)_PP_ENABLE
${KMPP_SRC}-objs += vproc/pp/vepu_pp_common.o

${KMPP_SRC}-$(RKVEPU540C_PP_ENABLE) += vproc/pp/vepu_pp.o
${KMPP_SRC}-$(RKVEPU500_PP_ENABLE) += vproc/pp/vepu500_pp.o
${KMPP_SRC}-$(RKVEPU511_PP_ENABLE) += vproc/pp/vepu511_pp.o
