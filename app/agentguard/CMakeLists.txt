if(CONFIG_LVX_USE_DEMO_CONTEST2026_123_AGENTGUARD)
  nuttx_add_application(
    NAME agentguard
    STACKSIZE 16384
    PRIORITY 100
    SRCS src/agentguard_main.c src/core.c src/vision.c src/storage.c
         src/display_ui.c src/lcd_dma_buffer.c
    INCLUDE_DIRECTORIES include
  )
endif()
