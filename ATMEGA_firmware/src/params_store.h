/*
 * params_store.h - the parameter file in EEPROM (docs/04_register_maps.md).
 */
#ifndef PARAMS_STORE_H
#define PARAMS_STORE_H

#include "params.h"

/* Load from EEPROM. Returns true if the stored file was valid. If not, `p` is filled with the
 * defaults (nothing is written back automatically). */
bool params_load(Params &p);

/* Seal (recompute the CRC) and write to EEPROM. Uses update, so unchanged bytes are not rewritten. */
void params_save(Params &p);

#endif /* PARAMS_STORE_H */
