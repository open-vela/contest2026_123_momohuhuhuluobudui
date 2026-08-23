CC ?= cc
CXX ?= c++
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Werror -pedantic -O2

ROOT := ..
TARGETS := test_core test_vision test_diagnostic_fingerprint \
	test_face_diagnostics test_face_fallback test_face_presence \
	test_storage test_display_ui \
	test_frame_timing test_lcd_timing test_lcd_transfer test_lcd_bounce \
	test_thread_priority test_display_preview test_rgb565_region \
	test_display_regions \
	test_espdl_backend_config test_espdl_error_check \
	test_espdl_pixel_mode test_espdl_roi_filter \
	test_espdl_alloc_policy \
	test_espdl_private_pool test_espdl_tie_selftest

.PHONY: all test clean
all: $(TARGETS)

test_core: test_core.c $(ROOT)/src/core.c $(ROOT)/include/agentguard/core.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_core.c $(ROOT)/src/core.c -o $@

test_vision: test_vision.c $(ROOT)/src/vision.c $(ROOT)/include/agentguard/vision.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_vision.c $(ROOT)/src/vision.c -o $@

test_diagnostic_fingerprint: test_diagnostic_fingerprint.c \
	$(ROOT)/src/diagnostic_fingerprint.c \
	$(ROOT)/include/agentguard/diagnostic_fingerprint.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_diagnostic_fingerprint.c \
		$(ROOT)/src/diagnostic_fingerprint.c -o $@

test_face_diagnostics: test_face_diagnostics.c \
	$(ROOT)/src/face_diagnostics.c $(ROOT)/src/diagnostic_fingerprint.c \
	$(ROOT)/include/agentguard/face_diagnostics.h \
	$(ROOT)/include/agentguard/diagnostic_fingerprint.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_face_diagnostics.c \
		$(ROOT)/src/face_diagnostics.c \
		$(ROOT)/src/diagnostic_fingerprint.c -pthread -o $@

test_face_fallback: test_face_fallback.c \
	$(ROOT)/src/face_fallback.c \
	$(ROOT)/include/agentguard/face_fallback.h \
	$(ROOT)/include/agentguard/vision.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_face_fallback.c \
		$(ROOT)/src/face_fallback.c -o $@

test_face_presence: test_face_presence.c \
	$(ROOT)/src/face_presence.c \
	$(ROOT)/include/agentguard/face_presence.h \
	$(ROOT)/include/agentguard/vision.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_face_presence.c \
		$(ROOT)/src/face_presence.c -o $@

test_storage: test_storage.c $(ROOT)/src/storage.c $(ROOT)/src/core.c \
	$(ROOT)/include/agentguard/storage.h $(ROOT)/include/agentguard/core.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_storage.c $(ROOT)/src/storage.c \
		$(ROOT)/src/core.c -o $@

test_display_ui: test_display_ui.c $(ROOT)/src/display_ui.c \
	$(ROOT)/include/agentguard/display_ui.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_display_ui.c \
		$(ROOT)/src/display_ui.c -o $@

test_frame_timing: test_frame_timing.c $(ROOT)/src/frame_timing.c \
	$(ROOT)/include/agentguard/frame_timing.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_frame_timing.c \
		$(ROOT)/src/frame_timing.c -o $@

test_lcd_timing: test_lcd_timing.c $(ROOT)/src/lcd_timing.c \
	$(ROOT)/include/agentguard/lcd_timing.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_lcd_timing.c \
		$(ROOT)/src/lcd_timing.c -o $@

test_lcd_transfer: test_lcd_transfer.c $(ROOT)/src/lcd_transfer.c \
	$(ROOT)/src/lcd_timing.c $(ROOT)/include/agentguard/lcd_transfer.h \
	$(ROOT)/include/agentguard/lcd_timing.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_lcd_transfer.c \
		$(ROOT)/src/lcd_transfer.c $(ROOT)/src/lcd_timing.c -o $@

test_lcd_bounce: test_lcd_bounce.c $(ROOT)/src/lcd_bounce.c \
	$(ROOT)/src/lcd_timing.c $(ROOT)/include/agentguard/lcd_bounce.h \
	$(ROOT)/include/agentguard/lcd_timing.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_lcd_bounce.c \
		$(ROOT)/src/lcd_bounce.c $(ROOT)/src/lcd_timing.c -o $@

test_thread_priority: test_thread_priority.c \
	$(ROOT)/src/thread_priority.c \
	$(ROOT)/include/agentguard/thread_priority.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_thread_priority.c \
		$(ROOT)/src/thread_priority.c -pthread -o $@

test_display_preview: test_display_preview.c \
	$(ROOT)/src/display_preview.c \
	$(ROOT)/include/agentguard/display_preview.h \
	$(ROOT)/include/agentguard/vision.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_display_preview.c \
		$(ROOT)/src/display_preview.c -o $@

test_rgb565_region: test_rgb565_region.c \
	$(ROOT)/src/rgb565_region.c \
	$(ROOT)/include/agentguard/rgb565_region.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_rgb565_region.c \
		$(ROOT)/src/rgb565_region.c -o $@

test_display_regions: test_display_regions.c \
	$(ROOT)/src/display_regions.c $(ROOT)/src/rgb565_region.c \
	$(ROOT)/include/agentguard/display_regions.h \
	$(ROOT)/include/agentguard/display_preview.h \
	$(ROOT)/include/agentguard/rgb565_region.h
	$(CC) $(CFLAGS) -I$(ROOT)/include test_display_regions.c \
		$(ROOT)/src/display_regions.c $(ROOT)/src/rgb565_region.c -o $@

test_espdl_backend_config: test_espdl_backend_config.cpp \
	$(ROOT)/third_party/esp-dl/dl/dl_define_private.hpp \
	$(ROOT)/compat/espdl/sdkconfig.h
	$(CXX) $(CXXFLAGS) -I$(ROOT)/compat/espdl \
		-I$(ROOT)/third_party/esp-dl/dl \
		$< -o $@

test_espdl_error_check: test_espdl_error_check.c \
	$(ROOT)/compat/espdl/esp_log.h
	$(CC) $(CFLAGS) -DNDEBUG -I$(ROOT)/compat/espdl $< -o $@

test_espdl_pixel_mode: test_espdl_pixel_mode.c \
	$(ROOT)/include/agentguard/espdl_pixel_mode.h
	$(CC) $(CFLAGS) -I$(ROOT)/include $< -o $@

test_espdl_roi_filter: test_espdl_roi_filter.cpp \
	$(ROOT)/third_party/human_face_detect/human_face_detect_roi.hpp
	$(CXX) $(CXXFLAGS) -I$(ROOT)/compat/espdl \
		-isystem $(ROOT)/third_party/esp-dl/dl \
		-isystem $(ROOT)/third_party/esp-dl/dl/tool/include \
		-isystem $(ROOT)/third_party/esp-dl/vision/detect \
		-I$(ROOT)/third_party/human_face_detect \
		-I$(ROOT)/include $< -o $@

test_espdl_alloc_policy: test_espdl_alloc_policy.c \
	$(ROOT)/compat/espdl/esp_heap_caps.h
	$(CC) $(CFLAGS) -I$(ROOT)/compat/espdl $< -o $@

test_espdl_private_pool: test_espdl_private_pool.cpp \
	$(ROOT)/src/espdl_private_pool.cpp \
	$(ROOT)/compat/espdl/espdl_private_pool.h \
	fakes/nuttx/mm/mm.h
	$(CXX) $(CXXFLAGS) -Ifakes -I$(ROOT)/compat/espdl \
		test_espdl_private_pool.cpp $(ROOT)/src/espdl_private_pool.cpp \
		-pthread -o $@

test_espdl_tie_selftest: test_espdl_tie_selftest.cpp \
	$(ROOT)/src/espdl_tie_selftest_logic.cpp \
	$(ROOT)/include/agentguard/espdl_tie_selftest.h
	$(CXX) $(CXXFLAGS) -I$(ROOT)/include \
		test_espdl_tie_selftest.cpp \
		$(ROOT)/src/espdl_tie_selftest_logic.cpp -o $@

test: $(TARGETS)
	./test_core
	./test_vision
	./test_diagnostic_fingerprint
	./test_face_diagnostics
	./test_face_fallback
	./test_face_presence
	./test_storage
	./test_display_ui
	./test_frame_timing
	./test_lcd_timing
	./test_lcd_transfer
	./test_lcd_bounce
	./test_thread_priority
	./test_display_preview
	./test_rgb565_region
	./test_display_regions
	./test_espdl_backend_config
	./test_espdl_error_check
	./test_espdl_pixel_mode
	./test_espdl_roi_filter
	./test_espdl_alloc_policy
	./test_espdl_private_pool
	./test_espdl_tie_selftest
	python3 test_espdl_tie_no_stdio.py
	python3 test_lcd_dma_config.py

clean:
	$(RM) $(TARGETS)
