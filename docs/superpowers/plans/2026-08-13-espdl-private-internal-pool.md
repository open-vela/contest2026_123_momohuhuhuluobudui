# ESP-DL Private Internal Pool Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give ESP-DL a private 96 KiB internal-DRAM heap without enabling the NuttX global separate-heap switch or changing task-stack allocation.

**Architecture:** A focused private-pool module owns aligned BSS storage and initializes a NuttX `mm_heap_s` with `pthread_once`. The ESP-IDF compatibility layer routes internal/SIMD/DMA requests to this module and leaves default/SPIRAM requests on the common heap. Host tests compile the production module against a fake NuttX heap API before the accelerated backend is restored.

**Tech Stack:** NuttX `mm_*` API, ESP32-S3 internal BSS, ESP-DL 3.2.0, pthreads, C++20/C11 tests, GNU Make.

## Global Constraints

- Keep `CONFIG_XTENSA_IMEM_USE_SEPARATE_HEAP` disabled.
- Reserve exactly 96 KiB for the private ESP-DL pool, aligned to 16 bytes.
- Do not alter global malloc, task stacks, camera buffers, LCD buffers, or networking allocations.
- Internal, SIMD, and DMA requests must not fall back to PSRAM on exhaustion.
- Correctness on the embedded reference and live camera takes precedence over inference latency.
- Restore the prior mixed backend immediately if the private-pool firmware fails to boot or display the LCD.

---

### Task 1: Private Pool Module with Fake NuttX Heap Tests

**Files:**
- Create: `app/agentguard/compat/espdl/espdl_private_pool.h`
- Create: `app/agentguard/src/espdl_private_pool.cpp`
- Create: `app/agentguard/tests/fakes/nuttx/mm/mm.h`
- Create: `app/agentguard/tests/test_espdl_private_pool.cpp`
- Modify: `app/agentguard/tests/Makefile`

**Interfaces:**
- Produces: `ag_espdl_private_malloc`, `ag_espdl_private_calloc`, `ag_espdl_private_aligned_alloc`, `ag_espdl_private_aligned_calloc`, `ag_espdl_private_owns`, `ag_espdl_private_free`, `ag_espdl_private_free_size`, and `ag_espdl_private_largest_free_block`.
- Consumes: NuttX `mm_initialize`, `mm_malloc`, `mm_memalign`, `mm_free`, `mm_heapmember`, and `mm_mallinfo`.

- [ ] **Step 1: Add the public interface, fake heap header, failing test, and Makefile target**

The public header declares the eight functions with C linkage. The fake NuttX
header declares a minimal `mm_heap_s`, `mm_heap_config_s`, `mallinfo`, and the
six required `mm_*` calls. The test defines fake calls over the storage passed
to `mm_initialize_heap`, fills ordinary allocations with `0xa5`, and records
initialization/free counts.

Test these exact behaviors:

```cpp
assert(fake_initialize_count == 0);
void *plain = ag_espdl_private_malloc(32);
assert(plain != nullptr);
assert(fake_initialize_count == 1);
assert(ag_espdl_private_owns(plain));

void *aligned = ag_espdl_private_aligned_alloc(32, 64);
assert(aligned != nullptr);
assert(reinterpret_cast<uintptr_t>(aligned) % 32 == 0);

auto *zeroed = static_cast<unsigned char *>(
  ag_espdl_private_calloc(16, 4));
for (size_t i = 0; i < 64; ++i) assert(zeroed[i] == 0);

assert(ag_espdl_private_calloc(SIZE_MAX, 2) == nullptr);
assert(ag_espdl_private_aligned_calloc(16, SIZE_MAX, 2) == nullptr);
assert(!ag_espdl_private_owns(nullptr));
int outside;
assert(!ag_espdl_private_owns(&outside));

ag_espdl_private_free(plain);
assert(fake_free_count == 1);
assert(ag_espdl_private_free_size() <= 96u * 1024u);
assert(ag_espdl_private_largest_free_block() <= 96u * 1024u);
```

Add `test_espdl_private_pool` to `TARGETS`, compile it as C++20 with
`-Ifakes -I../compat/espdl`, and link `../src/espdl_private_pool.cpp` plus
`-pthread`. Execute it from `make test`.

- [ ] **Step 2: Run the private-pool target and verify RED**

Run: `make -C app/agentguard/tests test_espdl_private_pool`

Expected: link failure because the eight `ag_espdl_private_*` functions do not
exist.

- [ ] **Step 3: Implement the minimal production private pool**

Use:

```cpp
constexpr size_t kPrivatePoolBytes = 96u * 1024u;
alignas(16) unsigned char g_private_pool[kPrivatePoolBytes];
pthread_once_t g_private_pool_once = PTHREAD_ONCE_INIT;
mm_heap_s *g_private_heap;
```

The once callback calls `mm_initialize("espdl-private", g_private_pool,
sizeof(g_private_pool))`. Every operation first calls `pthread_once`; it
returns failure safely if that call fails or `g_private_heap` is null. Reject
`count * size` overflow before calloc and aligned calloc. Zero successful
calloc allocations with `memset`. `ag_espdl_private_owns(nullptr)` returns
false, and `ag_espdl_private_free(nullptr)` does nothing.

- [ ] **Step 4: Run the full host suite and verify GREEN**

Run: `make -C app/agentguard/tests clean`

Run: `make -C app/agentguard/tests test`

Expected: all seven test programs pass.

- [ ] **Step 5: Commit the private-pool module and tests**

```bash
git add app/agentguard/compat/espdl/espdl_private_pool.h \
  app/agentguard/src/espdl_private_pool.cpp \
  app/agentguard/tests/fakes/nuttx/mm/mm.h \
  app/agentguard/tests/test_espdl_private_pool.cpp \
  app/agentguard/tests/Makefile
git commit -m "feat: add ESP-DL private internal pool"
```

### Task 2: Compatibility-Layer Routing and Accelerated Backend

**Files:**
- Modify: `app/agentguard/src/espdl_compat.cpp`
- Modify: `app/agentguard/Makefile`
- Modify: `app/agentguard/compat/espdl/sdkconfig.h`
- Modify: `app/agentguard/tests/test_espdl_backend_config.cpp`
- Modify: `tools/apply_agentguard_config.sh`

**Interfaces:**
- Consumes: all eight `ag_espdl_private_*` functions from Task 1 and `ag_espdl_caps_require_internal`.
- Produces: capability-correct `heap_caps_*` routing while the global Xtensa separate heap stays disabled; both convolution families use TIE728.

- [ ] **Step 1: Change backend assertions back to full TIE728 and verify RED**

Require both force-C flags to equal zero:

```cpp
static_assert(CONFIG_AGENTGUARD_ESP_DL_FORCE_C_CONV == 0,
              "ordinary convolution must retain TIE728 acceleration");
static_assert(CONFIG_AGENTGUARD_ESP_DL_FORCE_C_DEPTHWISE == 0,
              "depthwise convolution must retain TIE728 acceleration");
```

Run: `make -C app/agentguard/tests test_espdl_backend_config`

Expected: the ordinary-convolution assertion fails while its force-C flag is
still 1 in the recovery configuration.

- [ ] **Step 2: Route capability calls to the private pool**

Add `src/espdl_private_pool.cpp` to the ESP-DL `CXXSRCS` in the application
Makefile. In `espdl_compat.cpp`, replace every `xtensa_imm_*` branch with the
matching private-pool operation. For malloc and aligned allocation:

```cpp
if (ag_espdl_caps_require_internal(caps))
  {
    return ag_espdl_private_malloc(size);
  }
```

Use private calloc functions directly for internal capability requests.
`heap_caps_free` selects `ag_espdl_private_free` when
`ag_espdl_private_owns(memory)` is true and otherwise calls `free`. Internal
free and largest-block queries delegate to the private pool. Remove the
`<arch/arch.h>` dependency from `espdl_compat.cpp`.

- [ ] **Step 3: Restore both convolution families to TIE728 and retain the safe global config**

Set both force-C flags to zero in `sdkconfig.h`. Keep
`apply_agentguard_config.sh` explicitly disabling
`XTENSA_IMEM_USE_SEPARATE_HEAP`, with the boot-crash explanation already
recorded beside it.

- [ ] **Step 4: Run host tests and static checks**

Run: `make -C app/agentguard/tests clean`

Run: `make -C app/agentguard/tests test`

Run: `git diff --check`

Expected: all seven tests pass and the diff check emits no errors.

- [ ] **Step 5: Commit routing, backend, and recovery corrections**

```bash
git add app/agentguard/src/espdl_compat.cpp app/agentguard/Makefile \
  app/agentguard/compat/espdl/sdkconfig.h \
  app/agentguard/tests/test_espdl_backend_config.cpp \
  tools/apply_agentguard_config.sh
git commit -m "fix: isolate ESP-DL internal allocations"
```

### Task 3: Clean Build, Memory-Map Verification, and Device Test

**Files:**
- Modify at build time: `/home/yhx/Desktop/openvela/nuttx/.config`
- Verify: `/home/yhx/Desktop/openvela/nuttx/nuttx`
- Verify: `/home/yhx/Desktop/openvela/nuttx/nuttx.bin`

**Interfaces:**
- Consumes: private pool, routed compatibility API, and dual-TIE728 backend.
- Produces: a flashed diagnostic firmware that boots normally and runs ESP-DL from a private internal pool.

- [ ] **Step 1: Apply safe product configuration and clean-build**

Run the project configuration script and verify `.config` says
`# CONFIG_XTENSA_IMEM_USE_SEPARATE_HEAP is not set`. Clean the NuttX build,
activate `/home/yhx/Desktop/openvela/myenv/bin/activate`, and run
`make -C /home/yhx/Desktop/openvela/nuttx -j8`.

Expected: `nuttx.bin` is generated successfully.

- [ ] **Step 2: Verify the ELF and image**

Use `xtensa-esp32s3-elf-nm` to verify:

- `_sheap` remains below the internal DRAM heap end;
- `g_private_pool` occupies 98304 bytes in internal BSS;
- `mm_initialize_heap`, `mm_malloc`, `mm_memalign`, `mm_free`,
  `mm_heapmember`, and `mm_mallinfo` are linked;
- ordinary and depthwise objects both reference `dl_tie728_*conv2d*`.

Run all seven host tests again, record the image SHA-256, and run esptool image
inspection. Expected: tests and image checks pass.

- [ ] **Step 3: Flash and verify startup over serial before requesting visual input**

Flash through `/dev/ttyACM0` with write verification. Reset once while
capturing ten seconds of serial output.

Pass criteria before user testing: no `xtensa_user_panic`, LCD initialization
progress reaches the AgentGuard application, and the USB serial device remains
available. If startup fails, immediately rebuild and flash the recovery mixed
backend.

- [ ] **Step 4: Validate reference and live inference on the physical device**

Ask for `AI`, `T`, `F`, `S`, `FACE`, and `M` in an empty scene and with one
centered face, plus LCD state.

Pass criteria: LCD displays; reference `F` is at least 1; empty-scene `FACE`
is 0; centered-face `FACE` is at least 1; live diagnostics change between
scenes. Fixed live output fails validation.
