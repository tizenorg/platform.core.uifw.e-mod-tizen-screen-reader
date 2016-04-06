#include "e.h"
#include "eina_log.h"
#include "e_mod_main.h"
#include <e_screen_reader_config.h>
#include <e_screen_reader_private.h>

#undef DBG
int _eina_log_dom = -1;
#define DBG(...)  do EINA_LOG_DOM_DBG(_eina_log_dom, __VA_ARGS__); while(0)

static Eina_Bool g_gesture_navi;
static Eina_List *handlers;

EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Screen Reader Module of Window Manager"
};

/* Use this util function for gesture enum to string conversion */
static const char *_gesture_enum_to_string(Gesture g)
{
   switch(g)
     {
      case ONE_FINGER_HOVER:
         return "OneFingerHover";
      case TWO_FINGERS_HOVER:
         return "TwoFingersHover";
      case THREE_FINGERS_HOVER:
         return "ThreeFingersHover";
      case ONE_FINGER_FLICK_LEFT:
         return "OneFingerFlickLeft";
      case ONE_FINGER_FLICK_RIGHT:
         return "OneFingerFlickRight";
      case ONE_FINGER_FLICK_UP:
         return "OneFingerFlickUp";
      case ONE_FINGER_FLICK_DOWN:
         return "OneFingerFlickDown";
      case TWO_FINGERS_FLICK_UP:
         return "TwoFingersFlickUp";
      case TWO_FINGERS_FLICK_DOWN:
         return "TwoFingersFlickDown";
      case TWO_FINGERS_FLICK_LEFT:
         return "TwoFingersFlickLeft";
      case TWO_FINGERS_FLICK_RIGHT:
         return "TwoFingersFlickRight";
      case THREE_FINGERS_FLICK_LEFT:
         return "ThreeFingersFlickLeft";
      case THREE_FINGERS_FLICK_RIGHT:
         return "ThreeFingersFlickRight";
      case THREE_FINGERS_FLICK_UP:
         return "ThreeFingersFlickUp";
      case THREE_FINGERS_FLICK_DOWN:
         return "ThreeFingersFlickDown";
      case ONE_FINGER_SINGLE_TAP:
         return "OneFingerSingleTap";
      case ONE_FINGER_DOUBLE_TAP:
         return "OneFingerDoubleTap";
      case ONE_FINGER_TRIPLE_TAP:
         return "OneFingerTripleTap";
      case TWO_FINGERS_SINGLE_TAP:
         return "TwoFingersSingleTap";
      case TWO_FINGERS_DOUBLE_TAP:
         return "TwoFingersDoubleTap";
      case TWO_FINGERS_TRIPLE_TAP:
         return "TwoFingersTripleTap";
      case THREE_FINGERS_SINGLE_TAP:
         return "ThreeFingersSingleTap";
      case THREE_FINGERS_DOUBLE_TAP:
         return "ThreeFingersDoubleTap";
      case THREE_FINGERS_TRIPLE_TAP:
         return "ThreeFingersTripleTap";
      case ONE_FINGER_FLICK_LEFT_RETURN:
         return "OneFingerFlickLeftReturn";
      case ONE_FINGER_FLICK_RIGHT_RETURN:
         return "OneFingerFlickRightReturn";
      case ONE_FINGER_FLICK_UP_RETURN:
         return "OneFingerFlickUpReturn";
      case ONE_FINGER_FLICK_DOWN_RETURN:
         return "OneFingerFlickDownReturn";
      case TWO_FINGERS_FLICK_LEFT_RETURN:
         return "TwoFingersFlickLeftReturn";
      case TWO_FINGERS_FLICK_RIGHT_RETURN:
         return "TwoFingersFlickRightReturn";
      case TWO_FINGERS_FLICK_UP_RETURN:
         return "TwoFingersFlickUpReturn";
      case TWO_FINGERS_FLICK_DOWN_RETURN:
         return "TwoFingersFlickDownReturn";
      case THREE_FINGERS_FLICK_LEFT_RETURN:
         return "ThreeFingersFlickLeftReturn";
      case THREE_FINGERS_FLICK_RIGHT_RETURN:
         return "ThreeFingersFlickRightReturn";
      case THREE_FINGERS_FLICK_UP_RETURN:
         return "ThreeFingersFlickUpReturn";
      case THREE_FINGERS_FLICK_DOWN_RETURN:
         return "ThreeFingersFlickDownReturn";
      default:
         ERROR("[atspi] dbus: unhandled gesture enum");
         return NULL;
     }
}

int _e_mod_atspi_dbus_broadcast(Gesture_Info *gi)
{
   /* Implement this for gesture broadcast */
   const char *name;

   name = _gesture_enum_to_string(gi->type);
   if (!name) return -1;

   INFO("GestureDetected %s (%d %d %d %d %d)", name, gi->x_beg, gi->y_beg, gi->x_end, gi->y_end, gi->state);
   return 0;
}

static Eina_Bool
_gesture_cb(void    *data,
            int      type EINA_UNUSED,
            void    *event)
{
   Gesture_Info *gi = event;

   if (g_gesture_navi)
     _e_mod_atspi_dbus_broadcast(gi);

   return EINA_TRUE;
}

static void
_events_init(void)
{
#define HANDLER_APPEND(event, cb) \
   handlers = eina_list_append( \
      handlers, ecore_event_handler_add(event, cb, NULL));
   HANDLER_APPEND(E_EVENT_ATSPI_GESTURE_DETECTED, _gesture_cb);
   /* Use this list for other handlers */
#undef APPEND_HANDLER
}

static void
_events_shutdown(void)
{
   E_FREE_LIST(handlers, ecore_event_handler_del);
}

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

   _events_init();
   g_gesture_navi = EINA_TRUE;
   return m;

fail_gestures:
   ERROR("Gestures submodule initialization failed.");
   _e_mod_atspi_config_shutdown();
fail:
   ERROR("Module initialization failed.");

   return NULL;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   if (_eina_log_dom)
     {
        eina_log_domain_unregister(_eina_log_dom);
        _eina_log_dom = 0;
     }

   _events_shutdown();
   g_gesture_navi = EINA_FALSE;
   _e_mod_atspi_config_save();
   _e_mod_atspi_config_shutdown();
   _e_mod_atspi_gestures_shutdown();

   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   /* Do Something */
   return 1;
}
