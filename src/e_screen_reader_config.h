#ifndef E_SCREEN_READER_CONFIG_H_
#define E_SCREEN_READER_CONFIG_H_

#define E_ATSPI_CFG       "module.e-mod-tizen-screen-reader"

struct _Gestures_Config
{
   // minimal required length of flick gesture (in pixels)
   int one_finger_flick_min_length;
   // maximal time of gesture
   int one_finger_flick_max_time;
   // timeout period to activate hover gesture (first longpress timeout)
   double one_finger_hover_longpress_timeout;
   // tap timeout - maximal ammount of time allowed between seqiential taps
   double two_fingers_hover_longpress_timeout;
   // tap timeout - maximal ammount of time allowed between seqiential taps
   double one_finger_tap_timeout;
   // tap radius(in pixels)
   int one_finger_tap_radius;
};

typedef struct _Gestures_Config Gestures_Config;

int _e_mod_atspi_config_init(void);
int _e_mod_atspi_config_shutdown(void);
int _e_mod_atspi_config_save(void);

/*< External config handle - valid after initalization */
extern Gestures_Config *_e_mod_config;

#endif /* E_SCREEN_READER_CONFIG_H_ */
