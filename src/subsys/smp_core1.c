#include "subsys.h"
#include "../kernel/kernel.h"
#include "../drivers/drivers.h"

#define REG32(a) (*(volatile uint32_t *)(a))
#define SIO_FIFO_ST     REG32(0xd0000050u)
#define SIO_FIFO_WR     REG32(0xd0000054u)
#define SIO_FIFO_RD     REG32(0xd0000058u)

static uint32_t core1_stack[256];
static volatile int core1_mode = 0;
static volatile uint32_t core1_loops = 0;

static void core1_entry(void) {
    while (1) {
        core1_loops++;
        if (core1_mode == 1) {
            drv_gpio_toggle(15);
            k_msleep(250);
        } else if (core1_mode == 2) {
            for (volatile int i = 0; i < 1000; i++);
        } else {
            __asm__ volatile ("nop");
        }
    }
}

static void fifo_send(uint32_t v) { while (!(SIO_FIFO_ST & 2u)); SIO_FIFO_WR = v; __asm__ volatile ("sev"); }
static uint32_t fifo_recv(void) { while (!(SIO_FIFO_ST & 1u)) __asm__ volatile ("wfe"); return SIO_FIFO_RD; }

void smp_init(void) {
    uint32_t cmd[] = { 0u, 0u, 1u, (uint32_t)REG32(0xe000ed08u), (uint32_t)&core1_stack, (uint32_t)core1_entry };
    int seq = 0;
    while (seq < 6) {
        uint32_t c = cmd[seq];
        if (!c) { while (SIO_FIFO_ST & 1u) (void)SIO_FIFO_RD; __asm__ volatile ("sev"); }
        fifo_send(c);
        seq = (fifo_recv() == c) ? (seq + 1) : 0;
    }
    k_log("SMP", "Core 1 (2. Cekirdek) uyandirildi ve hazir");
}

void smp_set_mode(int mode) { core1_mode = mode; }
int  smp_get_mode(void) { return core1_mode; }
uint32_t smp_get_cycles(void) { return core1_loops; }
