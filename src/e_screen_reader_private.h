#ifndef E_SCREEN_READER_PRIVATE_H_
#define E_SCREEN_READER_PRIVATE_H_

#include "e.h"
#include "e_screen_reader_config.h"
#include "e_comp.h"
#include <Ecore_Wayland.h>

extern int _eina_log_dom;

#define INFO(...) EINA_LOG_DOM_INFO(_eina_log_dom, __VA_ARGS__);
#define DEBUG(...) EINA_LOG_DOM_DBG(_eina_log_dom, __VA_ARGS__);
#define ERROR(...) EINA_LOG_DOM_ERR(_eina_log_dom, __VA_ARGS__);

/**
 * @brief Accessibility gestures
 */
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

typedef struct {
     Gesture type;         // Type of recognized gesture
     int x_beg, x_end;     // (x,y) coordinates when gesture begin
     int y_beg, y_end;     // (x,y) coordinates when gesture ends
     int state;            // 0 - begin, 1 - ongoing, 2 - ended
     unsigned int event_time; //time stamp for the event
} Gesture_Info;

int _e_mod_log_init(void);
void _e_mod_log_shutdown(void);

extern int E_EVENT_ATSPI_GESTURE_DETECTED;

int _e_mod_atspi_gestures_init(void);
int _e_mod_atspi_gestures_shutdown(void);


#endif /* E_SCREEN_READER_PRIVATE_H_ */
