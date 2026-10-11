#ifndef TCVR_QUEST_COOP_H
#define TCVR_QUEST_COOP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define QCOOP_DEFAULT_TCP_PORT 27009
#define QCOOP_DEFAULT_DISCOVERY_PORT 27010
#define QCOOP_PROTOCOL_VERSION 1
#define QCOOP_SHOT_QUEUE 4
#ifndef QCOOP_STALL_TIMEOUT_MS
#define QCOOP_STALL_TIMEOUT_MS 5000
#endif

typedef struct QCoopInput {
    bool trigger;
    bool exposed;
    bool aim_valid;
    uint16_t gun_x;
    uint16_t gun_y;
    bool offscreen;
    bool coin;
} QCoopInput;

typedef struct QCoopTick {
    bool fire;
    bool exposed;
    bool coin;
    uint16_t gun_x;
    uint16_t gun_y;
    bool offscreen;
} QCoopTick;

typedef struct QCoopShot {
    bool pending;
    bool exposed;
    uint16_t gun_x;
    uint16_t gun_y;
    bool offscreen;
} QCoopShot;

typedef struct QCoopArbiter {
    bool trigger_was_down[2];
    bool neutral_rearm[2];
    bool previous_fire;
    unsigned tie_preference;
    QCoopShot pending[2][QCOOP_SHOT_QUEUE];
    unsigned pending_head[2];
    unsigned pending_count[2];
} QCoopArbiter;

/* Explicit process-local selection. No launch mode is inferred from saved state. */
bool qcoop_start_host(const char *rom_dir, int tcp_port, int discovery_port);
bool qcoop_start_join(const char *rom_dir, const char *ipv4, int tcp_port, int discovery_port);
bool qcoop_host_start(void);
void qcoop_cancel(bool force_solo);
void qcoop_report_error(const char *message);
bool qcoop_launch_arguments_supported(int argc, char **argv);
bool qcoop_frame_diagnostics_pending(void);
char *qcoop_status_json(void); /* caller frees */
char **qcoop_discover(const char *rom_dir, int discovery_port, int timeout_ms, size_t *count); /* caller frees all */
char **qcoop_local_ipv4_addresses(size_t *count); /* caller frees all */

bool qcoop_requested(void);
bool qcoop_is_active(void);
bool qcoop_is_failed(void);
bool qcoop_is_paused(void);
bool qcoop_is_client(void);
void qcoop_copy_display_status(char *out, size_t size);
const char *qcoop_failure_message(void);
void qcoop_set_local_ready(bool ready);
void qcoop_capture_local(QCoopInput input);
/* Called at the existing simulation deadline. Never waits; false keeps rendering without advancing emulation. */
bool qcoop_try_tick(void);
bool qcoop_current_input(QCoopTick *tick);
/* Convert the canonical shared trigger/pedal/coin to the arcade input bits. */
uint16_t qcoop_to_arcade_bits(QCoopTick tick);
/* Called after local EEPROM load and before DSP/video/game entry. Returns 0 for solo, 1 for synchronized co-op, -1 on failure. */
int qcoop_engine_boot(uint8_t *eeprom, size_t eeprom_size, bool diagnostic_replay, bool diagnostics_pending);

/* Pure deterministic arbitration used by the production worker and its host tests. */
void qcoop_arbitrate(QCoopArbiter *arbiter, const QCoopInput players[2], QCoopTick *tick);

#endif
