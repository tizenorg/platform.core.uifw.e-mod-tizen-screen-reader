#include "e.h"
#include "eina_log.h"
#include "e_mod_main.h"
#include <e_screen_reader_config.h>
#include <e_screen_reader_private.h>

#undef DBG
int _eina_log_dom = -1;
#define DBG(...)  do EINA_LOG_DOM_DBG(_eina_log_dom, __VA_ARGS__); while(0)

EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Screen Reader Module of Window Manager"
};

EAPI void *
e_modapi_init(E_Module *m)
{
   _eina_log_dom = eina_log_domain_register("e_screen_reader", EINA_COLOR_YELLOW);
   if (!_eina_log_dom)
     {
        DBG("Failed @ eina_log_domain_register()..!");
        return NULL;
     }

   if (_e_mod_atspi_config_init())
     goto fail;
   if (_e_mod_atspi_gestures_init())
     goto fail_gestures;

fail_gestures:
   ERROR("Gestures submodule initialization failed.");
   _e_mod_atspi_config_shutdown();
fail:
   ERROR("Module initialization failed.");

   //Start from here
   //DBG("ecore evas: %p", e_comp->ee);

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   eina_log_domain_unregister(_eina_log_dom);

   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   /* Do Something */
   return 1;
}
