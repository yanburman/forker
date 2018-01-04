#pragma once

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

EXTERN int map_memory(int idx, int respawned);
EXTERN void map_remap_private(int idx);

