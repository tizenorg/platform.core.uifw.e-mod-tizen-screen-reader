#include "e.h"
#include "eina_log.h"
#include "e_mod_main.h"
#include <dlog.h>

#undef DBG
int _log_dom = -1;
#define DBG(...)  do EINA_LOG_DOM_DBG(_log_dom, __VA_ARGS__); while(0)
#define LOG_TAG "SCREEN_READER_GESTURE"
EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Screen Reader Module of Window Manager"
};

typedef enum {
     GESTURE_NOT_STARTED = 0, // Gesture is ready to start
     GESTURE_ONGOING,         // Gesture in progress.
     GESTURE_FINISHED,        // Gesture finished - should be emited
     GESTURE_ABORTED          // Gesture aborted
} gesture_state_e;

typedef enum {
      ONE_FINGER_GESTURE = 1,
      TWO_FINGERS_GESTURE,
      THREE_FINGERS_GESTURE
} gesture_type_e;

enum _Gesture {
     ONE_FINGER_HOVER,
     TWO_FINGERS_HOVER,
     THREE_FINGERS_HOVER,
     ONE_FINGER_FLICK_LEFT,
     ONE_FINGER_FLICK_RIGHT,
     ONE_FINGER_FLICK_UP,
     ONE_FINGER_FLICK_DOWN,
     TWO_FINGERS_FLICK_LEFT,
     TWO_FINGERS_FLICK_RIGHT,
     TWO_FINGERS_FLICK_UP,
     TWO_FINGERS_FLICK_DOWN,
     THREE_FINGERS_FLICK_LEFT,
     THREE_FINGERS_FLICK_RIGHT,
     THREE_FINGERS_FLICK_UP,
     THREE_FINGERS_FLICK_DOWN,
     ONE_FINGER_SINGLE_TAP,
     ONE_FINGER_DOUBLE_TAP,
     ONE_FINGER_TRIPLE_TAP,
     TWO_FINGERS_SINGLE_TAP,
     TWO_FINGERS_DOUBLE_TAP,
     TWO_FINGERS_TRIPLE_TAP,
     THREE_FINGERS_SINGLE_TAP,
     THREE_FINGERS_DOUBLE_TAP,
     THREE_FINGERS_TRIPLE_TAP,
     ONE_FINGER_FLICK_LEFT_RETURN,
     ONE_FINGER_FLICK_RIGHT_RETURN,
     ONE_FINGER_FLICK_UP_RETURN,
     ONE_FINGER_FLICK_DOWN_RETURN,
     TWO_FINGERS_FLICK_LEFT_RETURN,
     TWO_FINGERS_FLICK_RIGHT_RETURN,
     TWO_FINGERS_FLICK_UP_RETURN,
     TWO_FINGERS_FLICK_DOWN_RETURN,
     THREE_FINGERS_FLICK_LEFT_RETURN,
     THREE_FINGERS_FLICK_RIGHT_RETURN,
     THREE_FINGERS_FLICK_UP_RETURN,
     THREE_FINGERS_FLICK_DOWN_RETURN,
     GESTURES_COUNT,
};

typedef enum _Gesture Gesture;

struct _Cover
{
   Evas_Object   *gesture_rect; /**< Gesture rectangle */
   unsigned int    n_taps; /**< Number of fingers touching screen */
   unsigned int    event_time;

 /*  struct {
        gesture_state_e state;     // current state of gesture
        unsigned int timestamp[3]; // time of gesture;
        int finger[3];             // finger number which initiates gesture
        int x_org[3], y_org[3];    // coorinates of finger down event
        int x_end[3], y_end[3];    // coorinates of finger up event
        flick_direction_e dir;     // direction of flick
        int n_fingers;             // number of fingers in gesture
        int n_fingers_left;        // number of fingers in gesture
                                   //         still touching screen
        Eina_Bool finger_out[3];   // finger is out of the finger boundary
        Eina_Bool return_flick[3];
   } flick_gesture;
*/
 /*  struct {
        gesture_state_e state;   // currest gesture state
        int x[2], y[2];
        int n_fingers;
        int finger[2];
        unsigned int timestamp; // time of gesture;
        unsigned int last_emission_time; // last time of gesture emission
        Ecore_Timer *timer;
        Eina_Bool longpressed;
   } hover_gesture;*/

   struct {
        Eina_Bool started; // indicates if taps recognition process has started
        Eina_Bool pressed; // indicates if finger is down
        int n_taps;        // number of taps captures in sequence
        int finger[3];        // device id of finget
        Ecore_Timer *timer;  // sequence expiration timer
        int x_org[3], y_org[3];    // coordinates of first tap
        gesture_type_e tap_type;
   } tap_gesture_data;
};
typedef struct _Cover Cover;

typedef struct {
     Gesture type;         // Type of recognized gesture
     int x_beg, x_end;     // (x,y) coordinates when gesture begin
     int y_beg, y_end;     // (x,y) coordinates when gesture ends
     int state;            // 0 - begin, 1 - ongoing, 2 - ended
     unsigned int event_time; //time stamp for the event
} Gesture_Info;

static Cover *cover;
static Evas_Object *gesture_rectangle;
int E_EVENT_ATSPI_GESTURE_DETECTED;

static void _event_emit(Gesture g, int x, int y, int x_e, int y_e, int state, unsigned int event_time)
{
   Gesture_Info *info = calloc(sizeof(Gesture_Info), 1);
   EINA_SAFETY_ON_NULL_RETURN(info);

   info->type = g;
   info->x_beg = x;
   info->x_end = x_e;
   info->y_beg = y;
   info->y_end = y_e;
   info->state = state;
   info->event_time = event_time;
   dlog_print(DLOG_DEBUG, LOG_TAG, "Event Emitted");
   ecore_event_add(E_EVENT_ATSPI_GESTURE_DETECTED, info, NULL, NULL);
}


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
   if (event->multi.radius == MAGIC_NUMBER)
     {
        /* change value here */
        event->x += 100;
        event->y += 100;
DBG("[KSW]>>>>>     [TRUE] same_screen: %d, (x, y: %d, %d), (root x, y: %d, %d), mulit.device: %d", event->same_screen, event->x, event->y, event->root.x, event->root.y, event->multi.device);
        return EINA_TRUE;
     }

   if (!(ev = malloc(sizeof(Ecore_Event_Mouse_Button))))
     {
        DBG("NOT ENOUGH MEMORY");
        return EINA_FALSE;
     }
   memcpy(ev, event, sizeof(Ecore_Event_Mouse_Button));
  
   ev->timestamp = (int)(ecore_time_get() * 1000);
   ev->multi.radius = MAGIC_NUMBER;
DBG("same_screen: %d, (x, y: %d, %d), (root x, y: %d, %d), multi.device: %d", ev->same_screen, ev->x, ev->y, ev->root.x, ev->root.y, ev->multi.device);
   ecore_event_add(type, ev, NULL, NULL); 
   return EINA_FALSE;
}

static Eina_Bool
_mouse_move_check(int type, Ecore_Event_Mouse_Move *event)
{
   Ecore_Event_Mouse_Move *ev;
   if (event->multi.radius == MAGIC_NUMBER)
     {
        /* change value here */
        event->x += 100;
        event->y += 100;
DBG("[KSW]>>>>>     [TRUE] same_screen: %d, (x, y: %d, %d), (root x, y: %d, %d), multi.device: %d", event->same_screen, event->x, event->y, event->root.x, event->root.y, event->multi.device);
        return EINA_TRUE;
     }

   if (!(ev = malloc(sizeof(Ecore_Event_Mouse_Move))))
     {
        DBG("NOT ENOUGH MEMORY");
        return EINA_FALSE;
     }
   memcpy(ev, event, sizeof(Ecore_Event_Mouse_Move));

   ev->timestamp = (int)(ecore_time_get() * 1000);
   ev->multi.radius = MAGIC_NUMBER;
DBG("same_screen: %d, (x, y: %d, %d), (root x, y: %d, %d), multi.device: %d", ev->same_screen, ev->x, ev->y, ev->root.x, ev->root.y, ev->multi.device);
   ecore_event_add(type, ev, NULL, NULL); 
   return EINA_FALSE;
}

static void
_tap_event_emit(Cover *cov)
{
   switch (cov->tap_gesture_data.n_taps)
     {
      case 1:
         if(cov->tap_gesture_data.tap_type == ONE_FINGER_GESTURE)
            {
            dlog_print(DLOG_DEBUG, LOG_TAG,"ONE_FINGER_SINGLE_TAP");
               _event_emit(ONE_FINGER_SINGLE_TAP,
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     2, cov->event_time);
            }
     }
}

static Eina_Bool
_on_tap_timer_expire(void *data)
{
   Cover *cov = data;
   dlog_print(DLOG_DEBUG, LOG_TAG,"Timer expired");

   if (cov->tap_gesture_data.started && !cov->tap_gesture_data.pressed)
     _tap_event_emit(cov);

   // finish gesture
   cov->tap_gesture_data.started = EINA_FALSE;
   cov->tap_gesture_data.timer = NULL;
   cov->tap_gesture_data.tap_type = ONE_FINGER_GESTURE;
   cov->tap_gesture_data.finger[0] = -1;
   cov->tap_gesture_data.finger[1] = -1;
   cov->tap_gesture_data.finger[2] = -1;

   return EINA_FALSE;
}

static Eina_Bool
_mouse_down_check(int type, Ecore_Event_Mouse_Button *event, Cover *cov)
{
   Ecore_Event_Mouse_Move *ev;
   if (event->multi.radius == MAGIC_NUMBER)
     {
        /* storing values here */
        cov->tap_gesture_data.finger[0] = ev->multi.device;
        cov->tap_gesture_data.x_org[0] = ev->root.x;
        cov->tap_gesture_data.y_org[0] = ev->root.y;
        cov->tap_gesture_data.finger[1] = -1;
        cov->tap_gesture_data.finger[2] = -1;
        cov->tap_gesture_data.n_taps = 0;
        cov->tap_gesture_data.timer = ecore_timer_add(
                                          0.4,
                                          _on_tap_timer_expire, cov);
        cov->tap_gesture_data.tap_type = ONE_FINGER_GESTURE;
        dlog_print(DLOG_DEBUG, LOG_TAG,"[KSW]>>>>>     [TRUE] same_screen: %d, (x, y: %d, %d), (root x, y: %d, %d), multi.device: %d", event->same_screen, event->x, event->y, event->root.x, event->root.y, event->multi.device);
        return EINA_TRUE;
     }

   if (!(ev = malloc(sizeof(Ecore_Event_Mouse_Move))))
     {
      dlog_print(DLOG_DEBUG, LOG_TAG,"NOT ENOUGH MEMORY");
        return EINA_FALSE;
     }
   memcpy(ev, event, sizeof(Ecore_Event_Mouse_Move));

   ev->timestamp = (int)(ecore_time_get() * 1000);
   ev->multi.radius = MAGIC_NUMBER;
   dlog_print(DLOG_DEBUG, LOG_TAG,"same_screen: %d, (x, y: %d, %d), (root x, y: %d, %d), multi.device: %d", ev->same_screen, ev->x, ev->y, ev->root.x, ev->root.y, ev->multi.device);
   ecore_event_add(type, ev, NULL, NULL);
   return EINA_FALSE;
}

static Eina_Bool
_event_filter(void *data, void *loop_data, int type, void *event)
{
   dlog_print(DLOG_DEBUG, LOG_TAG, "[KSW] type: %d", type);

   if (type == ECORE_EVENT_MOUSE_BUTTON_DOWN)
     {
        return _mouse_down_check(type, event,cover);
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
   if (!E_EVENT_ATSPI_GESTURE_DETECTED)
      E_EVENT_ATSPI_GESTURE_DETECTED = ecore_event_type_new();
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
