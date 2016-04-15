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

static Evas_Object *gesture_rectangle;
static void
_resize_canvas_gesture(Ecore_Evas *ee EINA_UNUSED)
{
   int x, y, w, h;
   ecore_evas_geometry_get(e_comp->ee, &x, &y, &w, &h);
   evas_object_geometry_set(gesture_rectangle, x, y, 300, 400);
}
static void
_gesture_init(void)
{
   gesture_rectangle = evas_object_rectangle_add(e_comp->evas);
   evas_object_layer_set(gesture_rectangle, E_LAYER_MAX);
   evas_object_repeat_events_set(gesture_rectangle, EINA_FALSE);
   _resize_canvas_gesture(e_comp->ee);
   evas_object_color_set(gesture_rectangle, 0, 100, 0, 100);
   evas_object_show(gesture_rectangle);
   ecore_evas_callback_resize_set(e_comp->ee, _resize_canvas_gesture);
}
int MAGIC_NUMBER = 987654321;
static Eina_Bool
_mouse_button_check(int type, Ecore_Event_Mouse_Button *event)
{
   Ecore_Event_Mouse_Button *ev;
DBG("[KSW] same screen: %d", event->same_screen);
   if (event->same_screen < 0)
     {
        /* Restore same_screen value and do not consume */
        event->same_screen += MAGIC_NUMBER;
DBG("[KSW][TRUE] same screen: %d, event type: %d", event->same_screen, type);
        return EINA_TRUE;
     }

   if (!(ev = calloc(1, sizeof(Ecore_Event_Mouse_Button))))
     {
        DBG("NOT ENOUGH MEMORY");
        return EINA_FALSE;
     }
#if 0
   ev->window = event->window;
   ev->event_window = event->event_window;
   ev->root_window = event->root_window;
   ev->timestamp = event->timestamp;
   ev->same_screen = event->same_screen - MAGIC_NUMBER;

   ev->x = event->x;
   ev->y = event->y;
   ev->root.x = event->root.x;
   ev->root.y = event->root.y;

   ev->multi.device = event->multi.device;
   ev->multi.radius = event->multi.radius;
   ev->multi.radius_x = event->multi.radius_x;
   ev->multi.radius_y = event->multi.radius_y;
   ev->multi.pressure = event->multi.pressure;
   ev->multi.angle = event->multi.angle;

   ev->multi.x = event->multi.x;
   ev->multi.y = event->multi.y;
   ev->multi.root.x = event->multi.root.x;
   ev->multi.root.y = event->multi.root.y;
   ev->dev = event->dev;
   ev->buttons = ev->buttons;
#endif
   ev->window = e_comp->win;
   ev->event_window = e_comp->win;
   ev->root_window = e_comp->root;
   ev->timestamp = (int)(ecore_time_get() * 1000);
   ev->same_screen = event->same_screen - MAGIC_NUMBER;

   ev->x = event->x;
   ev->y = event->y;
   ev->root.x = event->x;
   ev->root.y = event->y;

   ev->multi.device = event->multi.device;
   ev->multi.radius = 1;
   ev->multi.radius_x = 1;
   ev->multi.radius_y = 1;
   ev->multi.pressure = 1.0;
   ev->multi.angle = 0.0;

   ev->multi.x = event->x;
   ev->multi.y = event->y;
   ev->multi.root.x = event->x;
   ev->multi.root.y = event->y;

   ecore_event_add(type, ev, NULL, NULL); 
   return EINA_FALSE;
}

static Eina_Bool
_mouse_move_check(int type, Ecore_Event_Mouse_Move *event)
{
   Ecore_Event_Mouse_Move *ev;
DBG("[KSW] same screen: %d", event->same_screen);
   if (event->same_screen < 0)
     {
        /* Restore same_screen value and do not consume */
        event->same_screen += MAGIC_NUMBER;
DBG("[KSW][TRUE] same screen: %d, event type: %d", event->same_screen, type);
        return EINA_TRUE;
     }

   if (!(ev = calloc(1, sizeof(Ecore_Event_Mouse_Move))))
     {
        DBG("NOT ENOUGH MEMORY");
        return EINA_FALSE;
     }
#if 0
   ev->window = event->window;
   ev->event_window = event->event_window;
   ev->root_window = event->root_window;
   ev->timestamp = event->timestamp;
   ev->same_screen = event->same_screen - MAGIC_NUMBER;

   ev->x = event->x;
   ev->y = event->y;
   ev->root.x = event->root.x;
   ev->root.y = event->root.y;

   ev->multi.device = event->multi.device;
   ev->multi.radius = event->multi.radius;
   ev->multi.radius_x = event->multi.radius_x;
   ev->multi.radius_y = event->multi.radius_y;
   ev->multi.pressure = event->multi.pressure;
   ev->multi.angle = event->multi.angle;

   ev->multi.x = event->multi.x;
   ev->multi.y = event->multi.y;
   ev->multi.root.x = event->multi.root.x;
   ev->multi.root.y = event->multi.root.y;
   ev->dev = event->dev;
#endif
   ev->window = e_comp->win;
   ev->event_window = e_comp->win;
   ev->root_window = e_comp->root;
   ev->timestamp = (int)(ecore_time_get() * 1000);
   ev->same_screen = event->same_screen - MAGIC_NUMBER;

   ev->x = event->x;
   ev->y = event->y;
   ev->root.x = event->x;
   ev->root.y = event->y;

   ev->multi.device = event->multi.device;
   ev->multi.radius = 1;
   ev->multi.radius_x = 1;
   ev->multi.radius_y = 1;
   ev->multi.pressure = 1.0;
   ev->multi.angle = 0.0;

   ev->multi.x = event->x;
   ev->multi.y = event->y;
   ev->multi.root.x = event->x;
   ev->multi.root.y = event->y;

   ecore_event_add(type, ev, NULL, NULL); 
   return EINA_FALSE;
}
static Eina_Bool
_event_filter(void *data, void *loop_data, int type, void *event)
{
   DBG("[KSW] type: %d", type);

   if (type == ECORE_EVENT_MOUSE_BUTTON_DOWN ||
       type == ECORE_EVENT_MOUSE_BUTTON_UP)
     {
        return _mouse_button_check(type, event);
     }
   else if (type == ECORE_EVENT_MOUSE_MOVE)
     {
        return _mouse_move_check(type, event);
     }

   return EINA_TRUE;
}

static void
_filter_init(void)
{
   ecore_event_filter_add(NULL, _event_filter, NULL, NULL);
}
EAPI void *
e_modapi_init(E_Module *m)
{
   _log_dom = eina_log_domain_register("e_screen_reader", EINA_COLOR_YELLOW);
   if (_log_dom < 0)
     {
        DBG("Failed @ eina_log_domain_register()..!");
        return NULL;
     }
   DBG("[KSW] screen-reader mod init done");
   _gesture_init();
   _filter_init();
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
