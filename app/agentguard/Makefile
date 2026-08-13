include $(APPDIR)/Make.defs

PROGNAME  = agentguard
PRIORITY  = SCHED_PRIORITY_DEFAULT
STACKSIZE = 16384
MODULE    = $(CONFIG_LVX_USE_DEMO_CONTEST2026_123_AGENTGUARD)

CFLAGS += -I$(APPDIR)/packages/demos/contest2026_123_agentguard/include
CSRCS = src/core.c src/vision.c src/storage.c src/display_ui.c
MAINSRC = src/agentguard_main.c

ifeq ($(CONFIG_AGENTGUARD_ESP_DL),y)
CXXEXT = .cpp
# NuttX's compatibility C++ headers shadow the GNU toolchain headers.  ESP-DL
# uses the real STL, so leave the compiler's native include search order intact.
CXXFLAGS := $(subst -isystem $(TOPDIR)/include/cxx,,$(CXXFLAGS))
ESPDL = third_party/esp-dl
ESPDL_INCLUDES = \
  $(ESPDL)/dl \
  $(ESPDL)/dl/tool/include \
  $(ESPDL)/dl/tensor/include \
  $(ESPDL)/dl/base \
  $(ESPDL)/dl/base/isa \
  $(ESPDL)/dl/base/isa/tie728 \
  $(ESPDL)/dl/base/isa/xtensa \
  $(ESPDL)/dl/math/include \
  $(ESPDL)/dl/model/include \
  $(ESPDL)/dl/module/include \
  $(ESPDL)/fbs_loader/include \
  $(ESPDL)/vision/detect \
  $(ESPDL)/vision/image \
  $(ESPDL)/vision/image/isa \
  third_party/human_face_detect \
  compat/espdl

CXXFLAGS += $(foreach dir,$(ESPDL_INCLUDES),-I$(dir))
CXXFLAGS += -I$(APPDIR)/crypto/mbedtls/mbedtls/include
CXXFLAGS += -Iinclude -std=gnu++20 -O3 -ffast-math \
  -fno-exceptions -fno-rtti \
  -Wno-array-bounds -Wno-deprecated-copy -Wno-strict-aliasing \
  -Wno-overloaded-virtual -Wno-format

CXXSRCS += src/vision_espdl.cpp src/espdl_compat.cpp \
  src/espdl_private_pool.cpp
CXXSRCS += $(filter-out $(ESPDL)/dl/tool/src/dl_tool_cache.cpp,\
  $(wildcard $(ESPDL)/dl/tool/src/*.cpp))
CXXSRCS += $(wildcard $(ESPDL)/dl/tensor/src/*.cpp)
CXXSRCS += $(wildcard $(ESPDL)/dl/base/*.cpp)
CXXSRCS += $(wildcard $(ESPDL)/dl/math/src/*.cpp)
CXXSRCS += $(wildcard $(ESPDL)/dl/model/src/*.cpp)
CXXSRCS += $(wildcard $(ESPDL)/dl/module/src/*.cpp)
CXXSRCS += $(ESPDL)/fbs_loader/src/fbs_loader.cpp
CXXSRCS += $(ESPDL)/vision/image/dl_image_preprocessor.cpp
CXXSRCS += $(ESPDL)/vision/image/dl_image_process.cpp
CXXSRCS += $(ESPDL)/vision/image/dl_image_pixel_cvt_dispatch_gray.cpp
CXXSRCS += $(ESPDL)/vision/image/dl_image_pixel_cvt_dispatch_rgb565.cpp
CXXSRCS += $(ESPDL)/vision/image/dl_image_pixel_cvt_dispatch_rgb888.cpp
CXXSRCS += $(ESPDL)/vision/detect/dl_detect_base.cpp
CXXSRCS += $(ESPDL)/vision/detect/dl_detect_postprocessor.cpp
CXXSRCS += $(ESPDL)/vision/detect/dl_detect_msr_postprocessor.cpp
CXXSRCS += $(ESPDL)/vision/detect/dl_detect_mnp_postprocessor.cpp
CXXSRCS += third_party/human_face_detect/human_face_detect.cpp

ASRCS += src/espdl_model.S
ASRCS += $(wildcard $(ESPDL)/dl/tool/isa/xtensa/*.S)
ASRCS += $(wildcard $(ESPDL)/dl/tool/isa/tie728/*.S)
ASRCS += $(wildcard $(ESPDL)/dl/base/isa/xtensa/*.S)
ASRCS += $(wildcard $(ESPDL)/dl/base/isa/tie728/*.S)

endif

include $(APPDIR)/Application.mk
