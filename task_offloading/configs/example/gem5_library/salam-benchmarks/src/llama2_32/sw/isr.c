#include <stdio.h>
#include <stdint.h>

extern volatile int stage;
extern volatile uint8_t *top_reg;

void isr(void)
{
	printf("Interrupt\n");
	stage += 1;
	*top_reg = 0x00;
	printf("Interrupt finished\n");
}
