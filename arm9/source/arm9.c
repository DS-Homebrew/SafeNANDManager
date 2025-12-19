#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include "f_xy.h"
#include "dsi.h"

#define CHUNKSIZE 0x80000
u8 *workbuffer;
u32 done=0;

static struct {
	u8 consoleID[8];
	u8 CID[16];
} consoleIdAndCid;

typedef struct nocash_footer {
	char footerID[16];
	u8 nand_cid[16];
	u8 consoleid[8];
	u8 pad[0x18];
} nocash_footer_t;

void wait(int ticks){
	while(ticks--)swiWaitForVBlank();
}

u32 getMBremaining(){
	struct statvfs stats;
	statvfs("/", &stats);
	u64 total_remaining = ((u64)stats.f_bsize * (u64)stats.f_bavail) / (u64)0x100000;
	return (u32)total_remaining;
}

void death(char *message){
	printf("%s\n", message);
	printf("Hold Power to exit\n");
	free(workbuffer);
	while(1)swiWaitForVBlank();
}

int dumpNAND(nocash_footer_t *footer){
	consoleClear();

	u32 rwTotal=nand_GetSize()*0x200; //240MB or 245.5MB
	printf("NAND size: %.2fMB\n", (float)rwTotal/(float)0x100000);
	
	if(rwTotal != 0xF000000 && rwTotal != 0xF580000) death("Unknown NAND size."); //there's no documented nand chip with sizes different from these two.
	
	bool batteryMsgShown=false;
	while(getBatteryLevel() < 0x4){
		if(!batteryMsgShown) {
			printf("Battery low: plug in to proceed\r"); //user can charge to 2 bars or more OR just plug in the charger. charging state adds +0x80 to battery level. low 4 bits are the battery charge level.
			batteryMsgShown=true;
		}
	}
	
	char *filename="nand.bin";
	int fail=0;
	swiSHA1context_t ctx;
	ctx.sha_block=0; //this is weird but it has to be done
	u8 sha1[20]={0};
	char sha1file[0x33]={0};
	
	FILE *f = fopen("nand.bin", "wb");
	if(!f) death("Could not open nand file");
	
	printf("Dumping...\n");
	printf("Hold A & B to cancel\n");
	printf("\x1b[16;0H");
	printf("Progress: 0%%\n");
	swiSHA1Init(&ctx);

	for(int i=0;i<rwTotal;i+=CHUNKSIZE){           //read from nand, dump to sd
		if(nand_ReadSectors(i / 0x200, CHUNKSIZE / 0x200, workbuffer) == false){
			printf("Nand read error\nOffset: %08X\nAborting...", (int)i);
			fclose(f);
			unlink(filename);
			fail=1;
			break;
		}
		swiSHA1Update(&ctx, workbuffer, CHUNKSIZE);
		if(fwrite(workbuffer, 1, CHUNKSIZE, f) < CHUNKSIZE){
			printf("Sdmc write error\nOffset: %08X\nAborting...", (int)i);
			fclose(f);
			unlink(filename);
			fail=1;
			break;
		}
		printf("\x1b[16;0H");
		printf("Progress: %lu%%\n", (long unsigned)((i+CHUNKSIZE)/(rwTotal/100)));
		scanKeys();
		int keys = keysHeld();
		if(keys & KEY_A && keys & KEY_B){
			printf("\nCanceling...");
			fclose(f);
			unlink(filename);
			fail=1;
			break;
		}
	}
	
	if(!fail) 
	{
		fwrite(footer, 1, sizeof(nocash_footer_t), f);
		fclose(f);
		swiSHA1Final(sha1, &ctx);
		char temp[3];
		for(int i=0;i<20;i++){
			snprintf(temp, 3, "%02X", sha1[i]);
			memcpy(&sha1file[i*2], temp, 2);
		}
		memcpy(&sha1file[20*2], " *nand.bin\n", 11);
		FILE *g=fopen("nand.bin.sha1","wb");
		fwrite(sha1file, 1, 0x33, g);
		fclose(g);
	}

	printf("\nDone.\nPress START to exit");
	done=1;
	
	return fail;
}

int restoreNAND(nocash_footer_t *footer){
	consoleClear();

	printf("\x1B[41mWARNING!\x1B[47m Even with the safety\n");
	printf("measures taken here, writing to\n");
	printf("NAND is very dangerous and most\n");
	printf("issues are not helped by\n");
	printf("restoring a NAND backup.\n\n");
	printf("Only continue if you're certain\n");
	printf("this will fix your problem.\n\n");
	printf("Press X + Y to begin restore\n");
	printf("Press B to cancel\n");

	u16 held = 0;
	while(1) {
		do {
			swiWaitForVBlank();
			scanKeys();
			held = keysHeld();
		} while(!(held & (KEY_X | KEY_Y | KEY_B)));

		if((held & (KEY_X | KEY_Y)) == (KEY_X | KEY_Y)) {
			consoleClear();
			break;
		} else if(held & KEY_B) {
			consoleClear();
			printf("Press Y to begin NAND restore\n");
			printf("Press A to begin NAND dump\nPress START to exit\n\n");
			return -1;
		}
	}

	u32 rwTotal=nand_GetSize()*0x200; //240MB or 245.5MB
	printf("NAND size: %.2fMB\n", (float)rwTotal/(float)0x100000);

	if(rwTotal != 0xF000000 && rwTotal != 0xF580000) death("Unknown NAND size."); //there's no documented nand chip with sizes different from these two.

	bool batteryMsgShown=false;
	while(getBatteryLevel() < 0x4){
		if(!batteryMsgShown) {
			printf("Battery low: plug in to proceed\r"); //user can charge to 2 bars or more OR just plug in the charger. charging state adds +0x80 to battery level. low 4 bits are the battery charge level.
			batteryMsgShown=true;
		}
	}

	int fail=0;
	int sectorsWritten=0;

	FILE *f = fopen("nand.bin", "rb");
	if(!f) death("Could not open nand file");

	fseek(f, (rwTotal==0xF580000 ? 0xF580000 : 0xF000000)+0x10, SEEK_SET);

	u8 CID[16];
	u8 consoleID[8];
	fread(&CID, 1, 16, f);
	fread(&consoleID, 1, 8, f);

	if((memcmp(footer->nand_cid, &CID, 16) != 0) || (memcmp(footer->consoleid, &consoleID, 8) != 0)) death("Footer does not match");

	fseek(f, 0, SEEK_SET);

	printf("Restoring...\n");
	printf("Do not turn off the power\n");
	printf("or remove the SD card.\n");
	printf("\x1b[16;0H");
	printf("Progress: 0%%\nSectors written: 0\n");

	int i2=0;
	for(int i=0;i<rwTotal;i+=0x200){           //read nand dump from sd, compare sectors, and write to nand
		if(nand_ReadSectors(i2, 1, workbuffer) == false){
			printf("Nand read error\nOffset: %08X\nAborting...", (int)i);
			fclose(f);
			fail=1;
			break;
		}
		if(fread(workbuffer+0x200, 1, 0x200, f) < 0x200){
			printf("Sdmc read error\nOffset: %08X\nAborting...", (int)i);
			fclose(f);
			fail=1;
			break;
		}
		if(memcmp(workbuffer, workbuffer+0x200, 0x200) != 0){
			if(nand_WriteSectors(i2, 1, workbuffer+0x200) == false){
				printf("Nand write error\nOffset: %08X\nAborting...", (int)i);
				fclose(f);
				fail=1;
				break;
			}
			sectorsWritten++;
		}
		printf("\x1b[16;0H");
		printf("Progress: %lu%%\nSectors written: %i\n", (long unsigned)((i+0x200)/(rwTotal/100)), sectorsWritten);
		i2++;
	}

	if(!fail) 
	{
		fclose(f);
	}

	printf("\nDone.\nPress START to exit");
	done=1;

	return fail;
}

int verifyNocashFooter(nocash_footer_t *footer){
	u8 out[0x200]={0};
	u8 sha1[20]={0};
	u32 key_x[4]={0};
	u32 key[4]={0};
	u8 iv[16]={0};
	u32 cpuid[2]={0};
	
	nand_ReadSectors(0, 1, workbuffer); 
	dsi_context ctx;
	
	swiSHA1Calc(sha1, &footer->nand_cid, 16);
	memcpy(iv, sha1, 16);
	memcpy(cpuid, &footer->consoleid[0], 8);
	
	key_x[0]=cpuid[0];
	key_x[1]=cpuid[0] ^ 0x24EE6906;
	key_x[2]=cpuid[1] ^ 0xE65B601D;
	key_x[3]=cpuid[1];
	
	F_XY((uint8_t*)key, (uint8_t*)key_x, DSi_NAND_KEY_Y);

	dsi_set_key(&ctx, (u8*)key);
	dsi_set_ctr(&ctx, iv);
	dsi_crypt_ctr(&ctx, workbuffer, out, 0x200);
	
	if(out[510]==0x55 && out[511]==0xAA) return 1;

	return 0;
}

int main(void) {
	extern void dsiOnly(void);
	dsiOnly();

	consoleDemoInit();
	printf("SafeNANDManager v1.1.1 by\n");
	printf("Rocket Robz (dumpTool by zoogie)\n");

	workbuffer=(u8*)malloc(CHUNKSIZE);
	if(!workbuffer) death("Could not allocate workbuffer");  
	nocash_footer_t nocash_footer;
	fifoWaitDatamsg(FIFO_USER_02);
	fifoGetDatamsg(FIFO_USER_02, sizeof(consoleIdAndCid), (u8*)&consoleIdAndCid);
	char dirname[128]={0};

	memset(nocash_footer.footerID, 0, sizeof(nocash_footer_t));
	memcpy(nocash_footer.footerID, "DSi eMMC CID/CPU", 16);
	memcpy(nocash_footer.nand_cid, consoleIdAndCid.CID, 16);
	memcpy(nocash_footer.consoleid, consoleIdAndCid.consoleID, 8);

	if(!fatInitDefault() || !nand_Startup()) death("MMC init problem - dying...");
	wait(1); //was having a race condition issue with nand_startup and nand_readsectors, so this might help

	if(getMBremaining() < 250) death("SD space remaining < 250MBs");
	//printf("sectors: %d\n", nand_GetSize());
	if(nand_GetSize()*0x200 > 600*0x100000) death("This isn't a DSi!"); //I'll give you a kidney if there's unmodified DSi out there with a 600MB NAND.

	snprintf(dirname, 32, "DT%016llX", *(u64*)consoleIdAndCid.CID); //that 'certain other tool' uses MAC for console-unique ID, while this one uses part of the nand CID. either is fine but I don't want to overwrite the other app's dump.
	mkdir(dirname, 0777);
	chdir(dirname);

	bool nandFound = (access("nand.bin", F_OK) == 0);

	printf("Verifying nocash_footer: ");
	bool isNocashFooterGood = verifyNocashFooter(&nocash_footer);
	printf("%s\n", isNocashFooterGood ? "\033[32mGOOD\033[39m":"\033[31mBAD\033[39m\nThis dump can't be decrypted\nwith this footer!\n\033[33mThis can occur if this tool is\nran through HiyaCFW (SDNAND),\nwhich isn't supported.\nPlease run it on SysNAND instead(DSiWare exploit or Unlaunch)."); //The color value not being reset in the "BAD" string is intended. "instead(DSiWare" because it reached the end of the line
	printf("\nConsoleID:\n");
	for (int i = 7; i > -1; i--) printf("%02X", consoleIdAndCid.consoleID[i]);
	printf("\n\nCID: \n");
	for (int i = 0; i < 16; i++) printf("%02X", consoleIdAndCid.CID[i]);
	if (!isNocashFooterGood) printf("\033[31m^ At least one of these is wrong"); //the CID value fills a whole line, no \n is needed at the beginning
	printf("\n\n\033[39m");
	
	if (nandFound) {
		printf("Press Y to begin NAND restore\n");
	}
	printf("Press A to begin NAND dump\nPress START to exit\n\n");

	while(1) {

		swiWaitForVBlank();
		scanKeys();
		if      ((keysDown() & KEY_Y) && nandFound && !done) restoreNAND(&nocash_footer);
		if      ((keysDown() & KEY_A) && !done) dumpNAND(&nocash_footer);
		else if (keysDown() & KEY_START) break;
	}

	free(workbuffer);

	return 0;
}
