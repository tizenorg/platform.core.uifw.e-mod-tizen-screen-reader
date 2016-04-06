#include <e.h>
#include <e_screen_reader_config.h>
#include <e_screen_reader_private.h>

Gestures_Config *_e_mod_config;

static E_Config_DD *_conf_edd;


static void
_e_mod_config_new(void)
{
   _e_mod_config = E_NEW(Gestures_Config, 1);

   _e_mod_config->one_finger_flick_min_length = 100;
   _e_mod_config->one_finger_flick_max_time = 400;
   _e_mod_config->one_finger_hover_longpress_timeout = 0.81;
   _e_mod_config->two_fingers_hover_longpress_timeout = 0.1;
   _e_mod_config->one_finger_tap_timeout = 0.4;
   _e_mod_config->one_finger_tap_radius = 100;
}

int _e_mod_atspi_config_init(void)
{
   DEBUG("Config init");
   _conf_edd = E_CONFIG_DD_NEW("Gestures_Config", Gestures_Config);

#define T Gestures_Config
#define D _conf_edd
   E_CONFIG_VAL(D, T, one_finger_flick_min_length, INT);
   E_CONFIG_VAL(D, T, one_finger_flick_max_time, INT);
   E_CONFIG_VAL(D, T, one_finger_hover_longpress_timeout, DOUBLE);
   E_CONFIG_VAL(D, T, two_fingers_hover_longpress_timeout, DOUBLE);
   E_CONFIG_VAL(D, T, one_finger_tap_timeout, DOUBLE);
   E_CONFIG_VAL(D, T, one_finger_tap_radius, INT);

   _e_mod_config = e_config_domain_load(E_ATSPI_CFG, _conf_edd);

   if (!_e_mod_config)
     {
        _e_mod_config_new();
        _e_mod_atspi_config_save();
        INFO("New config file for e-mod-tizen-screen-reader module created.");
     }
   else
     INFO("Config file for e-mod-tizen-screen-reader module loaded successfully.");

   return 0;
}

int _e_mod_atspi_config_shutdown(void)
{
   DEBUG("Config shutdown");
   E_FREE(_e_mod_config);
   E_CONFIG_DD_FREE(_conf_edd);

   return 0;
}

int _e_mod_atspi_config_save(void)
{
   return e_config_domain_save(E_ATSPI_CFG, _conf_edd, _e_mod_config);
}
