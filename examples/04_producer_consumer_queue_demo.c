/**
 * @file 04_producer_consumer_queue_demo.c
 * @brief Inter-Task Message Queue Pipeline (Producer-Consumer Pattern)
 */

#include "rtos_kernel.h"
#include "rtos_task.h"
#include "rtos_queue.h"
#include "rtos_trace.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint32_t sensor_id;
    float    reading;
    uint32_t timestamp_ms;
} sensor_packet_t;

static rtos_queue_handle_t g_sensor_queue = NULL;

/* Producer Task: Reads sensors and pushes data packets */
static void sensor_producer_task(void *arg) {
    uint32_t sensor_id = (uint32_t)(uintptr_t)arg;
    
    for (int i = 1; i <= 5; i++) {
        sensor_packet_t packet = {
            .sensor_id = sensor_id,
            .reading = 25.0f + (float)(sensor_id * 10 + i) * 0.5f,
            .timestamp_ms = rtos_ticks_to_ms(rtos_kernel_get_ticks())
        };
        
        printf("[Producer-%u] Enqueueing packet %d/5: (Reading=%.2f C, T=%u ms)\n",
               sensor_id, i, packet.reading, packet.timestamp_ms);
        
        rtos_status_t status = rtos_queue_send(g_sensor_queue, &packet, rtos_ms_to_ticks(50));
        if (status != RTOS_STATUS_OK) {
            printf("[Producer-%u] Queue send timed out or error: %d\n", sensor_id, status);
        }
        
        rtos_task_delay_ms(15);
    }
    
    printf("[Producer-%u] Production finished.\n", sensor_id);
}

/* Consumer Task: Dequeues and processes sensor packets */
static void data_consumer_task(void *arg) {
    (void)arg;
    sensor_packet_t recv_pkt;
    int packets_processed = 0;
    
    while (packets_processed < 10) {
        rtos_status_t status = rtos_queue_receive(g_sensor_queue, &recv_pkt, rtos_ms_to_ticks(100));
        if (status == RTOS_STATUS_OK) {
            packets_processed++;
            printf("  ==> [Consumer] Processed Packet #%d from Sensor %u: Temp=%.2f C (Latency=%u ms)\n",
                   packets_processed, recv_pkt.sensor_id, recv_pkt.reading,
                   rtos_ticks_to_ms(rtos_kernel_get_ticks()) - recv_pkt.timestamp_ms);
        } else {
            printf("  ==> [Consumer] Queue receive timeout.\n");
        }
    }
    
    printf("\n--- Inter-Task Queue Runtime Diagnostics ---\n");
    rtos_trace_print_task_stats();
    
    printf("[Demo 4] Producer-Consumer Message Queue Demo Complete!\n");
    exit(0);
}

int main(void) {
    printf("============================================================\n");
    printf("   RTOS Demonstration 04: Inter-Task Message Queue Pipeline \n");
    printf("============================================================\n");
    
    rtos_kernel_init();
    
    /* Create a message queue holding up to 4 items */
    g_sensor_queue = rtos_queue_create(4, sizeof(sensor_packet_t), "Telemetry_FIFO");
    
    rtos_task_config_t prod1_cfg = { .name = "IMU_Producer_1",   .entry = sensor_producer_task, .arg = (void*)(uintptr_t)1, .priority = 4, .stack_size = 0 };
    rtos_task_config_t prod2_cfg = { .name = "Temp_Producer_2",  .entry = sensor_producer_task, .arg = (void*)(uintptr_t)2, .priority = 4, .stack_size = 0 };
    rtos_task_config_t cons_cfg  = { .name = "Filter_Consumer",  .entry = data_consumer_task,   .arg = NULL,                 .priority = 6, .stack_size = 0 };
    
    rtos_task_create(&prod1_cfg, NULL);
    rtos_task_create(&prod2_cfg, NULL);
    rtos_task_create(&cons_cfg, NULL);
    
    rtos_kernel_start();
    return 0;
}
