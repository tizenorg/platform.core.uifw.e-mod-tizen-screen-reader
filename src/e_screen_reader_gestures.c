#include "e.h"
#include "e_comp.h"
#include "e_screen_reader_private.h"


#define HISTORY_MAX 8
#define LONGPRESS_TIMEOUT 0.4
double MAGIC_NUMBER = 987654321.0;

typedef enum {
     FLICK_DIRECTION_UNDEFINED,
     FLICK_DIRECTION_DOWN,
     FLICK_DIRECTION_UP,
     FLICK_DIRECTION_LEFT,
     FLICK_DIRECTION_RIGHT,
     FLICK_DIRECTION_DOWN_RETURN,
     FLICK_DIRECTION_UP_RETURN,
     FLICK_DIRECTION_LEFT_RETURN,
     FLICK_DIRECTION_RIGHT_RETURN,
} flick_direction_e;

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

struct _Cover
{
   Evas_Object   *gesture_rect; /**< Gesture rectangle */
   unsigned int    n_taps; /**< Number of fingers touching screen */
   unsigned int    event_time;
   int angle;
   struct {
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
        Eina_Bool flick_to_scroll;
        Ecore_Event_Mouse_Button *ev_first_down;
        Ecore_Event_Mouse_Button *ev_up;
        int flick_to_scroll_last_x;
        int flick_to_scroll_last_y;
   } flick_gesture;

   struct {
        gesture_state_e state;   // currest gesture state
        int x[2], y[2];
        int n_fingers;
        int finger[2];
        unsigned int timestamp; // time of gesture;
        unsigned int last_emission_time; // last time of gesture emission
        Ecore_Timer *timer;
        Eina_Bool longpressed;
   } hover_gesture;

   struct {
        Eina_Bool started; // indicates if taps recognition process has started
        Eina_Bool pressed; // indicates if finger is down
        int n_taps;        // number of taps captures in sequence
        int finger[3];        // device id of finget
        Ecore_Timer *timer;  // sequence expiration timer
        int x_org[3], y_org[3];    // coordinates of first tap
        gesture_type_e tap_type;
   } tap_gesture_data;

   struct {
        int n_taps;
        Eina_Bool double_tap;
        Ecore_Event_Mouse_Button *ev_down;
        Eina_Bool drag_start;
   } tap_n_hold_gesture_data;
};
typedef struct _Cover Cover;

int E_EVENT_ATSPI_GESTURE_DETECTED;

static Cover *cover;
static Ecore_Event_Filter *ef;
static Ecore_Event_Handler *eh;
static void _gesture_init(void);
static void _gesture_shutdown(void);
static void _hover_event_emit(Cover *cov, gesture_state_e state);

static void
_gesture_info_free(void *data, void *info)
{
   free(data);
}

int gesture_state_enum_to_int(gesture_state_e state)
{
   int gesture_state;
   switch (state)
     {
      case GESTURE_NOT_STARTED:
        gesture_state = 0;
        break;

      case GESTURE_ONGOING:
        gesture_state = 1;
        break;

      case GESTURE_FINISHED:
        gesture_state = 2;
        break;

      case GESTURE_ABORTED:
        gesture_state = 3;
        break;

      default:
        gesture_state = -1;
        break;
     }

   return gesture_state;
}

static Eina_Bool
_rotation_cb_change_end(void *data, int type, void *event)
{
   E_Event_Client *ev = event;
   E_Client *ec = ev->ec;
   Cover *cov = data;
   cov->angle = ec->e.state.rot.ang.curr;

   return ECORE_CALLBACK_PASS_ON;
}

static void _emit_mouse_move_event ( Ecore_Event_Mouse_Button *ev_btn)
{
   Ecore_Event_Mouse_Move *ev_move;//that is because of the EFL bug. Mouse move event before mouse down, if move is not send before down then coordinates are not coming right for mouse down
   if (!(ev_move = malloc(sizeof(Ecore_Event_Mouse_Move))))
     {
        DEBUG("NOT ENOUGH MEMORY");
        return ;
     }
   ev_move->window = ev_btn->window;
   ev_move->event_window = ev_btn->event_window;
   ev_move->root_window = ev_btn->root_window;
   ev_move->same_screen = ev_btn->same_screen;
   ev_move->dev = ev_btn->dev;

   ev_move->x = ev_btn->x;
   ev_move->y = ev_btn->y;
   ev_move->root.x = ev_btn->root.x;
   ev_move->root.y = ev_btn->root.y;

   ev_move->multi.device = ev_btn->multi.device;

   ev_move->multi.radius_x = ev_btn->multi.radius_x;
   ev_move->multi.radius_y = ev_btn->multi.radius_y;
   ev_move->multi.pressure = ev_btn->multi.pressure;
   ev_move->multi.angle = ev_btn->multi.angle;

   ev_move->multi.x = ev_btn->multi.x;
   ev_move->multi.y = ev_btn->multi.y;
   ev_move->multi.root.x = ev_btn->multi.root.x;
   ev_move->multi.root.y = ev_btn->multi.root.y;
   ev_move->timestamp = (int)(ecore_time_get() * 1000);
   ev_move->multi.radius = MAGIC_NUMBER + 10.0;
   ecore_event_add(ECORE_EVENT_MOUSE_MOVE, ev_move, NULL, NULL);
}

void __transform_coordinates(int *ax, int *ay, int win_angle)
{
    int w, h, tmp;

    ecore_wl_screen_size_get(&w, &h);
    switch (win_angle) {
       case 90:
          tmp = *ax;
          *ax = h - *ay;
          *ay = tmp;
          break;
       case 270:
          tmp = *ax;
          *ax = *ay;
          *ay = w - tmp;
          break;
    }
}

static void _event_emit(Gesture g, int x, int y, int x_e, int y_e, gesture_state_e state, unsigned int event_time, int angle)
{
   Gesture_Info *info = calloc(sizeof(Gesture_Info), 1);
   EINA_SAFETY_ON_NULL_RETURN(info);

   __transform_coordinates(&x, &y, angle);
   __transform_coordinates(&x_e, &y_e, angle);

   info->type = g;
   info->x_beg = x;
   info->x_end = x_e;
   info->y_beg = y;
   info->y_end = y_e;
   info->state = gesture_state_enum_to_int(state);
   info->event_time = event_time;

   ecore_event_add(E_EVENT_ATSPI_GESTURE_DETECTED, info, _gesture_info_free, NULL);
}

static void
_flick_gesture_mouse_down(Ecore_Event_Mouse_Button *ev, Cover *cov)
{
   if (cov->flick_gesture.state == GESTURE_NOT_STARTED)
     {
        cov->flick_gesture.state = GESTURE_ONGOING;
        cov->flick_gesture.finger[0] = ev->multi.device;
        cov->flick_gesture.x_org[0] = ev->root.x;
        cov->flick_gesture.y_org[0] = ev->root.y;
        cov->flick_gesture.timestamp[0] = ev->timestamp;
        cov->flick_gesture.flick_to_scroll = EINA_FALSE;
        cov->flick_gesture.n_fingers = 1;
        cov->flick_gesture.n_fingers_left = 1;
        cov->flick_gesture.dir = FLICK_DIRECTION_UNDEFINED;
        cov->flick_gesture.finger_out[0] = EINA_FALSE;
        cov->flick_gesture.return_flick[0] = EINA_FALSE;
        if (!(cov->flick_gesture.ev_first_down = malloc(sizeof(Ecore_Event_Mouse_Button))))
          {
             DEBUG("NOT ENOUGH MEMORY");
             return ;
          }
        memcpy (cov->flick_gesture.ev_first_down, ev, sizeof(Ecore_Event_Mouse_Button));
     }
   else if (cov->flick_gesture.state == GESTURE_ONGOING)
     {
        // abort gesture if too many fingers touched screen
        if ((cov->n_taps > 3) || (cov->flick_gesture.n_fingers > 2))
          {
             cov->flick_gesture.state = GESTURE_ABORTED;
             return;
          }

        cov->flick_gesture.x_org[cov->flick_gesture.n_fingers] = ev->root.x;
        cov->flick_gesture.y_org[cov->flick_gesture.n_fingers] = ev->root.y;
        cov->flick_gesture.timestamp[cov->flick_gesture.n_fingers] = ev->timestamp;
        cov->flick_gesture.finger[cov->flick_gesture.n_fingers] = ev->multi.device;
        cov->flick_gesture.n_fingers++;
        cov->flick_gesture.n_fingers_left++;
         if (cov->flick_gesture.n_fingers < 3) /* n_fingers == 3 makes out of bounds write */
           {
              cov->flick_gesture.finger_out[cov->flick_gesture.n_fingers] = EINA_FALSE;
              cov->flick_gesture.return_flick[cov->flick_gesture.n_fingers] = EINA_FALSE;
           }
     }
}

static Eina_Bool
_flick_gesture_time_check(unsigned int event_time, unsigned int gesture_time)
{
   DEBUG("Flick time: %d", event_time - gesture_time);
   if ((event_time - gesture_time) < _e_mod_config->one_finger_flick_max_time * 2) //Double time because of the possible of return flick
     return EINA_TRUE;
   else
     return EINA_FALSE;
}

static Eina_Bool
_flick_gesture_length_check(int x, int y, int x_org, int y_org)
{
   int dx = x - x_org;
   int dy = y - y_org;

   if ((dx * dx + dy * dy) > (_e_mod_config->one_finger_flick_min_length *
                            _e_mod_config->one_finger_flick_min_length))
     return EINA_TRUE;
   else
     return EINA_FALSE;
}

static flick_direction_e
_flick_gesture_direction_get(int x, int y, int x_org, int y_org)
{
   int dx = x - x_org;
   int dy = y - y_org;

   if ((dy < 0) && (abs(dx) < -dy))
     return FLICK_DIRECTION_UP;
   if ((dy > 0) && (abs(dx) < dy))
     return FLICK_DIRECTION_DOWN;
   if ((dx > 0) && (dx > abs(dy)))
     return FLICK_DIRECTION_RIGHT;
   if ((dx < 0) && (-dx > abs(dy)))
     return FLICK_DIRECTION_LEFT;

   return FLICK_DIRECTION_UNDEFINED;
}

static void
_flick_event_emit(Cover *cov)
{
   int ax, ay, axe, aye, i, type = -1;
   ax = ay = axe = aye = 0;

   for (i = 0; i < cov->flick_gesture.n_fingers; i++)
     {
        ax += cov->flick_gesture.x_org[i];
        ay += cov->flick_gesture.y_org[i];
        axe += cov->flick_gesture.x_end[i];
        aye += cov->flick_gesture.y_end[i];
     }

   ax /= cov->flick_gesture.n_fingers;
   ay /= cov->flick_gesture.n_fingers;
   axe /= cov->flick_gesture.n_fingers;
   aye /= cov->flick_gesture.n_fingers;

   if (cov->flick_gesture.dir == FLICK_DIRECTION_LEFT)
     {
        if (cov->flick_gesture.n_fingers == 1)
          type = ONE_FINGER_FLICK_LEFT;
        if (cov->flick_gesture.n_fingers == 2)
          type = TWO_FINGERS_FLICK_LEFT;
        if (cov->flick_gesture.n_fingers == 3)
          type = THREE_FINGERS_FLICK_LEFT;
     }
   else if (cov->flick_gesture.dir == FLICK_DIRECTION_RIGHT)
     {
        if (cov->flick_gesture.n_fingers == 1)
          type = ONE_FINGER_FLICK_RIGHT;
        if (cov->flick_gesture.n_fingers == 2)
          type = TWO_FINGERS_FLICK_RIGHT;
        if (cov->flick_gesture.n_fingers == 3)
          type = THREE_FINGERS_FLICK_RIGHT;
     }
   else if (cov->flick_gesture.dir == FLICK_DIRECTION_UP)
     {
        if (cov->flick_gesture.n_fingers == 1)
          type = ONE_FINGER_FLICK_UP;
        if (cov->flick_gesture.n_fingers == 2)
          type = TWO_FINGERS_FLICK_UP;
        if (cov->flick_gesture.n_fingers == 3)
          type = THREE_FINGERS_FLICK_UP;
     }
   else if (cov->flick_gesture.dir == FLICK_DIRECTION_DOWN)
     {
        if (cov->flick_gesture.n_fingers == 1)
          type = ONE_FINGER_FLICK_DOWN;
        if (cov->flick_gesture.n_fingers == 2)
          type = TWO_FINGERS_FLICK_DOWN;
        if (cov->flick_gesture.n_fingers == 3)
          type = THREE_FINGERS_FLICK_DOWN;
     }
   else if (cov->flick_gesture.dir == FLICK_DIRECTION_DOWN_RETURN)
     {
        if (cov->flick_gesture.n_fingers == 1)
          type = ONE_FINGER_FLICK_DOWN_RETURN;
        if (cov->flick_gesture.n_fingers == 2)
          type = TWO_FINGERS_FLICK_DOWN_RETURN;
        if (cov->flick_gesture.n_fingers == 3)
          type = THREE_FINGERS_FLICK_DOWN_RETURN;
     }
   else if (cov->flick_gesture.dir == FLICK_DIRECTION_UP_RETURN)
     {
        if (cov->flick_gesture.n_fingers == 1)
          type = ONE_FINGER_FLICK_UP_RETURN;
        if (cov->flick_gesture.n_fingers == 2)
          type = TWO_FINGERS_FLICK_UP_RETURN;
        if (cov->flick_gesture.n_fingers == 3)
          type = THREE_FINGERS_FLICK_UP_RETURN;
     }
   else if (cov->flick_gesture.dir == FLICK_DIRECTION_LEFT_RETURN)
     {
        if (cov->flick_gesture.n_fingers == 1)
          type = ONE_FINGER_FLICK_LEFT_RETURN;
        if (cov->flick_gesture.n_fingers == 2)
          type = TWO_FINGERS_FLICK_LEFT_RETURN;
        if (cov->flick_gesture.n_fingers == 3)
          type = THREE_FINGERS_FLICK_LEFT_RETURN;
     }
   else if (cov->flick_gesture.dir == FLICK_DIRECTION_RIGHT_RETURN)
     {
        if (cov->flick_gesture.n_fingers == 1)
          type = ONE_FINGER_FLICK_RIGHT_RETURN;
        if (cov->flick_gesture.n_fingers == 2)
          type = TWO_FINGERS_FLICK_RIGHT_RETURN;
        if (cov->flick_gesture.n_fingers == 3)
          type = THREE_FINGERS_FLICK_RIGHT_RETURN;
     }
   _event_emit(type, ax, ay, axe, aye, GESTURE_FINISHED, cov->event_time, cov->angle);
}

static void
_flick_gesture_mouse_up(Ecore_Event_Mouse_Button *ev, Cover *cov)
{
   if (cov->flick_gesture.state == GESTURE_ONGOING)
     {
        int i;
        // check if fingers match
        for (i = 0; i < cov->flick_gesture.n_fingers; i++)
          {
             if (cov->flick_gesture.finger[i] == ev->multi.device)
               break;
          }
        if (i == cov->flick_gesture.n_fingers)
          {
             DEBUG("Finger id not recognized. Gesture aborted.");
             cov->flick_gesture.state = GESTURE_ABORTED;
             goto end;
          }

        if (cov->flick_gesture.flick_to_scroll)
          {
             if (ev->multi.device == 1)
               {
                  cov->flick_gesture.flick_to_scroll_last_x = ev->x;
                  cov->flick_gesture.flick_to_scroll_last_y = ev->y;
               }
             cov->flick_gesture.state = GESTURE_ABORTED;
             goto end;
          }

        // check if flick for given finger is valid
        if (!_flick_gesture_time_check(ev->timestamp,
                                       cov->flick_gesture.timestamp[i]))
          {
             DEBUG("finger flick gesture timeout expired. Gesture aborted.");
             cov->flick_gesture.state = GESTURE_ABORTED;
             goto end;
          }

        // check minimal flick length
        if (!_flick_gesture_length_check(ev->root.x, ev->root.y,
                                        cov->flick_gesture.x_org[i],
                                        cov->flick_gesture.y_org[i]))
          {
             if (!cov->flick_gesture.finger_out[i])
               {
                  DEBUG("Minimal gesture length not reached and no return flick. Gesture aborted.");
                  cov->flick_gesture.state = GESTURE_ABORTED;
                  goto end;
               }
             cov->flick_gesture.return_flick[i] = EINA_TRUE;
          }

        flick_direction_e s = cov->flick_gesture.return_flick[i] ?
                                       cov->flick_gesture.dir :
                                       _flick_gesture_direction_get(ev->root.x, ev->root.y,
                                                                     cov->flick_gesture.x_org[i],
                                                                     cov->flick_gesture.y_org[i]);

        cov->flick_gesture.n_fingers_left--;

        if ((cov->flick_gesture.dir == FLICK_DIRECTION_UNDEFINED ||
               cov->flick_gesture.dir > FLICK_DIRECTION_RIGHT)
               && cov->flick_gesture.return_flick[i] == EINA_FALSE)
         {
            DEBUG("Flick gesture");
            cov->flick_gesture.dir = s;
         }

        // gesture is valid only if all flicks are in same direction
        if (cov->flick_gesture.dir != s)
          {
             DEBUG("Flick in different direction. Gesture aborted.");
             cov->flick_gesture.state = GESTURE_ABORTED;
             goto end;
          }

        cov->flick_gesture.x_end[i] = ev->root.x;
        cov->flick_gesture.y_end[i] = ev->root.y;

        if (!cov->flick_gesture.n_fingers_left)
          {
             _flick_event_emit(cov);
             cov->flick_gesture.state = GESTURE_NOT_STARTED;
          }
     }

end:
   // if no finger is touching a screen, gesture will be reseted.
   if ((cov->flick_gesture.state == GESTURE_ABORTED))
     {
        if (cov->flick_gesture.flick_to_scroll)
          {
             Ecore_Event_Mouse_Button *event1, *event2;
             if (!(event1 = malloc(sizeof(Ecore_Event_Mouse_Button))))
               {
                  DBG("NOT ENOUGH MEMORY");
                  return ;
               }
             memcpy(event1, ev, sizeof(Ecore_Event_Mouse_Button));
             if (!(event2 = malloc(sizeof(Ecore_Event_Mouse_Button))))
               {
                  DBG("NOT ENOUGH MEMORY");
                  return ;
               }
             memcpy(event2, ev, sizeof(Ecore_Event_Mouse_Button));
             event1->x = cov->flick_gesture.flick_to_scroll_last_x;
             event1->y = cov->flick_gesture.flick_to_scroll_last_y;
             event1->multi.device = 0;
             event1->multi.radius += MAGIC_NUMBER;
             event2->multi.radius += MAGIC_NUMBER;
             event2->multi.device = 1;
             cov->flick_gesture.flick_to_scroll = EINA_FALSE;
             ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_UP, event1, NULL, NULL);
             ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_UP, event2, NULL, NULL);
          }
        DEBUG("Restet flick gesture");
        if (cov->n_taps == 0)
           cov->flick_gesture.state = GESTURE_NOT_STARTED;
     }
}

static Eina_Bool _flick_to_scroll_gesture_conditions_met(Ecore_Event_Mouse_Move * ev, int gesture_timestamp, int dx, int dy)
{
   if (ev->timestamp - gesture_timestamp > _e_mod_config->two_finger_flick_to_scroll_timeout)
      if (abs(dx) > _e_mod_config->two_finger_flick_to_scroll_min_length || abs(dy) > _e_mod_config->two_finger_flick_to_scroll_min_length)
         return EINA_TRUE;

   return EINA_FALSE;
}

static void
start_scroll(int x, int y, Cover *cov)
{
   Ecore_Event_Mouse_Button *ev_down;
   if (!(ev_down = malloc(sizeof(Ecore_Event_Mouse_Button))))
     {
        DBG("NOT ENOUGH MEMORY");
        return ;
     }
   memcpy(ev_down, cov->flick_gesture.ev_first_down, sizeof(Ecore_Event_Mouse_Button));
   cov->flick_gesture.ev_first_down->x = x;
   cov->flick_gesture.ev_first_down->y = y;
   _emit_mouse_move_event(cov->flick_gesture.ev_first_down);
   cov->flick_gesture.ev_first_down->timestamp = (int)(ecore_time_get() * 1000);
   cov->flick_gesture.ev_first_down->multi.radius += MAGIC_NUMBER;
   ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_DOWN, cov->flick_gesture.ev_first_down, NULL, NULL);
   _emit_mouse_move_event(ev_down);
   ev_down->multi.device = 1;
   ev_down->multi.radius += MAGIC_NUMBER;
   ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_DOWN, ev_down, NULL, NULL);
}

static void
_flick_gesture_mouse_move(Ecore_Event_Mouse_Move *ev, Cover *cov)
{
   if (cov->flick_gesture.state == GESTURE_ONGOING)
      {
         int i;
         for(i = 0; i < cov->flick_gesture.n_fingers; ++i)
            {
               if (cov->flick_gesture.finger[i] == ev->multi.device)
               break;
            }
         if (i == cov->flick_gesture.n_fingers)
          {
             if (cov->flick_gesture.n_fingers >= 3) //that is because of the EFL bug. Mouse move event before mouse down(!)
               {
                  ERROR("Finger id not recognized. Gesture aborted.");
                  cov->flick_gesture.state = GESTURE_ABORTED;
                  return;
               }
          }
         int dxx = ev->root.x - cov->flick_gesture.x_org[i];
         int dyy = ev->root.y - cov->flick_gesture.y_org[i];
         if (i == 1) {
            if (cov->flick_gesture.flick_to_scroll || _flick_to_scroll_gesture_conditions_met(ev, cov->flick_gesture.timestamp[i], dxx, dyy)) {
               if (!cov->flick_gesture.flick_to_scroll) {
                  start_scroll(ev->x, ev->y, cov);
                  cov->flick_gesture.flick_to_scroll = EINA_TRUE;
               }
               return;
            }
         }
         if(!cov->flick_gesture.finger_out[i])
            {
               int dx = ev->root.x - cov->flick_gesture.x_org[i];
               int dy = ev->root.y - cov->flick_gesture.y_org[i];

               if (dx < 0) dx *= -1;
              if (dy < 0) dy *= -1;

              if (dx > _e_mod_config->one_finger_flick_min_length)
                  {
                     cov->flick_gesture.finger_out[i] = EINA_TRUE;
                     if (ev->root.x > cov->flick_gesture.x_org[i])
                        {
                           if (cov->flick_gesture.dir == FLICK_DIRECTION_UNDEFINED ||
                                 cov->flick_gesture.dir == FLICK_DIRECTION_RIGHT_RETURN)
                              {
                                 cov->flick_gesture.dir = FLICK_DIRECTION_RIGHT_RETURN;
                              }
                           else
                              {
                                 ERROR("Invalid direction, abort");
                                 cov->flick_gesture.state = GESTURE_ABORTED;
                              }
                        }
                     else
                        {
                           if (cov->flick_gesture.dir == FLICK_DIRECTION_UNDEFINED ||
                                 cov->flick_gesture.dir == FLICK_DIRECTION_LEFT_RETURN)
                              {
                                 cov->flick_gesture.dir = FLICK_DIRECTION_LEFT_RETURN;
                              }
                           else
                              {
                                 ERROR("Invalid direction, abort");
                                 cov->flick_gesture.state = GESTURE_ABORTED;
                              }
                        }
                     return;
                  }

               else if (dy > _e_mod_config->one_finger_flick_min_length)
                  {
                     cov->flick_gesture.finger_out[i] = EINA_TRUE;
                     if (ev->root.y > cov->flick_gesture.y_org[i])
                        {
                           if (cov->flick_gesture.dir == FLICK_DIRECTION_UNDEFINED ||
                                 cov->flick_gesture.dir == FLICK_DIRECTION_DOWN_RETURN)
                              {
                                 cov->flick_gesture.dir = FLICK_DIRECTION_DOWN_RETURN;
                              }
                           else
                              {
                                 ERROR("Invalid direction, abort");
                                 cov->flick_gesture.state = GESTURE_ABORTED;
                              }
                        }
                     else
                        {
                           if (cov->flick_gesture.dir == FLICK_DIRECTION_UNDEFINED ||
                                 cov->flick_gesture.dir == FLICK_DIRECTION_UP_RETURN)
                              {
                                 cov->flick_gesture.dir = FLICK_DIRECTION_UP_RETURN;
                              }
                           else
                              {
                                 ERROR("Invalid direction, abort");
                                 cov->flick_gesture.state = GESTURE_ABORTED;
                              }
                        }
                     return;
                  }
            }
      }
   return;
}

static Eina_Bool
_on_hover_timeout(void *data)
{
   Cover *cov = data;
   DEBUG("Hover timer expierd");

   cov->hover_gesture.longpressed = EINA_TRUE;
   cov->hover_gesture.timer = NULL;

   if (cov->hover_gesture.last_emission_time == -1)
     {
        _hover_event_emit(cov, GESTURE_NOT_STARTED);
        cov->hover_gesture.last_emission_time = cov->event_time;
     }
   return EINA_FALSE;
}

static void
_hover_gesture_timer_reset(Cover *cov, double time)
{
   DEBUG("Hover timer reset");
   cov->hover_gesture.longpressed = EINA_FALSE;
   if (cov->hover_gesture.timer)
     {
        ecore_timer_reset(cov->hover_gesture.timer);
        return;
     }
   cov->hover_gesture.timer = ecore_timer_add(time, _on_hover_timeout, cov);
}

static void
_hover_gesture_mouse_down(Ecore_Event_Mouse_Button *ev, Cover *cov)
{
   if (cov->hover_gesture.state == GESTURE_NOT_STARTED &&
       cov->n_taps == 1)
     {
        cov->hover_gesture.state = GESTURE_ONGOING;
        cov->hover_gesture.timestamp = ev->timestamp;
        cov->hover_gesture.last_emission_time = -1;
        cov->hover_gesture.x[0] = ev->root.x;
        cov->hover_gesture.y[0] = ev->root.y;
        cov->hover_gesture.finger[0] = ev->multi.device;
        cov->hover_gesture.n_fingers = 1;
        _hover_gesture_timer_reset(cov, _e_mod_config->one_finger_hover_longpress_timeout);
     }
   if (cov->hover_gesture.state == GESTURE_ONGOING &&
       cov->n_taps == 2)
     {
        if (cov->hover_gesture.longpressed)
          {
             _hover_event_emit(cov, GESTURE_FINISHED);
             goto abort;
          }
        cov->hover_gesture.timestamp = -1;
        cov->hover_gesture.last_emission_time = -1;
        cov->hover_gesture.x[1] = ev->root.x;
        cov->hover_gesture.y[1] = ev->root.y;
        cov->hover_gesture.finger[1] = ev->multi.device;
        cov->hover_gesture.n_fingers = 2;
        cov->hover_gesture.longpressed = EINA_TRUE;
        if (cov->hover_gesture.timer)
          ecore_timer_del(cov->hover_gesture.timer);
        cov->hover_gesture.timer = NULL;
        _hover_event_emit(cov, GESTURE_NOT_STARTED);
     }
   // abort gesture if more then 2 fingers touched screen
   if ((cov->hover_gesture.state == GESTURE_ONGOING) &&
       cov->n_taps > 2)
     {
        DEBUG("More then 2 finged. Abort hover gesture");
        _hover_event_emit(cov, GESTURE_FINISHED);
        goto abort;
     }
   return;

abort:
   cov->hover_gesture.state = GESTURE_ABORTED;
   if (cov->hover_gesture.timer)
     ecore_timer_del(cov->hover_gesture.timer);
   cov->hover_gesture.timer = NULL;
}

static void
_hover_gesture_mouse_up(Ecore_Event_Mouse_Button *ev, Cover *cov)
{
   int i;
   if (cov->hover_gesture.state == GESTURE_ONGOING)
     {

        for (i = 0; i < cov->hover_gesture.n_fingers; i++)
          {
             if (cov->hover_gesture.finger[i] == ev->multi.device)
              break;
          }
        if (i == cov->hover_gesture.n_fingers)
          {
             DEBUG("Invalid finger id: %d", ev->multi.device);
             return;
          }
        else
          {
             cov->hover_gesture.state = GESTURE_ABORTED;
             if (cov->hover_gesture.timer)
               ecore_timer_del(cov->hover_gesture.timer);
             cov->hover_gesture.timer = NULL;
             // aditionally emit event to complete sequence
             if (cov->hover_gesture.longpressed)
                _hover_event_emit(cov, GESTURE_FINISHED);
          }
     }
   // reset gesture only if user released all his fingers
   if (cov->n_taps == 0)
     cov->hover_gesture.state = GESTURE_NOT_STARTED;
}

static void
_hover_event_emit(Cover *cov, gesture_state_e state)
{
   if (cov->tap_n_hold_gesture_data.double_tap && !is_slider)
     {
        _emit_mouse_move_event(cov->tap_n_hold_gesture_data.ev_down);
        cov->tap_n_hold_gesture_data.ev_down->multi.radius += MAGIC_NUMBER;
        cov->tap_n_hold_gesture_data.drag_start = EINA_TRUE;
        ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_DOWN, cov->tap_n_hold_gesture_data.ev_down, NULL, NULL);
     }

   cov->tap_n_hold_gesture_data.double_tap = EINA_FALSE;
   cov->tap_n_hold_gesture_data.n_taps = 0;

   int ax = 0, ay = 0, j;
   for (j = 0; j < cov->hover_gesture.n_fingers; j++)
     {
        ax += cov->hover_gesture.x[j];
        ay += cov->hover_gesture.y[j];
     }

   ax /= cov->hover_gesture.n_fingers;
   ay /= cov->hover_gesture.n_fingers;

   switch (cov->hover_gesture.n_fingers)
     {
      case 1:
         _event_emit(ONE_FINGER_HOVER, ax, ay, ax, ay, state, cov->event_time, cov->angle);
         break;
      case 2:
         _event_emit(TWO_FINGERS_HOVER, ax, ay, ax, ay, state, cov->event_time, cov->angle);
         break;
      default:
         break;
     }
}

static void
_hover_gesture_mouse_move(Ecore_Event_Mouse_Move *ev, Cover *cov)
{
   if (cov->hover_gesture.state == GESTURE_ONGOING)
     {
        // check fingers
        int i;
        if (!cov->hover_gesture.longpressed)
          return;

        for (i = 0; i < cov->hover_gesture.n_fingers; i++)
          {
             if (cov->hover_gesture.finger[i] == ev->multi.device)
               break;
          }
        if (i == cov->hover_gesture.n_fingers)
          {
             DEBUG("Invalid finger id: %d", ev->multi.device);
             return;
          }
          cov->hover_gesture.x[i] = ev->root.x;
          cov->hover_gesture.y[i] = ev->root.y;
          _hover_event_emit(cov, GESTURE_ONGOING);
      }
}

static void
_tap_event_emit(Cover *cov, gesture_state_e state)
{
   switch (cov->tap_gesture_data.n_taps)
     {
      case 1:
         if(cov->tap_gesture_data.tap_type == ONE_FINGER_GESTURE)
            {
               DEBUG("ONE_FINGER_SINGLE_TAP");
               _event_emit(ONE_FINGER_SINGLE_TAP,
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     state, cov->event_time, cov->angle);
            }
         else if(cov->tap_gesture_data.tap_type == TWO_FINGERS_GESTURE)
            {
               DEBUG("TWO_FINGERS_SINGLE_TAP");
               _event_emit(TWO_FINGERS_SINGLE_TAP,
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     cov->tap_gesture_data.x_org[1], cov->tap_gesture_data.y_org[1],
                     state, cov->event_time, cov->angle);
            }
         else if(cov->tap_gesture_data.tap_type == THREE_FINGERS_GESTURE)
            {
               DEBUG("THREE_FINGERS_SINGLE_TAP");
               _event_emit(THREE_FINGERS_SINGLE_TAP,
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     cov->tap_gesture_data.x_org[2], cov->tap_gesture_data.y_org[2],
                     state, cov->event_time, cov->angle);
            }
         else
            {
               ERROR("Unknown tap");
            }
         break;
      case 2:
         if(cov->tap_gesture_data.tap_type == ONE_FINGER_GESTURE)
            {
               DEBUG("ONE_FINGER_DOUBLE_TAP");
               _event_emit(ONE_FINGER_DOUBLE_TAP,
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     state, cov->event_time, cov->angle);
            }
         else if(cov->tap_gesture_data.tap_type == TWO_FINGERS_GESTURE)
            {
               DEBUG("TWO_FINGERS_DOUBLE_TAP");
               _event_emit(TWO_FINGERS_DOUBLE_TAP,
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     cov->tap_gesture_data.x_org[1], cov->tap_gesture_data.y_org[1],
                     state, cov->event_time, cov->angle);
            }
         else if(cov->tap_gesture_data.tap_type == THREE_FINGERS_GESTURE)
            {
               DEBUG("THREE_FINGERS_DOUBLE_TAP");
               _event_emit(THREE_FINGERS_DOUBLE_TAP,
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     cov->tap_gesture_data.x_org[2], cov->tap_gesture_data.y_org[2],
                     state, cov->event_time, cov->angle);
            }
         else
            {
               ERROR("Unknown tap");
            }
         break;
      case 3:
         if(cov->tap_gesture_data.tap_type == ONE_FINGER_GESTURE)
            {
               DEBUG("ONE_FINGER_TRIPLE_TAP");
               _event_emit(ONE_FINGER_TRIPLE_TAP,
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     state, cov->event_time, cov->angle);
            }
         else if(cov->tap_gesture_data.tap_type == TWO_FINGERS_GESTURE)
            {
               DEBUG("TWO_FINGERS_TRIPLE_TAP");
               _event_emit(TWO_FINGERS_TRIPLE_TAP,
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     cov->tap_gesture_data.x_org[1], cov->tap_gesture_data.y_org[1],
                     state, cov->event_time, cov->angle);
            }
         else if(cov->tap_gesture_data.tap_type == THREE_FINGERS_GESTURE)
            {
               DEBUG("THREE_FINGERS_TRIPLE_TAP");
               _event_emit(THREE_FINGERS_TRIPLE_TAP,
                     cov->tap_gesture_data.x_org[0], cov->tap_gesture_data.y_org[0],
                     cov->tap_gesture_data.x_org[2], cov->tap_gesture_data.y_org[2],
                     state, cov->event_time, cov->angle);
            }
         else
            {
               ERROR("Unknown tap");
            }
         break;
      default:
         ERROR("Unknown tap");
         break;
     }
}

static Eina_Bool
_on_tap_timer_expire(void *data)
{
   Cover *cov = data;
   DEBUG("Timer expired");

   if (cov->tap_n_hold_gesture_data.n_taps == 2 && cov->tap_gesture_data.tap_type == ONE_FINGER_GESTURE)
      cov->tap_n_hold_gesture_data.double_tap = EINA_TRUE;

   if (cov->tap_gesture_data.started && !cov->tap_gesture_data.pressed)
      _tap_event_emit(cov, GESTURE_FINISHED);
   else if (cov->tap_n_hold_gesture_data.double_tap)
      _tap_event_emit(cov, GESTURE_ABORTED);

   // finish gesture
   cov->tap_gesture_data.started = EINA_FALSE;
   cov->tap_gesture_data.timer = NULL;
   cov->tap_gesture_data.tap_type = ONE_FINGER_GESTURE;
   cov->tap_gesture_data.finger[0] = -1;
   cov->tap_gesture_data.finger[1] = -1;
   cov->tap_gesture_data.finger[2] = -1;

   return EINA_FALSE;
}

static int _tap_gesture_finger_check(Cover *cov, int x, int y)
{
   int dx = x - cov->tap_gesture_data.x_org[0];
   int dy = y - cov->tap_gesture_data.y_org[0];

   if (cov->tap_gesture_data.finger[0] != -1 &&
         (dx * dx + dy * dy < _e_mod_config->one_finger_tap_radius *
         _e_mod_config->one_finger_tap_radius))
      {
         return 0;
      }

   dx = x - cov->tap_gesture_data.x_org[1];
   dy = y - cov->tap_gesture_data.y_org[1];
   if (cov->tap_gesture_data.finger[1] != -1 &&
            (dx * dx + dy * dy < _e_mod_config->one_finger_tap_radius *
            _e_mod_config->one_finger_tap_radius))
      {
         return 1;
      }

   dx = x - cov->tap_gesture_data.x_org[2];
   dy = y - cov->tap_gesture_data.y_org[2];
   if (cov->tap_gesture_data.finger[2] != -1 &&
            (dx * dx + dy * dy < _e_mod_config->one_finger_tap_radius *
            _e_mod_config->one_finger_tap_radius))
      {
         return 2;
      }

   return -1;
}

static void
_tap_gestures_mouse_down(Ecore_Event_Mouse_Button *ev, Cover *cov)
{
   if (cov->n_taps > 4)
      {
         ERROR("Too many fingers");
         return;
      }

   cov->tap_gesture_data.pressed = EINA_TRUE;

   if (cov->tap_gesture_data.started == EINA_FALSE)
      {
         DEBUG("First finger down");
         cov->tap_gesture_data.started = EINA_TRUE;
         cov->tap_gesture_data.finger[0] = ev->multi.device;
         cov->tap_gesture_data.x_org[0] = ev->root.x;
         cov->tap_gesture_data.y_org[0] = ev->root.y;
         cov->tap_gesture_data.finger[1] = -1;
         cov->tap_gesture_data.finger[2] = -1;
         cov->tap_gesture_data.n_taps = 0;
         cov->tap_n_hold_gesture_data.n_taps = 1;
         cov->tap_n_hold_gesture_data.double_tap = EINA_FALSE;
         cov->tap_n_hold_gesture_data.drag_start = EINA_FALSE;
         if (!(cov->tap_n_hold_gesture_data.ev_down = malloc(sizeof(Ecore_Event_Mouse_Button))))
           {
              DEBUG("NOT ENOUGH MEMORY");
              return ;
           }
         memcpy (cov->tap_n_hold_gesture_data.ev_down, ev, sizeof(Ecore_Event_Mouse_Button));
         cov->tap_gesture_data.timer = ecore_timer_add(
                                           _e_mod_config->one_finger_tap_timeout,
                                           _on_tap_timer_expire, cov);
         cov->tap_gesture_data.tap_type = ONE_FINGER_GESTURE;
      }

   else
      {
         if (ev->multi.device == cov->tap_gesture_data.finger[0])
            {
               DEBUG("First finger down");

               if (_tap_gesture_finger_check(cov, ev->root.x, ev->root.y) == -1)
                  {
                     ERROR("Abort gesture");
                     cov->tap_gesture_data.started = EINA_FALSE;
                     ecore_timer_del(cov->tap_gesture_data.timer);
                     cov->tap_gesture_data.timer = NULL;
                     cov->tap_gesture_data.tap_type = ONE_FINGER_GESTURE;
                     cov->tap_gesture_data.finger[0] = -1;
                     cov->tap_gesture_data.finger[1] = -1;
                     cov->tap_gesture_data.finger[2] = -1;
                     cov->tap_n_hold_gesture_data.n_taps = 0;
                     _tap_gestures_mouse_down(ev, cov);
                     return;
                  }

               cov->tap_gesture_data.x_org[0] = ev->root.x;
               cov->tap_gesture_data.y_org[0] = ev->root.y;
               cov->tap_n_hold_gesture_data.n_taps++;
            }
         else if (cov->tap_gesture_data.finger[1] == -1 ||
                  cov->tap_gesture_data.finger[1] == ev->multi.device)
            {
               DEBUG("Second finger down");
               cov->tap_gesture_data.finger[1] = ev->multi.device;

               cov->tap_gesture_data.x_org[1] = ev->root.x;
               cov->tap_gesture_data.y_org[1] = ev->root.y;
               if (cov->tap_gesture_data.tap_type < TWO_FINGERS_GESTURE)
                  cov->tap_gesture_data.tap_type = TWO_FINGERS_GESTURE;
            }
         else if (cov->tap_gesture_data.finger[2] == -1 ||
                  cov->tap_gesture_data.finger[2] == ev->multi.device)
            {
               DEBUG("Third finger down");
               cov->tap_gesture_data.finger[2] = ev->multi.device;

               cov->tap_gesture_data.x_org[2] = ev->root.x;
               cov->tap_gesture_data.y_org[2] = ev->root.y;
               if (cov->tap_gesture_data.tap_type < THREE_FINGERS_GESTURE)
                  cov->tap_gesture_data.tap_type = THREE_FINGERS_GESTURE;
            }
         else
            {
               ERROR("Unknown finger down");
            }
         ecore_timer_reset(cov->tap_gesture_data.timer);
      }
}

static void
_tap_gestures_mouse_up(Ecore_Event_Mouse_Button *ev, Cover *cov)
{
   if (cov->tap_gesture_data.timer)
      {
         cov->tap_gesture_data.pressed = EINA_FALSE;

         if (ev->multi.device == cov->tap_gesture_data.finger[0])
            {
               DEBUG("First finger up");

               if (_tap_gesture_finger_check(cov, ev->root.x, ev->root.y) != -1)
                 {
                    if (cov->n_taps == 0)
                      {
                         cov->tap_gesture_data.n_taps++;
                      }
                 }
               else
                 {
                    ERROR("Abort gesture");
                    cov->tap_gesture_data.started = EINA_FALSE;
                 }
            }
         else if (ev->multi.device == cov->tap_gesture_data.finger[1])
            {
               DEBUG("Second finger up");

               if (_tap_gesture_finger_check(cov, ev->root.x, ev->root.y) != - 1)
                 {
                    if (cov->n_taps == 0)
                      {
                         cov->tap_gesture_data.n_taps++;
                      }
                 }
               else
                 {
                    ERROR("Abort gesture");
                    cov->tap_gesture_data.started = EINA_FALSE;
                 }
            }
         else if (ev->multi.device == cov->tap_gesture_data.finger[2])
            {
               DEBUG("Third finger up");

               int dx = ev->root.x - cov->tap_gesture_data.x_org[2];
               int dy = ev->root.y - cov->tap_gesture_data.y_org[2];

               if((dx * dx + dy * dy) < _e_mod_config->one_finger_tap_radius *
                     _e_mod_config->one_finger_tap_radius)
                  {
                     if (cov->n_taps == 0)
                        {
                           cov->tap_gesture_data.n_taps++;
                        }
                  }
               else
                  {
                     ERROR("Abort gesture");
                     cov->tap_gesture_data.started = EINA_FALSE;
                  }
            }
         else
            {
               ERROR("Unknown finger up, abort gesture");
               cov->tap_gesture_data.started = EINA_FALSE;
            }
      }
}

static void
_tap_gestures_move(Ecore_Event_Mouse_Move *ev, Cover *cov)
{
   int i;
   for (i = 0; i < sizeof(cov->tap_gesture_data.finger) / sizeof(cov->tap_gesture_data.finger[0]); i++)
     {
        if (ev->multi.device == cov->tap_gesture_data.finger[i])
          {
             int dx = ev->root.x - cov->tap_gesture_data.x_org[i];
             int dy = ev->root.y - cov->tap_gesture_data.y_org[i];
             if ((dx * dx + dy * dy) > _e_mod_config->one_finger_tap_radius * _e_mod_config->one_finger_tap_radius)
               {
                  ERROR("Abort gesture");
                  cov->tap_gesture_data.started = EINA_FALSE;
                  ecore_timer_del(cov->tap_gesture_data.timer);
                  cov->tap_gesture_data.timer = NULL;
                  cov->tap_gesture_data.tap_type = ONE_FINGER_GESTURE;
                  cov->tap_gesture_data.finger[0] = -1;
                  cov->tap_gesture_data.finger[1] = -1;
                  cov->tap_gesture_data.finger[2] = -1;
                  return;
               }
          }
     }
}

static Eina_Bool
_mouse_move(int type, Ecore_Event_Mouse_Move *event)
{
   Ecore_Event_Mouse_Move *ev = event;
   if (ev->multi.radius >= MAGIC_NUMBER || cover->flick_gesture.flick_to_scroll || cover->tap_n_hold_gesture_data.drag_start)
     {
        if (ev->multi.radius >= MAGIC_NUMBER) ev->multi.radius -= MAGIC_NUMBER;
        return EINA_TRUE;
     }

   cover->event_time = ev->timestamp;
   _flick_gesture_mouse_move(ev, cover);
   _hover_gesture_mouse_move(ev, cover);
   _tap_gestures_move(ev, cover);
   return EINA_FALSE;
}

static Eina_Bool
_mouse_button_up(int type, Ecore_Event_Mouse_Button *event)
{
   Ecore_Event_Mouse_Button *ev = event;

   if (event->multi.radius >= MAGIC_NUMBER)
     {
        event->multi.radius -= MAGIC_NUMBER;
        return EINA_TRUE;
     }
   if (cover->tap_n_hold_gesture_data.drag_start)
     {
        Ecore_Event_Mouse_Button *ev_up;
        if (!(ev_up = malloc(sizeof(Ecore_Event_Mouse_Button))))
          {
             DEBUG("NOT ENOUGH MEMORY");
             return EINA_FALSE;
          }
        memcpy(ev_up, ev, sizeof(Ecore_Event_Mouse_Button));
        ev_up->multi.radius += MAGIC_NUMBER;
        ecore_event_add(ECORE_EVENT_MOUSE_BUTTON_UP, ev_up, NULL, NULL);
        cover->tap_n_hold_gesture_data.drag_start = EINA_FALSE;
     }
   cover->n_taps--;
   cover->event_time = ev->timestamp;

   _flick_gesture_mouse_up(ev, cover);
   _hover_gesture_mouse_up(ev, cover);
   _tap_gestures_mouse_up(ev, cover);
   DEBUG("single mouse up,taps: %d Multi :%d", cover->n_taps,ev->multi.device);

   return EINA_FALSE;
}

static Eina_Bool
_mouse_button_down(int type, Ecore_Event_Mouse_Button *event)
{
   Ecore_Event_Mouse_Button *ev = event;

   if (event->multi.radius >= MAGIC_NUMBER)
     {
        ev->multi.radius -= MAGIC_NUMBER;
        return EINA_TRUE;
     }

   cover->n_taps++;
   cover->event_time = ev->timestamp;

   DEBUG("single mouse down: taps: %d Multi :%d", cover->n_taps,ev->multi.device);
   _flick_gesture_mouse_down(ev, cover);
   _hover_gesture_mouse_down(ev, cover);
   _tap_gestures_mouse_down(ev, cover);
   return EINA_FALSE;
}

static Eina_Bool
_event_filter(void *data, void *loop_data, int type, void *event)
{
   if (type == ECORE_EVENT_MOUSE_BUTTON_DOWN)
     {
        return _mouse_button_down(type, event);
     }
   else if (type == ECORE_EVENT_MOUSE_BUTTON_UP)
     {
        return _mouse_button_up(type, event);
     }
   else if (type == ECORE_EVENT_MOUSE_MOVE)
     {
        return _mouse_move(type, event);
     }

   return EINA_TRUE;
}

static void
_events_init(void)
{
   ef = ecore_event_filter_add(NULL, _event_filter, NULL, NULL);
   if (!E_EVENT_ATSPI_GESTURE_DETECTED)
      E_EVENT_ATSPI_GESTURE_DETECTED = ecore_event_type_new();
   eh = ecore_event_handler_add(E_EVENT_CLIENT_ROTATION_CHANGE_END, _rotation_cb_change_end, (void*)cover);
}

static void
_events_shutdown(void)
{
   ecore_event_filter_del(ef);
   ecore_event_handler_del(eh);
}

static void
_gesture_init()
{
   cover = E_NEW(Cover, 1);
   if (!cover)
     {
        ERROR("Fatal Memory error!");
        return;
     }
    is_slider = EINA_FALSE;
}

static void
_gesture_shutdown(void)
{
   if (cover->tap_gesture_data.timer)
     ecore_timer_del(cover->tap_gesture_data.timer);
   if (cover->hover_gesture.timer)
     ecore_timer_del(cover->hover_gesture.timer);
   free(cover);
}

int _e_mod_atspi_gestures_init(void)
{
   DEBUG("gesture init");
   _gesture_init();
   _events_init();
   ecore_wl_init(NULL);

   return 0;
}

int _e_mod_atspi_gestures_shutdown(void)
{
   DEBUG("gesture shutdown");

   _events_shutdown();
   _gesture_shutdown();
   ecore_wl_shutdown();

   return 0;
}
