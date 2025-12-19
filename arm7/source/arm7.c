#include <nds.h>

int main()
{
	irqInit();
	fifoInit();
	initClockIRQTimer(3);

	struct {
		u64 consoleId;
		u32 cid[4];
	} consoleIdAndCid;
	consoleIdAndCid.consoleId = getConsoleID();

	SDMMC_init(SDMMC_DEV_eMMC);
	SDMMC_getCidRaw(SDMMC_DEV_eMMC, consoleIdAndCid.cid);
	fifoSendDatamsg(FIFO_USER_02, sizeof(consoleIdAndCid), (u8*)&consoleIdAndCid);

	installSystemFIFO();

	irqSet(IRQ_VBLANK, inputGetAndSend);

	irqEnable(IRQ_VBLANK);
	while (1) {
		swiWaitForVBlank();
	}

	return 0;
}
