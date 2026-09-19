# AgentGuard AI Error Safe-State Design

**Date:** 2026-09-14

## Goal

Make ESP-DL inference failures visible and safe without disturbing the stable
face detector. A sustained inference failure must never be interpreted as
`FACE:0`, and time spent without trustworthy vision data must not advance
face-dependent reminder or privacy timers.

## Scope

This change covers failures returned by `ag_vision_process_rgb565()`. Camera
capture failures remain owned by the existing camera watchdog and `CAM:*`
stale-state UI.

The feature will:

- debounce inference health independently from face-presence smoothing;
- enter AI error after 3 consecutive failed inference attempts;
- recover after 2 consecutive successful inference attempts;
- freeze face-dependent state-machine timers from the first failed attempt
  until the recovery gate is satisfied;
- display `FACE:--` and `AI ERROR` while the error state is active;
- suppress face boxes while the error state is active;
- append one `ai_error` event on entry and one `ai_recovered` event on exit;
- resume normal processing automatically using the current successful result.

It will not add retries inside ESP-DL, replace ESP-DL results with heuristic
results, restart the camera, add KWS, or change model thresholds.

## Architecture

### Inference-health debounce

A new dependency-free `vision_health` module owns only consecutive-result
debouncing. Its state contains failure count, success count, and the active
error flag. Its update function accepts whether the current inference attempt
succeeded and returns one of three transitions: none, entered error, or
recovered.

Before error entry, any success clears the failure count. While error is
active, any failure clears the recovery count. Counters saturate so a long
failure cannot wrap. The module enters on the third failure and recovers on
the second success.

### Policy timer freeze

`struct ag_observation` gains `vision_valid`. Every completed camera frame is
submitted to `ag_step()`:

- failed inference and the first recovery-gate success use
  `vision_valid = false`;
- the success that satisfies the recovery gate uses `vision_valid = true`.

On the first invalid observation, core records `vision_paused_since_ms` and
returns after applying explicit user commands. It does not modify presence,
posture, reminder, blur, or lock state from invalid face data.

On the first valid observation after a pause, core shifts every active
face-dependent timestamp forward by the paused duration before evaluating the
observation. The elapsed invalid interval therefore cannot trigger an alert,
break confirmation, or privacy lock immediately after recovery. Timestamp
addition saturates at `UINT64_MAX`.

The timestamps shifted are `presence_since_ms`, `absence_since_ms`,
`poor_posture_since_ms`, and `awaiting_ack_since_ms`. Zero continues to mean
inactive. `vision_paused_since_ms` is then cleared.

Acknowledgement during an invalid interval uses the last trustworthy
`state.present`, not invalid face data, to record a pending sitting reset.
The reset is applied at the first valid observation after recovery, so the
acknowledgement cannot be negated by an immediate renewed sedentary alert.

### Events and persistent evidence

Two event bits are added to the existing core event vocabulary:

- `AG_EVENT_AI_ERROR` -> `"ai_error"`
- `AG_EVENT_AI_RECOVERED` -> `"ai_recovered"`

The main loop dispatches them only when the health module reports a
transition. Existing JSON formatting, retention, and append logic are reused.
They are stored locally but are not sent to the PC action endpoint and do not
change LED state.

Storage event slots increase from 14 to 16 so stats parsing recognizes both
events.

### Display behavior

`struct ag_ui_status` gains `ai_error`. When set:

- the header uses `FACE:--` rather than a numeric value;
- the primary footer status is `AI ERROR` in red;
- the status accent is red;
- the main loop publishes zero face boxes;
- `camera_stale` remains higher priority, so an absent camera frame continues
  to show `CAM:*` instead of mislabeling the failure as inference-only.

The display worker continues showing the latest camera frame. This preserves
operator visibility while clearly marking the AI output as unavailable.

### Main-loop flow

For each captured frame:

1. Rotate the frame and call `ag_vision_process_rgb565()` once.
2. Feed success/failure into `ag_vision_health_update()`.
3. Dispatch an AI entry/recovery event only on a transition.
4. If inference is not yet trusted, submit an invalid core observation and
   publish an error UI with an empty vision result once error is active.
5. If inference is trusted, apply the existing ESP-DL face-authority filter,
   submit a valid observation, and publish the existing normal UI.
6. Requeue the camera buffer through the existing path.

On error entry, reset the face-presence hold state. This prevents pre-error
boxes and counts from being reused after a prolonged failure.

The adapter's three-frame inference schedule advances only after a successful
fresh inference or a legitimate cache reuse. A failed fresh attempt leaves the
schedule on a fresh-inference slot, so subsequent old cached results cannot
mask repeated failures. Official ESP-DL algorithm/model files are unchanged.

## Failure Handling

- One or two isolated failures pause policy timing but retain the last display
  state, avoiding visible flicker.
- A sustained failure enters a visible safe state on the third failure.
- A single success during an active error is insufficient to expose results.
- Repeated failures produce no duplicate `ai_error` log records.
- A later sustained failure creates a new entry record only after a completed
  recovery transition.

## Verification

Host tests must prove:

- exact 3-failure entry and 2-success recovery thresholds;
- counter reset and transition de-duplication;
- invalid observations cannot create face-dependent events;
- recovery excludes paused time from timer deadlines;
- explicit commands remain usable during a vision pause;
- event names and storage statistics include both AI events;
- UI formatting produces `FACE:--`, `AI ERROR`, and red status styling;
- camera-stale presentation retains priority;
- main-loop source calls the health gate and never applies face authority to a
  failed inference result.

Target verification must include a clean ESP32-S3 build, an image below the
`0x300000` LittleFS boundary, flashing without erasing LittleFS, and a live
smoke test showing normal face detection still works. A controlled inference
failure may be demonstrated by a test hook only if one already exists; this
feature does not add production fault-injection code.
