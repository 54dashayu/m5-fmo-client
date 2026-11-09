/**
 * @file lv_port_indev_templ.c
 *
 */

/*Copy this file as "lv_port_indev.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "driver/gpio.h"
#include "lvgl/lvgl.h"

#if LV_USE_KEYBOARD
#include "lvgl/src/extra/widgets/keyboard/lv_keyboard.h"
#endif

#if LV_USE_SLIDER
#include "lvgl/src/widgets/lv_slider.h"
#endif

#if !defined(CONFIG_LV_TOUCH_CONTROLLER_NONE)
#include "touch_driver.h"
#endif

/*********************
 *      DEFINES
 *********************/
#if defined(CONFIG_LV_PREDEFINED_DISPLAY_M5STACK)
#define LV_PORT_USE_M5STACK_BUTTONS     1
#else
#define LV_PORT_USE_M5STACK_BUTTONS     0
#endif

#define LV_PORT_M5_BUTTON_A_GPIO        GPIO_NUM_39
#define LV_PORT_M5_BUTTON_B_GPIO        GPIO_NUM_38
#define LV_PORT_M5_BUTTON_C_GPIO        GPIO_NUM_37
#define LV_PORT_M5_BUTTON_ACTIVE_LEVEL  0

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static bool touchpad_is_pressed(void);
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y);

static void mouse_init(void);
static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static bool mouse_is_pressed(void);
static void mouse_get_xy(lv_coord_t * x, lv_coord_t * y);

static void keypad_init(void);
static void keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static bool keypad_is_keyboard_focused(void);
static bool keypad_is_slider_editing(void);
static uint32_t keypad_get_key(void);

static void encoder_init(void);
static void encoder_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static void encoder_handler(void);

static void button_init(void);
static void button_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static int8_t button_get_pressed_id(void);
static bool button_is_pressed(uint8_t id);

/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t * indev_touchpad;
lv_indev_t * indev_mouse;
lv_indev_t * indev_keypad;
lv_indev_t * indev_encoder;
lv_indev_t * indev_button;

static int32_t encoder_diff;
static lv_indev_state_t encoder_state;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
    /**
     * Here you will find example implementation of input devices supported by LittelvGL:
     *  - Touchpad
     *  - Mouse (with cursor support)
     *  - Keypad (supports GUI usage only with key)
     *  - Encoder (supports GUI usage only with: left, right, push)
     *  - Button (external buttons to press points on the screen)
     *
     *  The `..._read()` function are only examples.
     *  You should shape them according to your hardware
     */

    static lv_indev_drv_t indev_drv;

#if !defined(CONFIG_LV_TOUCH_CONTROLLER_NONE)
    /*------------------
     * Touchpad
     * -----------------*/

    /*Initialize your touchpad if you have*/
    //touchpad_init();

    /*Register a touchpad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_driver_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
#endif

#if LV_PORT_USE_M5STACK_BUTTONS
    keypad_init();

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keypad_read;
    indev_keypad = lv_indev_drv_register(&indev_drv);

    lv_group_t *default_group = lv_group_create();
    lv_group_set_default(default_group);
    lv_indev_set_group(indev_keypad, default_group);
#endif

#if 0
    /*------------------
     * Mouse
     * -----------------*/

    /*Initialize your mouse if you have*/
    mouse_init();

    /*Register a mouse input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read;
    indev_mouse = lv_indev_drv_register(&indev_drv);

    /*Set cursor. For simplicity set a HOME symbol now.*/
    lv_obj_t * mouse_cursor = lv_img_create(lv_scr_act());
    lv_img_set_src(mouse_cursor, LV_SYMBOL_HOME);
    lv_indev_set_cursor(indev_mouse, mouse_cursor);

    /*------------------
     * Keypad
     * -----------------*/

    /*Initialize your keypad or keyboard if you have*/
    keypad_init();

    /*Register a keypad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keypad_read;
    indev_keypad = lv_indev_drv_register(&indev_drv);

    /*Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
     *add objects to the group with `lv_group_add_obj(group, obj)`
     *and assign this input device to group to navigate in it:
     *`lv_indev_set_group(indev_keypad, group);`*/

    /*------------------
     * Encoder
     * -----------------*/

    /*Initialize your encoder if you have*/
    encoder_init();

    /*Register a encoder input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = encoder_read;
    indev_encoder = lv_indev_drv_register(&indev_drv);

    /*Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
     *add objects to the group with `lv_group_add_obj(group, obj)`
     *and assign this input device to group to navigate in it:
     *`lv_indev_set_group(indev_encoder, group);`*/

    /*------------------
     * Button
     * -----------------*/

    /*Initialize your button if you have*/
    button_init();

    /*Register a button input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_BUTTON;
    indev_drv.read_cb = button_read;
    indev_button = lv_indev_drv_register(&indev_drv);

    /*Assign buttons to points on the screen*/
    static const lv_point_t btn_points[2] = {
        {10, 10},   /*Button 0 -> x:10; y:10*/
        {40, 100},  /*Button 1 -> x:40; y:100*/
    };
    lv_indev_set_button_points(indev_button, btn_points);
    #endif
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

__attribute__((weak)) bool lv_port_m5_key_intercept(uint32_t raw_key)
{
    (void)raw_key;
    return false;
}

/*------------------
 * Touchpad
 * -----------------*/

/*Initialize your touchpad*/
static void touchpad_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    /*Save the pressed coordinates and the state*/
    if(touchpad_is_pressed()) {
        touchpad_get_xy(&last_x, &last_y);
        data->state = LV_INDEV_STATE_PR;
    }
    else {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Set the last pressed coordinates*/
    data->point.x = last_x;
    data->point.y = last_y;
}

/*Return true is the touchpad is pressed*/
static bool touchpad_is_pressed(void)
{
    /*Your code comes here*/

    return false;
}

/*Get the x and y coordinates if the touchpad is pressed*/
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y)
{
    /*Your code comes here*/

    (*x) = 0;
    (*y) = 0;
}

/*------------------
 * Mouse
 * -----------------*/

/*Initialize your mouse*/
static void mouse_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the mouse*/
static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    /*Get the current x and y coordinates*/
    mouse_get_xy(&data->point.x, &data->point.y);

    /*Get whether the mouse button is pressed or released*/
    if(mouse_is_pressed()) {
        data->state = LV_INDEV_STATE_PR;
    }
    else {
        data->state = LV_INDEV_STATE_REL;
    }
}

/*Return true is the mouse button is pressed*/
static bool mouse_is_pressed(void)
{
    /*Your code comes here*/

    return false;
}

/*Get the x and y coordinates if the mouse is pressed*/
static void mouse_get_xy(lv_coord_t * x, lv_coord_t * y)
{
    /*Your code comes here*/

    (*x) = 0;
    (*y) = 0;
}

/*------------------
 * Keypad
 * -----------------*/

/*Initialize your keypad*/
static void keypad_init(void)
{
#if LV_PORT_USE_M5STACK_BUTTONS
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LV_PORT_M5_BUTTON_A_GPIO) |
                        (1ULL << LV_PORT_M5_BUTTON_B_GPIO) |
                        (1ULL << LV_PORT_M5_BUTTON_C_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&io_conf);
#endif
}

/*Will be called by the library to read the mouse*/
static void keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static uint32_t last_key = 0;
    static uint32_t last_raw_key = 0;
    static bool raw_intercept_active = false;

    /*Get whether the a key is pressed and save the pressed key*/
    uint32_t act_key = keypad_get_key();

    if(act_key != 0) {
        if (raw_intercept_active) {
            last_raw_key = act_key;
            data->state = LV_INDEV_STATE_REL;
            data->key = last_key;
            return;
        }

        if (last_raw_key == 0 && lv_port_m5_key_intercept(act_key)) {
            last_raw_key = act_key;
            raw_intercept_active = true;
            data->state = LV_INDEV_STATE_REL;
            data->key = last_key;
            return;
        }

        last_raw_key = act_key;
        data->state = LV_INDEV_STATE_PR;
        bool keyboard_focused = keypad_is_keyboard_focused();
        bool slider_editing = keypad_is_slider_editing();

        /*Translate the keys to LVGL control characters according to your key definitions*/
        switch(act_key) {
            case 1:
                act_key = (keyboard_focused || slider_editing) ?
                    LV_KEY_RIGHT :
                    LV_KEY_NEXT;
                break;
            case 2:
                act_key = (keyboard_focused || slider_editing) ?
                    LV_KEY_LEFT :
                    LV_KEY_PREV;
                break;
            case 3:
                act_key = LV_KEY_LEFT;
                break;
            case 4:
                act_key = LV_KEY_RIGHT;
                break;
            case 5:
                act_key = LV_KEY_ENTER;
                break;
        }

        last_key = act_key;
    }
    else {
        last_raw_key = 0;
        raw_intercept_active = false;
        data->state = LV_INDEV_STATE_REL;
    }

    data->key = last_key;
}

static bool keypad_is_keyboard_focused(void)
{
#if LV_PORT_USE_M5STACK_BUTTONS && LV_USE_KEYBOARD
    lv_group_t *group = lv_group_get_default();
    if (!group) {
        return false;
    }

    lv_obj_t *focused = lv_group_get_focused(group);
    return focused && lv_obj_check_type(focused, &lv_keyboard_class);
#else
    return false;
#endif
}

static bool keypad_is_slider_editing(void)
{
#if LV_PORT_USE_M5STACK_BUTTONS && LV_USE_SLIDER
    lv_group_t *group = lv_group_get_default();
    if (!group || !lv_group_get_editing(group)) {
        return false;
    }

    lv_obj_t *focused = lv_group_get_focused(group);
    return focused && lv_obj_check_type(focused, &lv_slider_class);
#else
    return false;
#endif
}

/*Get the currently being pressed key.  0 if no key is pressed*/
static uint32_t keypad_get_key(void)
{
#if LV_PORT_USE_M5STACK_BUTTONS
    if (gpio_get_level(LV_PORT_M5_BUTTON_A_GPIO) ==
        LV_PORT_M5_BUTTON_ACTIVE_LEVEL) {
        return 2;
    }

    if (gpio_get_level(LV_PORT_M5_BUTTON_B_GPIO) ==
        LV_PORT_M5_BUTTON_ACTIVE_LEVEL) {
        return 5;
    }

    if (gpio_get_level(LV_PORT_M5_BUTTON_C_GPIO) ==
        LV_PORT_M5_BUTTON_ACTIVE_LEVEL) {
        return 1;
    }
#endif

    return 0;
}

/*------------------
 * Encoder
 * -----------------*/

/*Initialize your keypad*/
static void encoder_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the encoder*/
static void encoder_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{

    data->enc_diff = encoder_diff;
    data->state = encoder_state;
}

/*Call this function in an interrupt to process encoder events (turn, press)*/
static void encoder_handler(void)
{
    /*Your code comes here*/

    encoder_diff += 0;
    encoder_state = LV_INDEV_STATE_REL;
}

/*------------------
 * Button
 * -----------------*/

/*Initialize your buttons*/
static void button_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the button*/
static void button_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{

    static uint8_t last_btn = 0;

    /*Get the pressed button's ID*/
    int8_t btn_act = button_get_pressed_id();

    if(btn_act >= 0) {
        data->state = LV_INDEV_STATE_PR;
        last_btn = btn_act;
    }
    else {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Save the last pressed button's ID*/
    data->btn_id = last_btn;
}

/*Get ID  (0, 1, 2 ..) of the pressed button*/
static int8_t button_get_pressed_id(void)
{
    uint8_t i;

    /*Check to buttons see which is being pressed (assume there are 2 buttons)*/
    for(i = 0; i < 2; i++) {
        /*Return the pressed button's ID*/
        if(button_is_pressed(i)) {
            return i;
        }
    }

    /*No button pressed*/
    return -1;
}

/*Test if `id` button is pressed or not*/
static bool button_is_pressed(uint8_t id)
{

    /*Your code comes here*/

    return false;
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
