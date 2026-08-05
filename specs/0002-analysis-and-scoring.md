# Spec 0002 — Analysis, Metrics & Repeatability Scoring

- **Status:** DRAFT (for review — not yet agreed)
- **Owner:** Ryan
- **Related:** `specs/0001-system-architecture.md` (provides the stroke-window input); `CLAUDE.md` §2
- **Supersedes:** none

> Legend: **[PROPOSED — ratify]** = concrete choice put forward for agreement.
> **[OPEN]** = undecided, needs a decision or empirical result before it affects code.
> **[EMPIRICAL]** = to be resolved with real captured data during bring-up, then recorded here.

---

## 1. Context & goal
Turn one **stroke window** (the raw IMU time-series delivered per Spec 0001) into two numbers:
- a **strength score** — a *relative, unit-less* measure of how hard the putt was struck, and
- a **face-rotation measure** — how much/how fast the club face rotated through impact.

...and feed those into a **repeatability trainer** with two modes: hit a **target value within a
tolerance band**, and track **session consistency** (how tightly recent putts cluster).

This spec defines *what* is computed, the *coordinate frame and assumptions*, the *candidate
methods*, and — crucially — the *criteria and validation* by which a method is accepted. It does
**not** convert to physical units (per the ratified decision: strength is a relative score).

## 2. Requirements
Functional:
- R1. Given a stroke window, the analysis shall locate the **impact instant** within it.
- R2. The analysis shall compute a **strength score**: a scalar that is monotonic with how hard the
  ball was struck and **repeatable** for repeated near-identical strokes (see §5 acceptance).
- R3. The analysis shall compute a **face-rotation measure** at/through impact (direction + amount
  and/or rate), in physically meaningful terms (degrees and/or degrees/second).
- R4. The trainer shall support **target + tolerance** mode: given a target strength (set explicitly
  or captured from a reference putt) and a tolerance band, classify each putt hit/miss.
- R5. The trainer shall support **session-consistency** mode: over the last N putts, report a spread
  measure (e.g. standard deviation / range) of the strength score.
- R6. All metrics shall be computed by **pure functions over a stroke window** (no BLE/CLI/hardware
  dependency), so they run identically on live and on replayed/recorded windows (Spec 0001 R6/R7).

Non-functional:
- R7. Every physics/derivation assumption shall be stated in this spec, with its known limits.
- R8. Each accepted metric shall have a **documented empirical validation** showing it meets R2/R3
  (monotonic + repeatable), using recorded strokes — not assumed.

## 3. Assumptions & constraints (physics)
- A1. Sensor is rigidly fixed in the mallet rear cavity (Spec 0001 / CLAUDE.md §3): head and face
  move as one rigid body, so gyroscope = face angular motion, offset behind face center.
- A2. **Best physical predictor of ball roll is head speed at impact**, not the impact spike's peak
  magnitude (which depends on collision stiffness and is noisier). For a *relative* score we still
  prefer a metric that tracks head speed at impact if it proves more repeatable.
- A3. **Gravity contaminates the accelerometer**: the sensor measures (linear acceleration + gravity)
  in the *sensor* frame, and the sensor rotates through the stroke (pendulum arc). Removing gravity
  requires tracking orientation (accelerometer at rest gives the down vector; gyro tracks change).
- A4. **Integration drift**: single/double integration of accelerometer data drifts. Mitigated here
  by operating only over a short window (~300 ms pre-impact per Spec 0001) and by removing sensor
  biases measured while stationary at address.
- A5. **Gyro bias** must be removed; estimate it from a stationary "at address" period before the
  stroke. **[OPEN]** how the stationary period is detected/triggered.
- A6. Face opening/closing is rotation about the **vertical (yaw) axis**. The vertical axis in the
  sensor frame is obtained from the gravity direction measured while stationary at address (A3).

## 4. Design

### 4.1 Coordinate frame & calibration
- **[PROPOSED — ratify]** Establish a per-session (or per-stroke) reference from a brief **stationary
  "address" period**: the mean accelerometer vector defines **down** (hence the vertical/yaw axis,
  A6), and the mean gyro vector defines **gyro bias** (A5).
- **[OPEN]** Defining the **target line** (direction of intended ball travel) in the sensor frame.
  Face *rotation* (yaw about vertical) does **not** require it. A head-speed strength metric that
  needs a travel direction would; a magnitude-based metric would not. Resolve alongside §4.3.
- **[OPEN]** Whether a one-time mounting-orientation calibration is needed, or the address-period
  reference is sufficient each session.

### 4.2 Impact detection within the window
- **[PROPOSED — ratify]** Locate impact as the sample of maximum high-frequency acceleration
  response (the sharp transient), refined to the onset of the spike. Report its timestamp; split the
  window into pre-impact (stroke) and post-impact (follow-through/ringing) segments.
- **[EMPIRICAL]** Exact detector (threshold on jerk vs. peak resultant vs. band-pass energy) chosen
  from recorded impacts.

### 4.3 Strength metric — candidates, selection by validation
The metric is **relative/unit-less**, but must be monotonic + repeatable (R2). Candidate scalars,
to be evaluated against recorded strokes of graded, known-different effort:
- C1. **Pre-impact head-speed estimate** — remove gravity using the §4.1 orientation, integrate
  linear acceleration over the downswing to estimate head speed just before impact. Best physical
  link to ball roll (A2); most sensitive to drift/orientation error. May need the target line (§4.1).
- C2. **Impact impulse** — integral of the acceleration transient over the impact (∝ head momentum
  change). No pre-impact integration needed; depends on collision dynamics.
- C3. **Peak resultant acceleration** at impact — simplest; likely noisiest/least repeatable.
- **[EMPIRICAL] Selection rule:** capture a graded set of strokes; the accepted metric is the
  candidate with the best **monotonicity** vs. effort and best **repeatability** (lowest spread on
  repeated identical strokes). Record the winner and its numbers here. Multiple may be combined.
- **[PROPOSED — ratify]** The chosen raw scalar is mapped to a **strength score** by normalization
  (§4.5) so it reads on a stable, human-friendly scale.

### 4.4 Face-rotation metric
- **[PROPOSED — ratify]** Using the yaw (vertical) axis from §4.1 and bias-corrected gyro:
  - **Face angular velocity at impact** (°/s, signed: + = closing, − = opening, or per a fixed
    convention) — instantaneous rotation rate through contact.
  - **Net face rotation** (°) over the pre-impact segment — integrate yaw rate from address to
    impact, indicating whether the face was opening or closing into the ball.
- **[EMPIRICAL]** Confirm integration drift over the window is small enough for a repeatable ° value.
- **[OPEN]** Sign convention (which way is "open" vs "closed") — fix after mounting/calibration.

### 4.5 From raw metric to a relative score
- **[PROPOSED — ratify]** A strength **score** is the raw metric normalized to a stable scale (e.g.
  referenced to a captured baseline stroke, or scaled within a session). The score's job is
  repeatability and comparability, not physical meaning.
- **[OPEN]** Normalization reference: fixed baseline stroke, per-session auto-scale, or a saved
  calibration profile. Affects how "the same score tomorrow" is interpreted.

### 4.6 Trainer / feedback modes (both required, R4 + R5)
- **Target + tolerance (R4):** user sets a target strength score (typed, or captured from a
  reference putt) and a tolerance band; each putt is classified **in-band / high / low**, with the
  signed deviation. **[OPEN]** default tolerance width, and whether it is in score units or a %.
- **Session consistency (R5):** over the last **N [PROPOSED — ratify: N=10]** putts, report a spread
  measure (**[PROPOSED — ratify]** standard deviation, plus min/max range) of the strength score,
  and optionally of face rotation. Lower spread = more repeatable.
- Both modes also surface the per-putt **face-rotation** measure so the user can see if inconsistent
  strength correlates with face rotation.

### 4.7 Analysis API shape (honors R6)
Pure functions, roughly:
```
detect_impact(window) -> ImpactInfo
strength_metric(window, impact, calib) -> raw_scalar
face_rotation(window, impact, calib) -> RotationResult   # angular vel + net angle
score(raw_scalar, normalization_ref) -> StrengthScore
# trainer (stateful over a session, but fed only by the above pure outputs)
classify_target(score, target, tolerance) -> {in_band|high|low, deviation}
session_consistency(scores[-N:]) -> {stddev, range, n}
```
No BLE, CLI, or hardware types appear here — only the stroke-window data structure from Spec 0001
and plain results. This is what makes replay-based testing (Spec 0001 R7) possible.

## 5. Acceptance criteria
- AC1. Impact instant is correctly located on a set of recorded strokes (visually verifiable spike).
- AC2. **Strength repeatability:** on a set of repeated, deliberately-similar strokes, the strength
  score's spread is within a documented bound; on graded strokes it is **monotonic** with effort.
  (This is the core proof the metric works — R2/R8.)
- AC3. **Rotation validity:** for strokes deliberately hit with an open vs. closed vs. square face,
  the rotation measure separates them in the correct direction.
- AC4. Target+tolerance mode correctly classifies putts relative to a set target and band.
- AC5. Session-consistency mode reports a spread measure over the last N putts that decreases as
  strokes become more repeatable.
- AC6. All metrics run unchanged on recorded windows with no device present (R6).

## 6. Open questions
- Q1 (§4.1/§4.3): Is a target-line direction needed? — depends on whether the chosen strength metric
  is head-speed-based (C1) or magnitude/impulse-based (C2/C3).
- Q2 (§4.3): Which strength candidate wins — decided by the §4.3 [EMPIRICAL] selection on real data.
- Q3 (§4.4/§4.6): Face-rotation sign convention and whether both angular-velocity and net-angle are
  surfaced in v1 or just one.
- Q4 (§4.5): Score normalization reference (baseline stroke vs. per-session vs. saved profile).
- Q5 (§4.6): Default tolerance width and units; default N for the consistency window (proposed 10).
- Q6 (§3/§4.1): How the stationary "address" period is detected to grab bias/gravity reference.

## 7. Out of scope (this spec)
- Physical units / real-world distance conversion (explicitly excluded by the relative-score decision).
- Transport, packet format, capture windowing (Spec 0001).
- Any GUI; long-term history/trends beyond a single session's consistency window.
