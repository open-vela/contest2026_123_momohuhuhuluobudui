/* SPDX-License-Identifier: Apache-2.0 */

  .section .rodata.agentguard_espdl,"a",@progbits
  .balign 16
  .global _binary_human_face_detect_espdl_start
  .global _binary_human_face_detect_espdl_end
_binary_human_face_detect_espdl_start:
  .incbin "third_party/human_face_detect/human_face_detect.espdl"
_binary_human_face_detect_espdl_end:

  .balign 16
  .global _binary_human_face_rgb565be_start
  .global _binary_human_face_rgb565be_end
_binary_human_face_rgb565be_start:
  .incbin "third_party/human_face_detect/human_face_rgb565be.bin"
_binary_human_face_rgb565be_end:
  .balign 16
