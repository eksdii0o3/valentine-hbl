#ifndef _UTILS_H_
#define _UTILS_H_

#include <common/sdk.h>

#define PSP_GO 2
#define PSP_OTHER 3

#ifndef VITA
u32 get_fw_ver();
u32 getPSPModel();
#endif

//Returns 1 if address is a valid PSP umem address, 0 otherwise
int valid_umem_pointer(u32 addr);
#endif
