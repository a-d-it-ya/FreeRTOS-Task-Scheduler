/**
 * @file 05_flight_telemetry_sim.c
 * @brief Aerospace Flight Computer Avionics Multi-Rate Task Simulator
 *
 * Models an aerospace flight computer task pipeline:
 * - IMU Sensor Task (100 Hz / 10ms period, Priority 12)
 * - Flight Control & Guidance Loop (50 Hz / 20ms period, Priority 10)
 * - GPS Navigation Task (10 Hz / 100ms period, Priority 6)
 * - Telemetry Radio Downlink (2 Hz / 500ms period, Priority 3)
 * - Real-Time Flight Diagnostics Console (Priority 1)
 */

#include "rtos_kernel.h"
#include "rtos_task.h"
#include "rtos_sync.h"
#include "rtos_queue.h"
#include "rtos_trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Aerospace Telemetry State Struct */
typedef struct {
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
    float altitude_m;
    float airspeed_mps;
    uint32_t imu_samples_count;
    uint32_t guidance_cycles_count;
    uint32_t gps_fixes_count;
    uint32_t packets_transmitted;
} flight_state_t;

static flight_state_t      g_flight_state = {0};
static rtos_mutex_handle_t g_flight_state_mutex = NULL;
static volatile bool       g_flight_sim_active = true;

/* 1. IMU Fast Acquisition Loop (100 Hz / 10ms) */
static void imu_sampling_task(void *arg) {
    (void)arg;
    rtos_tick_t last_wake = rtos_kernel_get_ticks();
    float t = 0.0f;
    
    while (g_flight_sim_active) {
        rtos_task_delay_until(&last_wake, rtos_ms_to_ticks(10));
        
        t += 0.01f;
        rtos_mutex_lock(g_flight_state_mutex, RTOS_WAIT_FOREVER);
        g_flight_state.roll_deg = 2.5f * sinf(t * 2.0f);
        g_flight_state.pitch_deg = 1.8f * cosf(t * 1.5f) + 3.0f;
        g_flight_state.yaw_deg = fmodf(t * 5.0f, 360.0f);
        g_flight_state.imu_samples_count++;
        rtos_mutex_unlock(g_flight_state_mutex);
    }
}

/* 2. Flight Control & Attitude Calculation Loop (50 Hz / 20ms) */
static void flight_control_task(void *arg) {
    (void)arg;
    rtos_tick_t last_wake = rtos_kernel_get_ticks();
    
    while (g_flight_sim_active) {
        rtos_task_delay_until(&last_wake, rtos_ms_to_ticks(20));
        
        rtos_mutex_lock(g_flight_state_mutex, RTOS_WAIT_FOREVER);
        g_flight_state.altitude_m += (g_flight_state.pitch_deg > 0.0f ? 0.25f : -0.1f);
        g_flight_state.airspeed_mps = 45.0f + (g_flight_state.pitch_deg * 0.5f);
        g_flight_state.guidance_cycles_count++;
        rtos_mutex_unlock(g_flight_state_mutex);
    }
}

/* 3. GPS Navigation Task (10 Hz / 100ms) */
static void gps_nav_task(void *arg) {
    (void)arg;
    rtos_tick_t last_wake = rtos_kernel_get_ticks();
    
    while (g_flight_sim_active) {
        rtos_task_delay_until(&last_wake, rtos_ms_to_ticks(100));
        
        rtos_mutex_lock(g_flight_state_mutex, RTOS_WAIT_FOREVER);
        g_flight_state.gps_fixes_count++;
        rtos_mutex_unlock(g_flight_state_mutex);
    }
}

/* 4. Telemetry Downlink Radio Task (2 Hz / 500ms) */
static void radio_telemetry_task(void *arg) {
    (void)arg;
    rtos_tick_t last_wake = rtos_kernel_get_ticks();
    
    while (g_flight_sim_active) {
        rtos_task_delay_until(&last_wake, rtos_ms_to_ticks(500));
        
        rtos_mutex_lock(g_flight_state_mutex, RTOS_WAIT_FOREVER);
        g_flight_state.packets_transmitted++;
        
        printf("[RADIO TX] Frame #%-3u | Roll: %+5.1f | Pitch: %+5.1f | Yaw: %5.1f | Alt: %6.1f m | Spd: %4.1f m/s\n",
               g_flight_state.packets_transmitted,
               g_flight_state.roll_deg,
               g_flight_state.pitch_deg,
               g_flight_state.yaw_deg,
               g_flight_state.altitude_m,
               g_flight_state.airspeed_mps);
        
        rtos_mutex_unlock(g_flight_state_mutex);
    }
}

/* 5. Master Flight Supervisor & Diagnostic Console (Priority 1) */
static void flight_supervisor_task(void *arg) {
    (void)arg;
    
    printf("\n>>> Avionics Flight Computer Running Multi-Rate Sensor Pipeline <<<\n\n");
    
    /* Run simulation for 2.5 seconds (2500ms) */
    rtos_task_delay_ms(2500);
    g_flight_sim_active = false;
    
    /* Display final mission diagnostics */
    printf("\n============================================================\n");
    printf("   FLIGHT COMPUTER MISSION METRICS & SCHEDULER DIAGNOSTICS  \n");
    printf("============================================================\n");
    printf(" IMU Acquisition Samples (100Hz):   %u\n", g_flight_state.imu_samples_count);
    printf(" Flight Guidance Cycles (50Hz):     %u\n", g_flight_state.guidance_cycles_count);
    printf(" GPS Navigation Fixes (10Hz):       %u\n", g_flight_state.gps_fixes_count);
    printf(" Telemetry Radio Packets (2Hz):     %u\n", g_flight_state.packets_transmitted);
    printf(" Final Altitude:                    %.2f meters\n", g_flight_state.altitude_m);
    
    rtos_trace_print_task_stats();
    
    printf("[Flight Sim] Avionics Multi-Rate Simulation Successful!\n");
    exit(0);
}

int main(void) {
    printf("============================================================\n");
    printf("   AEROSPACE FLIGHT COMPUTER RTOS SCHEDULER SIMULATION      \n");
    printf("============================================================\n");
    
    rtos_kernel_init();
    
    g_flight_state_mutex = rtos_mutex_create("FlightState_Mutex");
    
    rtos_task_config_t imu_cfg   = { .name = "IMU_Acq_100Hz",     .entry = imu_sampling_task,     .arg = NULL, .priority = 12, .stack_size = 0 };
    rtos_task_config_t ctrl_cfg  = { .name = "Control_50Hz",      .entry = flight_control_task,   .arg = NULL, .priority = 10, .stack_size = 0 };
    rtos_task_config_t gps_cfg   = { .name = "GPS_Nav_10Hz",      .entry = gps_nav_task,          .arg = NULL, .priority = 6,  .stack_size = 0 };
    rtos_task_config_t radio_cfg = { .name = "Telemetry_Radio_2Hz",.entry = radio_telemetry_task,  .arg = NULL, .priority = 3,  .stack_size = 0 };
    rtos_task_config_t super_cfg = { .name = "FlightSupervisor",  .entry = flight_supervisor_task,.arg = NULL, .priority = 1,  .stack_size = 0 };
    
    rtos_task_create(&imu_cfg, NULL);
    rtos_task_create(&ctrl_cfg, NULL);
    rtos_task_create(&gps_cfg, NULL);
    rtos_task_create(&radio_cfg, NULL);
    rtos_task_create(&super_cfg, NULL);
    
    rtos_kernel_start();
    return 0;
}
