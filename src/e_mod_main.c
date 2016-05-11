#include "e.h"
#include "eina_log.h"
#include "e_mod_main.h"
#include <e_screen_reader_config.h>
#include <e_screen_reader_private.h>

#define E_A11Y_SERVICE_BUS_NAME "org.enlightnement.wm-screen-reader"
#define E_A11Y_SERVICE_NAVI_IFC_NAME "org.tizen.GestureNavigation"
#define E_A11Y_SERVICE_NAVI_OBJ_PATH "/org/tizen/GestureNavigation"
#define E_A11Y_SERVICE_TRACKER_IFC_NAME "org.tizen.WindowTracker"
#define E_A11Y_SERVICE_TRACKER_OBJ_PATH "/org/tizen/WindowTracker"

#define E_ATSPI_BUS_TIMEOUT 4000

#undef DBG
int _eina_log_dom = 0;
#define DBG(...)  do EINA_LOG_DOM_DBG(_eina_log_dom, __VA_ARGS__); while(0)

static Eina_Bool g_gesture_navi;
static Eina_List *handlers;
Eldbus_Connection *conn = NULL;

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
   DEBUG("atspi bus broadcast callback");
   const char *name;
   Eldbus_Message *msg;

   if (!conn) return -1;
   name = _gesture_enum_to_string(gi->type);
   if (!name) return -1;

   msg = eldbus_message_signal_new(E_A11Y_SERVICE_NAVI_OBJ_PATH,
                                    E_A11Y_SERVICE_NAVI_IFC_NAME, "GestureDetected");
   if (!msg) return -1;

   if (!eldbus_message_arguments_append(msg, "iiiiiiu", (int)gi->type, gi->x_beg, gi->y_beg, gi->x_end, gi->y_end, gi->state, gi->event_time))
     {
        eldbus_message_unref(msg);
        INFO("Append failed");
        return -1;
     }

   eldbus_connection_send(conn, msg, NULL, NULL, 0);
   INFO("GestureDetected %s %d (%d %d %d %d %d %u)", name, (int)gi->type, gi->x_beg, gi->y_beg, gi->x_end, gi->y_end, gi->state, gi->event_time);
   return 0;
}

static Eina_Bool
_gesture_cb(void    *data,
            int      type EINA_UNUSED,
            void    *event)
{
   Gesture_Info *gi = event;
   DEBUG("Gesture cb hit\n");
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
#undef HANDLER_APPEND
}

static void
_events_shutdown(void)
{
   E_FREE_LIST(handlers, ecore_event_handler_del);
}

static void
_on_name_cb(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   DEBUG("On_name_cb");
}

static void
_on_name_release_cb(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   DEBUG("On_name_release_cb");
}

static int
_e_mod_submodules_init(void)
{
   INFO("Init subsystems...");

   if (_e_mod_atspi_config_init())
     goto fail;
   if (_e_mod_atspi_gestures_init())
     goto fail_gestures;

   _events_init();
   g_gesture_navi = EINA_TRUE;
   return 0;

fail_gestures:
   ERROR("Gestures submodule initialization failed.");
   _e_mod_atspi_config_shutdown();
fail:
   ERROR("Module initialization failed.");
   return -1;
}

static void
_e_mod_submodules_shutdown(void)
{
   INFO("Shutdown subsystems...");
   _events_shutdown();
   g_gesture_navi = EINA_FALSE;
   _e_mod_atspi_config_save();
   _e_mod_atspi_config_shutdown();
   _e_mod_atspi_gestures_shutdown();
}


static Eldbus_Message *
_sc_enable(const Eldbus_Service_Interface *iface, const Eldbus_Message *msg)
{
   INFO("iface=%p.", iface);
   INFO("_sc_enable Method called");
   Eldbus_Message *reply = eldbus_message_method_return_new(msg);
   Eina_Bool bool_val = EINA_FALSE;
   if (!eldbus_message_arguments_get(msg, "b", &bool_val))
     printf("eldbus_message_arguments_get() error\n");
   eldbus_message_arguments_append(reply, "b", bool_val);
   if (bool_val)
   {
      INFO("Initialize events");
      _e_mod_submodules_init();
   }
   else
   {
      INFO("Shutdown events");
      _e_mod_submodules_shutdown();
   }
   return reply;
}

static const Eldbus_Method methods[] = {
      { "ScreenReaderEnabled", ELDBUS_ARGS({"b", "bool"}), ELDBUS_ARGS({"b", "bool"}),
        _sc_enable
      },
      { }
};

static const Eldbus_Service_Interface_Desc iface_desc = {
      E_A11Y_SERVICE_NAVI_IFC_NAME, methods
};

static int
_fetch_a11y_bus_address(void)
{
   if (conn) return 0;
   Eldbus_Service_Interface *iface = NULL;
   conn = eldbus_address_connection_get("unix:path=/var/run/dbus/system_bus_socket");
   if (!conn)
     {
        ERROR("unable to get system bus");
        goto fail;
     }
   INFO("Connected to: unix:path=/var/run/dbus/system_bus_socket");

   if (!conn) goto fail;
   eldbus_name_request(conn, E_A11Y_SERVICE_BUS_NAME,
                       ELDBUS_NAME_REQUEST_FLAG_DO_NOT_QUEUE, _on_name_cb, NULL);
   iface = eldbus_service_interface_register(conn, E_A11Y_SERVICE_NAVI_OBJ_PATH, &iface_desc);

   INFO("iface=%p.", iface);
   INFO("AT-SPI dbus service initialized.");
   return 0;
fail:
   INFO("Failed in _fetch_a11y_bus_address");
   return -1;
}

int _a11y_bus_unregister(void)
{
   if (!conn) return 0;

   eldbus_name_release(conn, E_A11Y_SERVICE_BUS_NAME, _on_name_release_cb, NULL);
   eldbus_connection_unref(conn);
   conn = NULL;

   return 0;
}

void _e_mod_atspi_dbus_shutdown()
{
   DEBUG("dbus shutdown");

   _a11y_bus_unregister();

   eldbus_shutdown();
}

int _e_mod_atspi_dbus_init(void)
{
   DEBUG("dbus init");

   eldbus_init();
   return _fetch_a11y_bus_address();
}

int _e_mod_log_init(void)
{
   if (!_eina_log_dom)
     {
        _eina_log_dom = eina_log_domain_register("e_screen_reader", EINA_COLOR_YELLOW);
        if (_eina_log_dom  < 0)
          {
             DBG("Failed @ eina_log_domain_register()..!");
             return -1;
          }
     }
   return 0;
}

void _e_mod_log_shutdown(void)
{
   if (_eina_log_dom)
     {
        eina_log_domain_unregister(_eina_log_dom);
        _eina_log_dom = 0;
     }
}

EAPI void *
e_modapi_init(E_Module *m)
{
   _e_mod_log_init();
   if (_e_mod_atspi_dbus_init())
     goto fail;

   return m;
fail:
   ERROR("Dbus initialization failed.");

   return NULL;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   _e_mod_log_shutdown();
   _e_mod_atspi_dbus_shutdown();

   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   /* Do Something */
   return 1;
}
