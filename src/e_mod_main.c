#include "e.h"
#include "eina_log.h"
#include "e_mod_main.h"
#include <e_screen_reader_config.h>
#include <e_screen_reader_private.h>
#include <dbus/dbus.h>

#define E_A11Y_SERVICE_BUS_NAME "com.samsung.EModule"
#define E_A11Y_SERVICE_NAVI_IFC_NAME "com.samsung.GestureNavigation"
#define E_A11Y_SERVICE_NAVI_OBJ_PATH "/com/samsung/GestureNavigation"
#define E_A11Y_SERVICE_TRACKER_IFC_NAME "com.samsung.WindowTracker"
#define E_A11Y_SERVICE_TRACKER_OBJ_PATH "/com/samsung/WindowTracker"

#define E_ATSPI_BUS_TIMEOUT 4000

#undef DBG
int _eina_log_dom = -1;
#define DBG(...)  do EINA_LOG_DOM_DBG(_eina_log_dom, __VA_ARGS__); while(0)

static Eina_Bool g_gesture_navi;
static Eina_List *handlers;
const char *_a11y_bus_address;
Eldbus_Connection *conn = NULL;
Eldbus_Service_Interface *navi_iface = NULL;
Eldbus_Service_Interface *tracker_iface = NULL;
static Eldbus_Pending *_bus_request;

static Eldbus_Message *_on_get_active_window(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg);
static Eldbus_Message *_on_get_supported_gestures(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg);

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

//static Eina_Bool
//_property_get(const Eldbus_Service_Interface *iface EINA_UNUSED,
//             const char *propname, Eldbus_Message_Iter *iter,
//             const Eldbus_Message *msg EINA_UNUSED,
//             Eldbus_Message **error EINA_UNUSED)
//{
//   DEBUG("Get property request");
//   char *type = eldbus_message_iter_signature_get(iter);
//   if (type[0] == 'b' && propname && !strcmp(propname, "AccessibilityGestureNavigation"))
//     {
//        if (type) *type = DBUS_TYPE_BOOLEAN;
//        eldbus_message_iter_basic_append(iter, 'b', g_gesture_navi);
//     }
//   else
//     if (type) *type = DBUS_TYPE_INVALID;
//   return EINA_TRUE;
//}
//
//static Eldbus_Message *
//_property_set(const Eldbus_Service_Interface *iface EINA_UNUSED, const char *propname, Eldbus_Message_Iter *iter, const Eldbus_Message *msg)
//{
//   DEBUG("Set property request");
//   char *type = eldbus_message_iter_signature_get(iter);
//   Eina_Bool value;
//   if (type[0] == 'b' && propname && !strcmp(propname, "AccessibilityGestureNavigation"))
//     {
//        eldbus_message_iter_arguments_get(iter, 'b', &value);
//        if (value && !g_gesture_navi)
//          {
//             _e_mod_atspi_gestures_init();
//             g_gesture_navi = EINA_TRUE;
//
//          }
//        else if (g_gesture_navi)
//          {
//             _e_mod_atspi_gestures_shutdown();
//             g_gesture_navi = EINA_TRUE;
//          }
//     }
//
//   return eldbus_message_method_return_new(msg);
//}

//Gesture navigation eldbus interface
static const Eldbus_Signal navi_signals[] = {
      {"GestureStateChanged", ELDBUS_ARGS({"siii", NULL}), 0},
      { }
};

static const Eldbus_Method navi_methods[] = {
      {"GetSupportedGestures", NULL, ELDBUS_ARGS({"as", "array of strings"}),
       _on_get_supported_gestures
      },
      { }
};

static const Eldbus_Property navi_properties[] = {
      //{"AccessibilityGestureNavigation", "b", _property_get, _property_set, NULL },
      { }
};

static const Eldbus_Service_Interface_Desc navi_iface_desc = {
   E_A11Y_SERVICE_NAVI_IFC_NAME, navi_methods, navi_signals, navi_properties, NULL, NULL
};

//Window tracker eldbus interface
static const Eldbus_Signal tracker_signals[] = {
      {"ActiveWindowChanged", ELDBUS_ARGS({"ii", NULL}), 0},
      { }
};

static const Eldbus_Method tracker_methods[] = {
      {"GetActiveWindow", NULL, ELDBUS_ARGS({"i", "integer1"}, {"i", "integer2"}),
       _on_get_active_window
      },
      { }
};

static const Eldbus_Property tracker_properties[] = {
      { }
};

static const Eldbus_Service_Interface_Desc tracker_iface_desc = {
   E_A11Y_SERVICE_TRACKER_IFC_NAME, tracker_methods, tracker_signals, tracker_properties, NULL, NULL
};

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

static Eldbus_Message *_on_get_active_window(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   Eldbus_Message *reply = NULL;

//   Ecore_X_Window win = _e_mod_atspi_window_tracker_top_window_get();
//   pid_t pid = _e_mod_atspi_window_tracker_top_window_pid_get();
//
//   if (pid == -1)
//     {
//        ERROR("Invalid PID");
//        return dbus_message_new_error(msg, "org.freedesktop.", "Invalid");
//     }
//
//   reply = dbus_message_new_method_return(msg);
//   if (!reply)
//     {
//        ERROR("Unable to create return message.");
//        return NULL;
//     }
//
//   if (!dbus_message_append_args(reply, DBUS_TYPE_INT32, &pid, DBUS_TYPE_INT32, &win,
//                                 DBUS_TYPE_INVALID))
//     {
//        ERROR("Appending replay args faild.");
//        dbus_message_unref(reply);
//        return NULL;
//     }

   INFO("GetActiveWindow method called");
   return reply;
}

static Eldbus_Message *_on_get_supported_gestures(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   Eldbus_Message *reply;
   Eldbus_Message_Iter *iter = NULL, *iter2 = NULL;
   Gesture g;
   INFO("Get supported gestures method called");

   reply = eldbus_message_method_return_new(msg);
   if (!reply) return NULL;

   iter = eldbus_message_iter_get(reply);
   if (!iter)
     goto fail;

   iter2 = eldbus_message_iter_container_new(iter, 'a', "s");
   if (!iter2)
     goto fail;

   for (g = ONE_FINGER_FLICK_LEFT; g < GESTURES_COUNT; g++)
     {
        INFO("Supported gesture returned %s", _gesture_enum_to_string(g));
        if (!eldbus_message_iter_arguments_append(iter2, 's', _gesture_enum_to_string(g)))
          goto fail;
     }

   if (!eldbus_message_iter_container_close(iter, iter2))
     goto fail;

//   DEBUG("Get supported gestures method called");
   return reply;

fail:
   eldbus_message_unref(reply);
   return NULL;
}

int _a11y_bus_register(void)
{
   if (!conn) return 0;

//   conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SESSION);
//   conn = eldbus_private_address_connection_get(_a11y_bus_address);
//   if (!conn)
//     {
//        ERROR("Unable to integrate with ecore_main_loop");
//        eldbus_shutdown();
//        return -1;
//     }

   eldbus_name_request(conn, E_A11Y_SERVICE_BUS_NAME,
                       ELDBUS_NAME_REQUEST_FLAG_DO_NOT_QUEUE, _on_name_cb, NULL);
   navi_iface = eldbus_service_interface_register(conn, E_A11Y_SERVICE_NAVI_OBJ_PATH, &navi_iface_desc);
//   tracker_iface = eldbus_service_interface_register(conn, E_A11Y_SERVICE_TRACKER_OBJ_PATH,
//                       &tracker_iface_desc);

   DEBUG("AT-SPI dbus service initialized.");
   return 0;
}

//static void _on_get_a11y_address(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
//{
//   const char *errname, *errmsg;
//   const char *address = NULL;
//
//   _bus_request = NULL;
//   if (eldbus_message_error_get(msg, &errname, &errmsg))
//     {
//        ERR("%s %s", errname, errmsg);
//        return;
//     }
//   else
//     {
//        if (eldbus_message_arguments_get(msg, "s", &address))
//          {
//             eina_stringshare_replace(&_a11y_bus_address, address);
//             DEBUG("AT-SPI bus address: %s", address);
//             _a11y_bus_register();
//          }
//        return;
//     }
//}

/**
 * @brief Get accessibility bus address
 */
static int
_fetch_a11y_bus_address(void)
{
//   Eldbus_Message *msg = NULL;

//   if (_bus_request)
//     return -1;
   if (conn) return 0;
//   conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SESSION);
   conn = eldbus_address_connection_get("unix:path=/var/run/dbus/system_bus_socket");
   if (!conn)
     {
        ERROR("unable to get system bus");
        goto fail;
     }
   INFO("Connected to: unix:path=/var/run/dbus/system_bus_socket");

//   msg = eldbus_message_method_call_new("org.a11y.Bus", "/org/a11y/bus",
//                                      "org.a11y.Bus", "GetAddress");
//   if (!msg)
//     {
//        ERROR("DBus message allocation failed");
//        goto fail;
//     }
//
//   _bus_request = eldbus_connection_send(conn, msg, _on_get_a11y_address,
//                                      NULL, E_ATSPI_BUS_TIMEOUT);
//   eldbus_message_unref(msg);
   _a11y_bus_register();
   return 0;
fail:
//   if (msg) eldbus_message_unref(msg);
   INFO("Failed in _fetch_a11y_bus_address");
   return -1;
}

int _a11y_bus_unregister(void)
{
   if (!conn) return 0;

//   eldbus_service_interface_unregister(navi_iface);
   navi_iface = NULL;
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
   if (_e_mod_atspi_dbus_init())
     goto fail_dbus;

   _events_init();
   g_gesture_navi = EINA_TRUE;
   return m;

fail_gestures:
   ERROR("Gestures submodule initialization failed.");
   _e_mod_atspi_config_shutdown();
fail:
   ERROR("Module initialization failed.");
fail_dbus:
   ERROR("Dbus submodule initialization failed.");

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
   _e_mod_atspi_dbus_shutdown();

   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   /* Do Something */
   return 1;
}
