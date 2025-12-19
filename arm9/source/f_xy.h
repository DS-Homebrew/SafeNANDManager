#ifndef _H_F_XY
#define _H_F_XY

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/************************ Constants / Defines *********************************/

extern const uint8_t DSi_KEY_MAGIC[16];
extern const uint8_t DSi_NAND_KEY_Y[16];

/************************ Function Protoypes **********************************/

void F_XY(uint8_t *key, const uint8_t *key_x, const uint8_t *key_y);
void F_XY_reverse(const uint8_t *key, uint8_t *key_xy);

#ifdef __cplusplus
}
#endif

#endif

