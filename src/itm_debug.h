#include <libopencm3/cm3/itm.h>

#include <libopencm3/stm32/rcc.h>

uint8_t itm_send_char(uint8_t c) {
	while (!(ITM_STIM8(0) & ITM_STIM_FIFOREADY)) {
	}
	ITM_STIM8(0) = c;
	return c;
}

int _write(int fd, char *ptr, int len) {
	(void)fd;
	int i = 0;
#if ENABLE_ITM == 1
	for (; i < len; i++) {
		// if (ptr[i] == '\n') {
		// 	itm_send_char('\r');
		// }
		itm_send_char(ptr[i]);
	}
#endif
	return i;
}

extern int printf(const char *restrict format, ...);
