#include <string.h>
#include <stdint.h>
#include "u128_math.h"
#include "f_xy.h"

const uint8_t DSi_KEY_MAGIC[16] = {
	0x79, 0x3e, 0x4f, 0x1a, 0x5f, 0x0f, 0x68, 0x2a,
	0x58, 0x02, 0x59, 0x29, 0x4e, 0xfb, 0xfe, 0xff
};
const uint8_t DSi_NAND_KEY_Y[16] = {
	0x76, 0xdc, 0xb9, 0x0a, 0xd3, 0xc4, 0x4d, 0xbd,
	0x1d, 0xdd, 0x2d, 0x20, 0x05, 0x00, 0xa0, 0xe1
};

/************************ Functions *******************************************/

void F_XY(uint8_t *key, const uint8_t *key_x, const uint8_t *key_y)
{
	uint8_t key_xy[16];

	for (int i=0; i<16; i++)
		key_xy[i] = key_x[i] ^ key_y[i];

	memcpy(key, DSi_KEY_MAGIC, sizeof(DSi_KEY_MAGIC));

	u128_add(key, key_xy);
	u128_lrot(key, 42);
}

//F_XY_reverse does the reverse of F(X^Y): takes (normal)key, and does F in reverse to generate the original X^Y key_xy.
void F_XY_reverse(const uint8_t *key, uint8_t *key_xy)
{
	memcpy(key_xy, key, 16);
	u128_rrot(key_xy, 42);
	u128_sub(key_xy, DSi_KEY_MAGIC);
}
