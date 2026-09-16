if(CONFIG_LVX_USE_DEMO_CONTEST2026_123_AGENTGUARD)
  nuttx_add_application(
    NAME agentguard
    STACKSIZE 16384
    PRIORITY 100
    SRCS src/agentguard_main.c src/button_device.c src/button_gesture.c src/core.c src/vision.c src/vision_health.c
         src/storage.c src/display_ui.c
    INCLUDE_DIRECTORIES include
  )
endif()
