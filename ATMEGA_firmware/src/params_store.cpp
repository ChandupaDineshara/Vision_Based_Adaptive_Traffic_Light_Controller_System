#include "params_store.h"
#include <EEPROM.h>

bool params_load(Params &p)
{
  EEPROM.get(0, p);
  if (params_valid(p)) return true;
  params_set_defaults(p);
  return false;
}

void params_save(Params &p)
{
  params_seal(p);
  EEPROM.put(0, p);   /* put() only writes bytes that changed */
}
