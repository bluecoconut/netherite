#include "game/runtime.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s\n", message); \
        return 1; \
    } \
} while (0)

enum {
    CASE_SWEEP,
    CASE_MOVING_SWORD,
    CASE_NODAMAGE,
    CASE_SPRINT,
    CASE_SPRINT_NODAMAGE
};

static int run_case(int kind) {
    GmConfig cfg;
    GmRuntime runtime;
    GmAction idle, attack;
    GmRuntimeSoundEvent first, second;
    char err[256];

    gm_config_defaults(&cfg);
    cfg.world = GM_WORLD_SUPERFLAT;
    cfg.view_distance = 1;
    cfg.mobs = 0;
    cfg.weather = 0;
    memset(&idle, 0, sizeof idle);
    idle.hotbar_sel = -1;
    attack = idle;
    attack.attack = 1;
    attack.do_break = 1;
    CHECK(gm_runtime_init(&runtime, &cfg, err, sizeof err), err);

    double base = (double)gm_world_surface_y(runtime.world, 8, 8);
    gm_runtime_set_pose_state(
        &runtime, 8.5, base, 8.5, 180.0F, 0.0F,
        0.0, -0.0784000015258789, 0.0, 1, 0.0F);
    if (kind == CASE_SWEEP || kind == CASE_MOVING_SWORD)
        isr_set_stack(&runtime.player.inv, 0, ic_mk(276, 1, 0));

    int primary = gm_mobs_spawn_exact(
        &runtime.mobs, GM_MOB_PIG, 7001,
        8.5, base + PSV_EYE_HEIGHT - 0.45, 6.5,
        0.0, 0.0, 0.0, 0.0F, 10.0F, 1, 0, 0, 0);
    int neighbor = -1;
    if (kind == CASE_SWEEP || kind == CASE_MOVING_SWORD) {
        neighbor = gm_mobs_spawn_exact(
            &runtime.mobs, GM_MOB_PIG, 7002,
            9.5, base + PSV_EYE_HEIGHT - 0.45, 6.5,
            0.0, 0.0, 0.0, 0.0F, 10.0F, 1, 0, 0, 0);
        runtime.mobs.a.on_ground[neighbor] =
            runtime.mobs.b.on_ground[neighbor] = 1;
    }
    CHECK(primary > 0 && (neighbor > 0 || kind >= CASE_NODAMAGE),
          "attack targets initialize");
    if (kind == CASE_NODAMAGE || kind == CASE_SPRINT_NODAMAGE) {
        runtime.mobs.entity_hurt_resistant[primary] = 20;
        runtime.mobs.entity_last_damage[primary] = 2.0F;
    }

    gm_runtime_tick(&runtime, attack);
    runtime.mobs.player_ticks_since_last_swing =
        kind == CASE_SWEEP || kind == CASE_MOVING_SWORD ? 12 : 5;
    runtime.player.ent.onGround = 1;
    runtime.player.fall_distance = 0.0F;
    runtime.player.movement_speed_multiplier = 1.0;
    if (kind == CASE_MOVING_SWORD) {
        runtime.server_prev_distance_walked_modified = 0.0F;
        runtime.server_distance_walked_modified = 0.2F;
    }
    if (kind == CASE_SPRINT || kind == CASE_SPRINT_NODAMAGE)
        runtime.player.sprinting = 1;
    gm_runtime_tick(&runtime, idle);

    int count = gm_runtime_sound_event_count(&runtime);
    CHECK(count == (kind >= CASE_SPRINT ? 2 : 1),
          "attack emits the expected number of player sound events");
    CHECK(gm_runtime_sound_event_get(&runtime, 0, &first),
          "first attack sound is readable");
    CHECK(first.category == GM_SOUND_CATEGORY_PLAYERS
              && first.volume == 1.0F && first.pitch == 1.0F
              && first.x == 8.5 && first.y == base && first.z == 8.5,
          "attack sound category, scalar, and source are exact");
    if (kind == CASE_SWEEP) {
        const EwStore *store = runtime.mobs.current
            ? &runtime.mobs.b : &runtime.mobs.a;
        CHECK(first.sound == GM_SOUND_PLAYER_ATTACK_SWEEP
                  && store->health[neighbor] == 9.0F,
              "stationary sword emits sweep and damages the neighbor");
    } else if (kind == CASE_MOVING_SWORD) {
        const EwStore *store = runtime.mobs.current
            ? &runtime.mobs.b : &runtime.mobs.a;
        CHECK(first.sound == GM_SOUND_PLAYER_ATTACK_STRONG
                  && store->health[neighbor] == 10.0F,
              "movement suppresses sweep and selects strong attack");
    } else if (kind == CASE_NODAMAGE) {
        CHECK(first.sound == GM_SOUND_PLAYER_ATTACK_NODAMAGE,
              "rejected damage emits nodamage");
    } else if (kind == CASE_SPRINT) {
        CHECK(first.sound == GM_SOUND_PLAYER_ATTACK_KNOCKBACK
                  && gm_runtime_sound_event_get(&runtime, 1, &second)
                  && second.sound == GM_SOUND_PLAYER_ATTACK_STRONG
                  && second.category == GM_SOUND_CATEGORY_PLAYERS
                  && second.volume == 1.0F && second.pitch == 1.0F,
              "sprint attack emits knockback before strong");
    } else {
        CHECK(first.sound == GM_SOUND_PLAYER_ATTACK_KNOCKBACK
                  && gm_runtime_sound_event_get(&runtime, 1, &second)
                  && second.sound == GM_SOUND_PLAYER_ATTACK_NODAMAGE
                  && second.category == GM_SOUND_CATEGORY_PLAYERS
                  && second.volume == 1.0F && second.pitch == 1.0F,
              "rejected sprint attack emits knockback before nodamage");
    }

    gm_runtime_destroy(&runtime);
    return 0;
}

int main(void) {
    CHECK(run_case(CASE_SWEEP) == 0, "sweep runtime case");
    CHECK(run_case(CASE_MOVING_SWORD) == 0, "moving sword runtime case");
    CHECK(run_case(CASE_NODAMAGE) == 0, "nodamage runtime case");
    CHECK(run_case(CASE_SPRINT) == 0, "sprint runtime case");
    CHECK(run_case(CASE_SPRINT_NODAMAGE) == 0,
          "rejected sprint runtime case");
    puts("player attack audio runtime: PASS (sweep/strong/nodamage/order)");
    return 0;
}
