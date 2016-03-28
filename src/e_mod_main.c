#include "e.h"
#include "eina_log.h"
#include "e_mod_main.h"

#undef DBG
int _log_dom = -1;
#define DBG(...)  do EINA_LOG_DOM_DBG(_log_dom, __VA_ARGS__); while(0)

EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Screen Reader Module of Window Manager"
};

EAPI void *
e_modapi_init(E_Module *m)
{
   _log_dom = eina_log_domain_register("e_screen_reader", EINA_COLOR_YELLOW);
   if (_log_dom < 0)
     {
        DBG("Failed @ eina_log_domain_register()..!");
        return NULL;
     }

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   eina_log_domain_unregister(_log_dom);

   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   /* Do Something */
   return 1;
}
