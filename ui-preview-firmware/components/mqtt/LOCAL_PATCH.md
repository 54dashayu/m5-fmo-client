# Project-local ESP-MQTT receive timeout fix

Copied from installed ESP-IDF v5.5.1 MQTT component; esp-mqtt revision
6af4446a48ea7fa54948edc4e62277ed70abb6e1. Original license retained.

`mqtt_process_receive` reads nonblocking. Upstream aborts an incomplete MQTT
message after one iteration without byte progress. With a 20ms poll and a
1096-byte RADPCM payload split at TCP MSS1024, ordinary TCP segment delay can
therefore close a healthy connection. This is independent of codec validation.

A per-client receive_progress_tick now tracks actual incoming byte progress.
Incomplete messages survive empty polls until network_timeout_ms expires
(currently 10000ms). EOF/transport errors still fail immediately; empty idle
connections do not become partial-message timeouts. No global SDK changes.

`tests/test_mqtt_partial_receive.py` extracts the production receive-decision
code and tests delayed payload/header fragments, progress refresh, expiry,
normal idle and EOF with controlled transport/time. The same harness fails on
the installed unmodified SDK at the 20ms inter-fragment gap, and passes here.
Project compile_commands confirms this component is selected.

## Bounded audio enqueue (0.6.2)

Added `esp_mqtt_client_enqueue_timeout` (0–50 ms recursive-lock wait, -3 busy) and `esp_mqtt_client_outbox_size_now` (nonblocking, -1 busy). Existing enqueue retains its blocking API. Outbox capacity check now occurs under the same lock as enqueue. Audio uses a 20 ms bound and a 2300-byte outbox limit; status polling never waits for the MQTT transport lock. Busy audio packets are dropped, not replayed late. Host regression: `tests/test_mqtt_bounded_enqueue.py`. This bounds lock contention, not network delivery latency.

## Current 0.7.3 behavior

Audio now uses a zero-wait enqueue and an800-byte MQTT outbox. A separate audio-task-owned four-packet FIFO retries locally, expires pending batches after640ms, and clears pending data across disconnect/new-burst/peer-RX boundaries. Two-frame batches are752bytes. The earlier0.6.2 paragraph describes historical settings only. TCP_NODELAY is enabled after each successful plain MQTT transport connect, inside the owning MQTT task. No global ESP-IDF modifications are needed.
