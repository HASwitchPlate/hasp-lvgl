/**
 * @file lv_conf.h
 *
 */

#if 1 /*Set it to "1" to enable content*/

#ifndef LV_CONF_H
#define LV_CONF_H
/* clang-format off */

#include <stdint.h>

#include "lv_symbol_mdi_def.h"

#if defined(ARDUINO_ARCH_ESP8266)
#define LV_HIGH_RESOURCE_MCU  0
#endif

#ifndef LV_HIGH_RESOURCE_MCU
#define LV_HIGH_RESOURCE_MCU  1
#endif

/*====================
   Graphical settings
 *====================*/

 /* Maximal horizontal and vertical resolution to support by the library.*/
#define LV_HOR_RES_MAX          (TFT_WIDTH)
#define LV_VER_RES_MAX          (TFT_HEIGHT)
#define LV_HOR_RES          (TFT_WIDTH)
#define LV_VER_RES          (TFT_HEIGHT)

/* Color depth:
 * - 1:  1 byte per pixel
 * - 8:  RGB233
 * - 16: RGB565
 * - 32: ARGB8888
 */
#define LV_COLOR_DEPTH     16

 /* Swap the 2 bytes of RGB565 color.
  * Useful if the display has a 8 bit interface (e.g. SPI)*/
#define LV_COLOR_16_SWAP   0

  /* 1: Enable screen transparency.
   * Useful for OSD or other overlapping GUIs.
   * Requires `LV_COLOR_DEPTH = 32` colors and the screen's style should be modified: `style.body.opa = ...`*/
#define LV_COLOR_SCREEN_TRANSP    0

   /*Images pixels with this color will not be drawn (with chroma keying)*/
#define LV_COLOR_TRANSP    LV_COLOR_LIME         /*LV_COLOR_LIME: pure green*/

/* Enable chroma keying for indexed images. */
#define LV_INDEXED_CHROMA    1

/* Enable anti-aliasing (lines, and radiuses will be smoothed) */
#define LV_ANTIALIAS        1

/* Default display refresh period.
 * Can be changed in the display driver (`lv_disp_drv_t`).*/
#define LV_DISP_DEF_REFR_PERIOD      50      /*[ms]*/

 /* Dot Per Inch: used to initialize default sizes.
  * E.g. a button with width = LV_DPI / 2 -> half inch wide
  * (Not so important, you can adjust it to modify default sizes and spaces)*/
#define LV_DPI              100     /*[px]*/

  /* Type of coordinates. Should be `int16_t` (or `int32_t` for extreme cases) */
typedef int16_t lv_coord_t;

/*=========================
   Memory manager settings
 *=========================*/

 /* LittelvGL's internal memory manager's settings.
  * The graphical objects and other related data are stored here. */

//#define LV_FS_SEEK(x, y) lv_fs_seek(x, y, LV_FS_SEEK_SET)
#define LV_FS_SEEK(x, y) lv_fs_seek(x, y)
#define _lv_img_decoder_t _lv_img_decoder

#ifndef LV_MEM_CUSTOM
  /* 1: use custom malloc/free, 0: use the built-in `lv_mem_alloc` and `lv_mem_free` */
#define LV_MEM_CUSTOM      0
#endif
#if LV_MEM_CUSTOM == 0
/* Size of the memory used by `lv_mem_alloc` in bytes (>= 2kB)*/

#ifndef LV_MEM_SIZE
#if defined(ARDUINO_ARCH_ESP8266)
#  define LV_MEM_SIZE    (12 * 1024U) // Minimum 12 Kb
#elif defined(ESP32S2) && defined(BOARD_HAS_PSRAM)
#  define LV_MEM_SIZE    (48 * 1024U)  // 48Kb on ESP32-S2 with PSram
#elif defined(ESP32S2)
#  define LV_MEM_SIZE    (32 * 1024U)  // 32Kb on ESP32-S2
#elif defined(ARDUINO_ARCH_ESP32) && defined(BOARD_HAS_PSRAM)
#  define LV_MEM_SIZE    (64 * 1024U)  // 64Kb on ESP32 with PSram
#elif defined(ARDUINO_ARCH_ESP32)
#  define LV_MEM_SIZE    (48 * 1024U)  // 48Kb on ESP32
#else
#  define LV_MEM_SIZE    (256 * 1024U) // native app
#endif
#endif // LV_MEM_SIZE

/* Complier prefix for a big array declaration */
#  define LV_MEM_ATTR

/* Set an address for the memory pool instead of allocating it as an array.
 * Can be in external SRAM too. */
#  define LV_MEM_ADR          0

# define LV_MEM_ADD_JUNK      0

 /* Automatically defrag. on free. Defrag. means joining the adjacent free cells. */
#  define LV_MEM_AUTO_DEFRAG  1
#else       /*LV_MEM_CUSTOM*/
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>   /*Header for the dynamic memory function*/
#define LV_MEM_CUSTOM_ALLOC   hasp_malloc  /*Wrapper to malloc*/
#define LV_MEM_CUSTOM_FREE    hasp_free    /*Wrapper to free*/
#endif     /*LV_MEM_CUSTOM*/

#ifndef LV_VDB_SIZE
#if defined(ARDUINO_ARCH_ESP8266)
#  define LV_VDB_SIZE    (8 * 1024U)   // Minimum 8 Kb
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
#  define LV_VDB_SIZE    (16 * 1024U)  // 16kB draw buffer
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#  define LV_VDB_SIZE    (48 * 1024U)  // 16kB draw buffer
#elif defined(ARDUINO_ARCH_ESP32)
#  define LV_VDB_SIZE    (32 * 1024U)  // 32kB draw buffer
#else
#  define LV_VDB_SIZE    (128 * 1024U) // native app
#endif
#endif // LV_VDB_SIZE

/* Garbage Collector settings
 * Used if lvgl is binded to higher level language and the memory is managed by that language */
#define LV_ENABLE_GC 0
#if LV_ENABLE_GC != 0
#  define LV_GC_INCLUDE "gc.h"                           /*Include Garbage Collector related things*/
#  define LV_MEM_CUSTOM_REALLOC   your_realloc           /*Wrapper to realloc*/
#  define LV_MEM_CUSTOM_GET_SIZE  your_mem_get_size      /*Wrapper to lv_mem_get_size*/
#endif /* LV_ENABLE_GC */

 /*=======================
    Input device settings
  *=======================*/

  /* Input device default settings.
   * Can be changed in the Input device driver (`lv_indev_drv_t`)*/

   /* Input device read period in milliseconds */
#ifndef LV_INDEV_DEF_READ_PERIOD
#define LV_INDEV_DEF_READ_PERIOD          20      /*[ms]*/
#endif

/* Drag threshold in pixels */
#define LV_INDEV_DEF_DRAG_LIMIT           10

/* Drag throw slow-down in [%]. Greater value -> faster slow-down */
#define LV_INDEV_DEF_DRAG_THROW           20

/* Long press time in milliseconds.
 * Time to send `LV_EVENT_LONG_PRESSSED`) */
#define LV_INDEV_DEF_LONG_PRESS_TIME      400

 /* Repeated trigger period in long press [ms]
  * Time between `LV_EVENT_LONG_PRESSED_REPEAT */
#define LV_INDEV_DEF_LONG_PRESS_REP_TIME  200


  /* Gesture threshold in pixels */
#define LV_INDEV_DEF_GESTURE_LIMIT        50

/* Gesture min velocity at release before swipe (pixels)*/
#define LV_INDEV_DEF_GESTURE_MIN_VELOCITY 3

/*==================
 * Feature usage
 *==================*/

 /*1: Enable the Animations */
#define LV_USE_ANIMATION        1 // Needed for scroll mode
#if LV_USE_ANIMATION

/*Declare the type of the user data of animations (can be e.g. `void *`, `int`, `struct`)*/
typedef void* lv_anim_user_data_t;

#endif

/* 1: Enable shadow drawing*/
#define LV_USE_SHADOW           (LV_HIGH_RESOURCE_MCU)

/* 1: Use other blend modes than normal (`LV_BLEND_MODE_...`)*/
#define LV_USE_BLEND_MODES      0

/* 1: Use the `opa_scale` style property to set the opacity of an object and its children at once*/
#define LV_USE_OPA_SCALE        1

/* 1: Enable object groups (for keyboard/encoder navigation) */
#define LV_USE_GROUP            0
#if LV_USE_GROUP
typedef void* lv_group_user_data_t;
#endif  /*LV_USE_GROUP*/

/* 1: Enable GPU interface*/
#define LV_USE_GPU              0

/* 1: Enable file system (might be required for images */
#define LV_USE_FILESYSTEM       1
#if LV_USE_FILESYSTEM
/*Declare the type of the user data of file system drivers (can be e.g. `void *`, `int`, `struct`)*/
typedef const char* lv_fs_drv_user_data_t;

/*File system interface*/
#ifndef LV_USE_FS_IF
#define LV_USE_FS_IF	      !defined(STM32)
#endif

#if LV_USE_FS_IF
#  define LV_FS_IF_FATFS    '\0'
#if defined(STM32F4xx) // || defined(ARDUINO_ARCH_ESP8266)
#  define LV_FS_IF_PC       '\0'
//#  define LV_FS_IF_SPIFFS   '\0'  // internal esp Flash
#else
#  define LV_FS_IF_PC       'L'   // Local filesystem
#  define LV_FS_IF_POSIX    '\0'
//#  define LV_FS_IF_SPIFFS   '\0'  // no internal esp Flash
#endif
#endif  /*LV_USE_FS_IF*/
#if HASP_TARGET_ARDUINO && !defined(LV_FS_PC_PATH)
#define LV_FS_PC_PATH "/littlefs"
#endif

#endif

/*1: Add a `user_data` to drivers and objects*/
#define LV_USE_USER_DATA        1

/*========================
 * Image decoder and cache
 *========================*/

 /* 1: Enable indexed (palette) images */
#define LV_IMG_CF_INDEXED       1

/* 1: Enable alpha indexed images */
#define LV_IMG_CF_ALPHA         1

/* Default image cache size. Image caching keeps the images opened.
 * If only the built-in image formats are used there is no real advantage of caching.
 * (I.e. no new image decoder is added)
 * With complex image decoders (e.g. PNG or JPG) caching can save the continuous open/decode of images.
 * However the opened images might consume additional RAM.
 * LV_IMG_CACHE_DEF_SIZE must be >= 1 */
#ifndef LV_IMG_CACHE_DEF_SIZE
#define LV_IMG_CACHE_DEF_SIZE       1
#endif
#ifndef LV_IMG_CACHE_DEF_SIZE_PSRAM
#define LV_IMG_CACHE_DEF_SIZE_PSRAM 12    // special openHASP setting when PSRAM is used
#endif

 /*Declare the type of the user data of image decoder (can be e.g. `void *`, `int`, `struct`)*/
typedef void* lv_img_decoder_user_data_t;

/*=====================
 *  Compiler settings
 *====================*/
 /* Define a custom attribute to `lv_tick_inc` function */
#define LV_ATTRIBUTE_TICK_INC

/* Define a custom attribute to `lv_task_handler` function */
#ifndef LV_ATTRIBUTE_TASK_HANDLER
#define LV_ATTRIBUTE_TASK_HANDLER
#endif

/* With size optimization (-Os) the compiler might not align data to
 * 4 or 8 byte boundary. This alignment will be explicitly applied where needed.
 * E.g. __attribute__((aligned(4))) */
#define LV_ATTRIBUTE_MEM_ALIGN

 /* Attribute to mark large constant arrays for example
  * font's bitmaps */
#define LV_ATTRIBUTE_LARGE_CONST

  /* Export integer constant to binding.
   * This macro is used with constants in the form of LV_<CONST> that
   * should also appear on lvgl binding API such as Micropython
   *
   * The default value just prevents a GCC warning.
   */
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning

   /*===================
    *  HAL settings
    *==================*/

    /* 1: use a custom tick source.
     * It removes the need to manually update the tick with `lv_tick_inc`) */
#ifdef ARDUINO

#define LV_TICK_CUSTOM     1
#if LV_TICK_CUSTOM == 1
#define LV_TICK_CUSTOM_INCLUDE  "Arduino.h"       /*Header for the sys time function*/
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())     /*Expression evaluating to current systime in ms*/
#endif   /*LV_TICK_CUSTOM*/

#else
#define LV_TICK_CUSTOM     0
#endif

typedef void* lv_disp_drv_user_data_t;             /*Type of user data in the display driver*/
typedef void* lv_indev_drv_user_data_t;            /*Type of user data in the input device driver*/

/*================
 * Log settings
 *===============*/

#define LV_USE_PERF_MONITOR  0

 /*1: Enable the log module*/
#define LV_USE_LOG      1  // set back to 0 before release !!
#if LV_USE_LOG
/* How important log should be added:
 * LV_LOG_LEVEL_TRACE       A lot of logs to give detailed information
 * LV_LOG_LEVEL_INFO        Log important events
 * LV_LOG_LEVEL_WARN        Log if something unwanted happened but didn't cause a problem
 * LV_LOG_LEVEL_ERROR       Only critical issue, when the system may fail
 * LV_LOG_LEVEL_NONE        Do not log anything
 */
#  define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN

 /* 1: Print the log with 'printf';
  * 0: user need to register a callback with `lv_log_register_print_cb`*/
#  define LV_LOG_PRINTF   0
#endif  /*LV_USE_LOG*/

  /*=================
   * Debug settings
   *================*/

   /* If Debug is enabled LittelvGL validates the parameters of the functions.
    * If an invalid parameter is found an error log message is printed and
    * the MCU halts at the error. (`LV_USE_LOG` should be enabled)
    * If you are debugging the MCU you can pause
    * the debugger to see exactly where  the issue is.
    *
    * The behavior of asserts can be overwritten by redefining them here.
    * E.g. #define LV_ASSERT_MEM(p)  <my_assert_code>
    */
#define LV_USE_DEBUG        1
#if LV_USE_DEBUG

    /*Check if the parameter is NULL. (Quite fast) */
#define LV_USE_ASSERT_NULL      1

/*Checks is the memory is successfully allocated or no. (Quite fast)*/
#define LV_USE_ASSERT_MEM       1

/*Check the integrity of `lv_mem` after critical operations. (Slow)*/
#ifndef LV_USE_ASSERT_MEM_INTEGRITY
#define LV_USE_ASSERT_MEM_INTEGRITY       0
#endif

/* Check the strings.
 * Search for NULL, very long strings, invalid characters, and unnatural repetitions. (Slow)
 * If disabled `LV_USE_ASSERT_NULL` will be performed instead (if it's enabled) */
#define LV_USE_ASSERT_STR       0

 /* Check NULL, the object's type and existence (e.g. not deleted). (Quite slow)
  * If disabled `LV_USE_ASSERT_NULL` will be performed instead (if it's enabled) */
#define LV_USE_ASSERT_OBJ       0

  /*Check if the styles are properly initialized. (Fast)*/
#define LV_USE_ASSERT_STYLE     1

#endif /*LV_USE_DEBUG*/

/*==================
 *    FONT USAGE
 *===================*/
// #if 1 || HASP_USE_FREETYPE<=0
//   #if TFT_HEIGHT>=480 && TFT_WIDTH>=480
//     #ifndef ROBOTOCONDENSED_REGULAR_24_LATIN1
//     #define ROBOTOCONDENSED_REGULAR_24_LATIN1 1
//     #endif
//     #ifndef ROBOTOCONDENSED_REGULAR_32_LATIN1
//     #define ROBOTOCONDENSED_REGULAR_32_LATIN1 1
//     #endif
//     #if HASP_USE_FREETYPE<=0
//       #ifndef ROBOTOCONDENSED_REGULAR_48_LATIN1
//       #define ROBOTOCONDENSED_REGULAR_48_LATIN1 1
//       #endif
//       #ifndef ROBOTOCONDENSED_REGULAR_64_LATIN1
//       #define ROBOTOCONDENSED_REGULAR_64_LATIN1 1
//       #endif
//       #ifndef ROBOTOCONDENSED_REGULAR_16_LATIN1
//       #define ROBOTOCONDENSED_REGULAR_16_LATIN1 1
//       #endif
//     #endif

//     #ifndef HASP_FONT_1
//     #define HASP_FONT_1 robotocondensed_regular_24_latin1  /* 5% Width */
//     #endif
//     #ifndef HASP_FONT_2
//     #define HASP_FONT_2 robotocondensed_regular_32_latin1  /* 5% Width */
//     #endif
//     #if HASP_USE_FREETYPE<=0
//       #ifndef HASP_FONT_3
//       #define HASP_FONT_3 robotocondensed_regular_48_latin1  /* 10% Width */
//       #endif
//       #ifndef HASP_FONT_4
//       #define HASP_FONT_4 robotocondensed_regular_64_latin1  /* 10% Height */
//       #endif
//       #ifndef HASP_FONT_5
//       #define HASP_FONT_5 robotocondensed_regular_16_latin1  /* 5% Width */
//       #endif
//     #endif

//     #ifndef HASP_FONT_SIZE_1
//     #define HASP_FONT_SIZE_1 24
//     #endif
//     #ifndef HASP_FONT_SIZE_2
//     #define HASP_FONT_SIZE_2 32
//     #endif
//     #if HASP_USE_FREETYPE<=0
//       #ifndef HASP_FONT_SIZE_3
//       #define HASP_FONT_SIZE_3 48
//       #endif
//       #ifndef HASP_FONT_SIZE_4
//       #define HASP_FONT_SIZE_4 64
//       #endif
//       #ifndef HASP_FONT_SIZE_5
//       #define HASP_FONT_SIZE_5 16
//       #endif
//     #endif
    
//   #elif TFT_HEIGHT>=320 && TFT_WIDTH>=320
//     #ifndef ROBOTOCONDENSED_REGULAR_16_LATIN1
//     #define ROBOTOCONDENSED_REGULAR_16_LATIN1 1
//     #endif
//     #ifndef ROBOTOCONDENSED_REGULAR_24_LATIN1
//     #define ROBOTOCONDENSED_REGULAR_24_LATIN1 1
//     #endif
//     #if HASP_USE_FREETYPE<=0
//       #ifndef ROBOTOCONDENSED_REGULAR_32_LATIN1
//       #define ROBOTOCONDENSED_REGULAR_32_LATIN1 1
//       #endif
//       #ifndef ROBOTOCONDENSED_REGULAR_48_LATIN1
//       #define ROBOTOCONDENSED_REGULAR_48_LATIN1 1
//       #endif
//       #ifndef ROBOTOCONDENSED_REGULAR_12_LATIN1
//       #define ROBOTOCONDENSED_REGULAR_12_LATIN1 1
//       #endif
//     #endif

//     #ifndef HASP_FONT_1
//     #define HASP_FONT_1 robotocondensed_regular_16_latin1  /* 5% Width */
//     #endif
//     #ifndef HASP_FONT_2
//     #define HASP_FONT_2 robotocondensed_regular_24_latin1  /* 5% Width */
//     #endif
//     #if HASP_USE_FREETYPE<=0
//       #ifndef HASP_FONT_3
//       #define HASP_FONT_3 robotocondensed_regular_32_latin1  /* 10% Width */
//       #endif
//       #ifndef HASP_FONT_4
//       #define HASP_FONT_4 robotocondensed_regular_48_latin1  /* 10% Height */
//       #endif
//       #ifndef HASP_FONT_5
//       #define HASP_FONT_5 robotocondensed_regular_12_latin1  /* 5% Width */
//       #endif
//     #endif

//     #ifndef HASP_FONT_SIZE_1
//     #define HASP_FONT_SIZE_1 16
//     #endif
//     #ifndef HASP_FONT_SIZE_2
//     #define HASP_FONT_SIZE_2 24
//     #endif
//     #if HASP_USE_FREETYPE<=0
//       #ifndef HASP_FONT_SIZE_3
//       #define HASP_FONT_SIZE_3 32
//       #endif
//       #ifndef HASP_FONT_SIZE_4
//       #define HASP_FONT_SIZE_4 48
//       #endif
//       #ifndef HASP_FONT_SIZE_5
//       #define HASP_FONT_SIZE_5 12
//       #endif
//     #endif

//   #elif TFT_HEIGHT>=272 && TFT_WIDTH>=272
//     #ifndef ROBOTOCONDENSED_REGULAR_14_LATIN1
//     #define ROBOTOCONDENSED_REGULAR_14_LATIN1 1
//     #endif
//     #ifndef ROBOTOCONDENSED_REGULAR_18_LATIN1
//     #define ROBOTOCONDENSED_REGULAR_18_LATIN1 1
//     #endif
//     #if HASP_USE_FREETYPE<=0
//       #ifndef ROBOTOCONDENSED_REGULAR_28_LATIN1
//       #define ROBOTOCONDENSED_REGULAR_28_LATIN1 1
//       #endif
//       #ifndef ROBOTOCONDENSED_REGULAR_36_LATIN1
//       #define ROBOTOCONDENSED_REGULAR_36_LATIN1 1
//       #endif
//       #ifndef ROBOTOCONDENSED_REGULAR_48_LATIN1
//       #define ROBOTOCONDENSED_REGULAR_48_LATIN1 1
//       #endif
//     #endif

//     #ifndef HASP_FONT_1
//     #define HASP_FONT_1 robotocondensed_regular_14_latin1  /* 5% Width */
//     #endif
//     #ifndef HASP_FONT_2
//     #define HASP_FONT_2 robotocondensed_regular_18_latin1  /* 5% Width */
//     #endif
//     #if HASP_USE_FREETYPE<=0
//       #ifndef HASP_FONT_3
//       #define HASP_FONT_3 robotocondensed_regular_28_latin1  /* 10% Width */
//       #endif
//       #ifndef HASP_FONT_4
//       #define HASP_FONT_4 robotocondensed_regular_36_latin1  /* 10% Height */
//       #endif
//       #ifndef HASP_FONT_5
//       #define HASP_FONT_5 robotocondensed_regular_48_latin1  /* 5% Width */
//       #endif
//     #endif

//     #ifndef HASP_FONT_SIZE_1
//     #define HASP_FONT_SIZE_1 14
//     #endif
//     #ifndef HASP_FONT_SIZE_2
//     #define HASP_FONT_SIZE_2 18
//     #endif
//     #if HASP_USE_FREETYPE<=0
//       #ifndef HASP_FONT_SIZE_3
//       #define HASP_FONT_SIZE_3 28
//       #endif
//       #ifndef HASP_FONT_SIZE_4
//       #define HASP_FONT_SIZE_4 36
//       #endif
//       #ifndef HASP_FONT_SIZE_5
//       #define HASP_FONT_SIZE_5 48
//       #endif
//     #endif

//   #else // smaller than 272

//     #ifndef HASP_FONT_1
//     #define HASP_FONT_1 robotocondensed_regular_12_latin1  /* 5% Width */
//     #endif
//     #ifndef HASP_FONT_2
//     #define HASP_FONT_2 robotocondensed_regular_16_latin1  /* 5% Width */
//     #endif
//     #if HASP_USE_FREETYPE<=0
//       #ifndef HASP_FONT_3
//       #define HASP_FONT_3 robotocondensed_regular_24_latin1  /* 10% Width */
//       #endif
//       #ifndef HASP_FONT_4
//       #define HASP_FONT_4 robotocondensed_regular_32_latin1  /* 10% Height */
//       #endif
//     #endif

//     #ifndef ROBOTOCONDENSED_REGULAR_12_LATIN1
//     #define ROBOTOCONDENSED_REGULAR_12_LATIN1 1
//     #endif
//     #ifndef ROBOTOCONDENSED_REGULAR_16_LATIN1
//     #define ROBOTOCONDENSED_REGULAR_16_LATIN1 1
//     #endif
//     #if HASP_USE_FREETYPE<=0
//       #ifndef ROBOTOCONDENSED_REGULAR_24_LATIN1
//       #define ROBOTOCONDENSED_REGULAR_24_LATIN1 1
//       #endif
//       #ifndef ROBOTOCONDENSED_REGULAR_32_LATIN1
//       #define ROBOTOCONDENSED_REGULAR_32_LATIN1 1
//       #endif
//       #ifndef ROBOTOCONDENSED_REGULAR_48_LATIN1
//       #define ROBOTOCONDENSED_REGULAR_48_LATIN1 1
//       #endif
//     #endif

//     #ifndef HASP_FONT_SIZE_1
//     #define HASP_FONT_SIZE_1 12
//     #endif
//     #ifndef HASP_FONT_SIZE_2
//     #define HASP_FONT_SIZE_2 16
//     #endif
//     #if HASP_USE_FREETYPE<=0
//       #ifndef HASP_FONT_SIZE_3
//       #define HASP_FONT_SIZE_3 24
//       #endif
//       #ifndef HASP_FONT_SIZE_4
//       #define HASP_FONT_SIZE_4 32
//       #endif
//       #ifndef HASP_FONT_SIZE_5
//       #define HASP_FONT_SIZE_5 48
//       #endif
//     #endif

//   #endif
// #endif

#ifndef ROBOTOCONDENSED_REGULAR_12_ALL
#define ROBOTOCONDENSED_REGULAR_12_ALL 1
#endif
#ifndef ROBOTOCONDENSED_REGULAR_16_ALL
#define ROBOTOCONDENSED_REGULAR_16_ALL 1
#endif
#ifndef ROBOTOCONDENSED_REGULAR_24_ALL
#define ROBOTOCONDENSED_REGULAR_24_ALL 1
#endif
#ifndef ROBOTOCONDENSED_REGULAR_32_ALL
#define ROBOTOCONDENSED_REGULAR_32_ALL 1
#endif

#ifndef HASP_FONT_1
#define HASP_FONT_1 robotocondensed_regular_12_all 
#endif
#ifndef HASP_FONT_2
#define HASP_FONT_2 robotocondensed_regular_16_all  
#endif
#ifndef HASP_FONT_3
#define HASP_FONT_3 robotocondensed_regular_24_all
#endif
#ifndef HASP_FONT_4
#define HASP_FONT_4 robotocondensed_regular_32_all
#endif

#ifndef HASP_FONT_SIZE_1
#define HASP_FONT_SIZE_1 12
#endif
#ifndef HASP_FONT_SIZE_2
#define HASP_FONT_SIZE_2 16
#endif
#ifndef HASP_FONT_SIZE_3
#define HASP_FONT_SIZE_3 24
#endif
#ifndef HASP_FONT_SIZE_4
#define HASP_FONT_SIZE_4 32
#endif

/* The built-in fonts contains the ASCII range and some Symbols with 4 bit-per-pixel.
 * The symbols are available via `LV_SYMBOL_...` defines
 * More info about fonts: https://docs.lvgl.io/v7/en/html/overview/font.html
 * To create a new font go to: https://lvgl.com/ttf-font-to-c-array
 */

/* Montserrat fonts with bpp = 4
 * https://fonts.google.com/specimen/Montserrat  */
#define LV_FONT_MONTSERRAT_8     0
#define LV_FONT_MONTSERRAT_10    0
#define LV_FONT_MONTSERRAT_12    0
#define LV_FONT_MONTSERRAT_14    0
#define LV_FONT_MONTSERRAT_16    0
#define LV_FONT_MONTSERRAT_18    0
#define LV_FONT_MONTSERRAT_20    0
#define LV_FONT_MONTSERRAT_22    0
#define LV_FONT_MONTSERRAT_24    0
#define LV_FONT_MONTSERRAT_26    0
#define LV_FONT_MONTSERRAT_28    0
#define LV_FONT_MONTSERRAT_30    0
#define LV_FONT_MONTSERRAT_32    0
#define LV_FONT_MONTSERRAT_34    0
#define LV_FONT_MONTSERRAT_36    0
#define LV_FONT_MONTSERRAT_38    0
#define LV_FONT_MONTSERRAT_40    0
#define LV_FONT_MONTSERRAT_42    0
#define LV_FONT_MONTSERRAT_44    0
#define LV_FONT_MONTSERRAT_46    0
#define LV_FONT_MONTSERRAT_48    0

/* Demonstrate special features */
#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0  /*bpp = 3*/
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0  /*Hebrew, Arabic, PErisan letters and all their forms*/
#define LV_FONT_SIMSUN_16_CJK            0  /*1000 most common CJK radicals*/

/*Pixel perfect monospace font
 * http://pelulamu.net/unscii/ */
#define LV_FONT_UNSCII_8     0
#define LV_FONT_UNSCII_16     0

/*Custom font*/
#define UNSCII_8_ICON 1
/* Optionally declare your custom fonts here.
 * You can use these fonts as default font too
 * and they will be available globally. E.g.
 * #define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(my_font_1) \
 *                                LV_FONT_DECLARE(my_font_2)
 */
//#define LV_FONT_CUSTOM_DECLARE

/* Enable it if you have fonts with a lot of characters.
 * The limit depends on the font size, font face and bpp
 * but with > 10,000 characters if you see issues probably you need to enable it.*/
#define LV_FONT_FMT_TXT_LARGE   0

/* Enables/disables support for compressed fonts. If it's disabled, compressed
 * glyphs cannot be processed by the library and won't be rendered.
 */
#define LV_USE_FONT_COMPRESSED 1

/* Enable subpixel rendering */
#define LV_USE_FONT_SUBPX 0
#if LV_USE_FONT_SUBPX
/* Set the pixel order of the display.
 * Important only if "subpx fonts" are used.
 * With "normal" font it doesn't matter.
 */
#define LV_FONT_SUBPX_BGR    0
#endif

    /*Declare the type of the user data of fonts (can be e.g. `void *`, `int`, `struct`)*/
typedef void* lv_font_user_data_t;

// #define FONT_CONCAT2(a, b)  a ## b
// #define FONT_CONCAT(a, b) FONT_CONCAT2(a, b)
// #define HASP_FONTNAME robotocondensed_regular_
// #define HASP_FONTNAME_BASE FONT_CONCAT(HASP_FONTNAME, _)
// #define HASP_CHARACTER_SET latin1

// /* Concatenate the fontname macros */
// #define HASP_FONT_1_size FONT_CONCAT(HASP_FONTNAME, 12)
// #define HASP_FONT_1_base FONT_CONCAT(HASP_FONT_1_size, _)
// #define HASP_FONT_1 FONT_CONCAT(HASP_FONT_1_base, HASP_CHARACTER_SET)


/*Always set a default font from the built-in fonts*/
#if 0 && HASP_USE_FREETYPE<=0 // LV_HIGH_RESOURCE_MCU>0
// #define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(lv_font_montserrat_16);

#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(HASP_FONT_1) \
                               LV_FONT_DECLARE(HASP_FONT_2) \
                               LV_FONT_DECLARE(HASP_FONT_3) \
                               LV_FONT_DECLARE(HASP_FONT_4) \
                               LV_FONT_DECLARE(HASP_FONT_5) \

#ifndef LV_FONT_DEFAULT
#define LV_FONT_DEFAULT        &HASP_FONT_2 //&lv_font_montserrat_16
#endif

#else

//#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(unscii_8_icon);
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(HASP_FONT_1) \
                               LV_FONT_DECLARE(HASP_FONT_2) \
                               LV_FONT_DECLARE(HASP_FONT_3) \
                               LV_FONT_DECLARE(HASP_FONT_4) \

#ifndef LV_FONT_DEFAULT
#if TFT_HEIGHT >= 480 && TFT_WIDTH >= 480
  #define LV_FONT_DEFAULT                  &HASP_FONT_4
  #define LV_THEME_DEFAULT_FONT_SMALL      &HASP_FONT_3
#elif TFT_HEIGHT >= 320 && TFT_WIDTH >= 320
  #define LV_FONT_DEFAULT                  &HASP_FONT_3
  #define LV_THEME_DEFAULT_FONT_SMALL      &HASP_FONT_2
#else // smaller than 320
  #define LV_FONT_DEFAULT                  &HASP_FONT_2
  #define LV_THEME_DEFAULT_FONT_SMALL      &HASP_FONT_1
#endif
#endif // LV_FONT_DEFAULT

#ifndef LV_THEME_DEFAULT_FONT_SMALL
  #define LV_THEME_DEFAULT_FONT_SMALL      LV_FONT_DEFAULT
#endif

#define LV_THEME_DEFAULT_FONT_NORMAL       LV_FONT_DEFAULT
#define LV_THEME_DEFAULT_FONT_SUBTITLE     LV_FONT_DEFAULT
#define LV_THEME_DEFAULT_FONT_TITLE        LV_FONT_DEFAULT

#endif // LV_HIGH_RESOURCE_MCU

/*================
 *  THEME USAGE
 *================*/

 /*Always enable at least on theme*/
#define LV_USE_THEME_MATERIAL    1   /*A fast and impressive theme*/

#define LV_THEME_DEFAULT_INIT               lv_theme_material_init // lv_theme_hasp_init // We init the theme ourselves
#define LV_THEME_DEFAULT_COLOR_PRIMARY      LV_COLOR_RED
#define LV_THEME_DEFAULT_COLOR_SECONDARY    LV_COLOR_BLUE
#define LV_THEME_DEFAULT_FLAG               0 //LV_THEME_MATERIAL_FLAG_NONE
// #if HASP_USE_FREETYPE<=0 //LV_HIGH_RESOURCE_MCU
// #define LV_THEME_DEFAULT_FONT_SMALL         &HASP_FONT_1 //&lv_font_montserrat_12
// #define LV_THEME_DEFAULT_FONT_NORMAL        &HASP_FONT_2 //&lv_font_montserrat_16
// #define LV_THEME_DEFAULT_FONT_SUBTITLE      &HASP_FONT_2 //&lv_font_montserrat_22
// #define LV_THEME_DEFAULT_FONT_TITLE         &HASP_FONT_2 //&lv_font_montserrat_22 //&lv_font_montserrat_28_compressed
// #else
// #define LV_THEME_DEFAULT_FONT_SMALL         LV_FONT_DEFAULT // &lv_font_montserrat_12
// #define LV_THEME_DEFAULT_FONT_NORMAL        LV_FONT_DEFAULT // &lv_font_montserrat_16
// #define LV_THEME_DEFAULT_FONT_SUBTITLE      LV_FONT_DEFAULT // &lv_font_montserrat_22
// #define LV_THEME_DEFAULT_FONT_TITLE         LV_FONT_DEFAULT // &lv_font_montserrat_28_compressed
// #endif

#define LV_USE_THEME_EMPTY 0
#define LV_USE_THEME_MONO 1
#define LV_USE_THEME_TEMPLATE 0
#define LV_USE_THEME_HASP 1

/*=================
 *  Text settings
 *=================*/

 /* Select a character encoding for strings.
  * Your IDE or editor should have the same character encoding
  * - LV_TXT_ENC_UTF8
  * - LV_TXT_ENC_ASCII
  * */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

  /*Can break (wrap) texts on these chars*/
#define LV_TXT_BREAK_CHARS                  " ,.;:-_"

/* If a word is at least this long, will break wherever "prettiest"
 * To disable, set to a value <= 0 */
#define LV_TXT_LINE_BREAK_LONG_LEN          0

 /* Minimum number of characters in a long word to put on a line before a break.
  * Depends on LV_TXT_LINE_BREAK_LONG_LEN. */
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN  3

  /* Minimum number of characters in a long word to put on a line after a break.
   * Depends on LV_TXT_LINE_BREAK_LONG_LEN. */
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3

   /* The control character to use for signalling text recoloring. */
#define LV_TXT_COLOR_CMD "#"

/* Support bidirectional texts.
 * Allows mixing Left-to-Right and Right-to-Left texts.
 * The direction will be processed according to the Unicode Bidirectioanl Algorithm:
 * https://www.w3.org/International/articles/inline-bidi-markup/uba-basics*/
#define LV_USE_BIDI     0
#if LV_USE_BIDI
 /* Set the default direction. Supported values:
  * `LV_BIDI_DIR_LTR` Left-to-Right
  * `LV_BIDI_DIR_RTL` Right-to-Left
  * `LV_BIDI_DIR_AUTO` detect texts base direction */
#define LV_BIDI_BASE_DIR_DEF  LV_BIDI_DIR_AUTO
#endif

  /*Change the built in (v)snprintf functions*/
#define LV_SPRINTF_CUSTOM   1   // saves 1.4 KiB
#if LV_SPRINTF_CUSTOM 
#  define LV_SPRINTF_INCLUDE <stdio.h>
#  define lv_snprintf     snprintf
#  define lv_vsnprintf    vsnprintf
#endif  /*LV_SPRINTF_CUSTOM*/


/*===================
 *  LV_OBJ SETTINGS
 *==================*/

 /*Declare the type of the user data of object (can be e.g. `void *`, `int`, `struct`)*/
typedef struct {
  uint8_t id:8;
  uint8_t objid:6;
  //uint8_t transitionid:4;
  //uint8_t actionid:4;
  uint8_t groupid:4;
  // uint8_t swipeid:4;
  void* ext;
  // char* action;
#if USE_OBJ_ALIAS > 0
  uint16_t aliashash;
  char *alias;  
#endif  // #if USE_OBJ_ALIAS > 0
} lv_obj_user_data_t;

/*1: enable `lv_obj_realaign()` based on `lv_obj_align()` parameters*/
#define LV_USE_OBJ_REALIGN          1

/* Enable to make the object clickable on a larger area.
 * LV_EXT_CLICK_AREA_OFF or 0: Disable this feature
 * LV_EXT_CLICK_AREA_TINY: The extra area can be adjusted horizontally and vertically (0..255 px)
 * LV_EXT_CLICK_AREA_FULL: The extra area can be adjusted in all 4 directions (-32k..+32k px)
 */
#define LV_USE_EXT_CLICK_AREA  LV_EXT_CLICK_AREA_TINY

 /*==================
  *  LV OBJ X USAGE
  *================*/
  /*
   * Documentation of the object types: https://docs.littlevgl.com/#Object-types
   */

   /*Arc (dependencies: -)*/
#define LV_USE_ARC      1

/*Bar (dependencies: -)*/
#define LV_USE_BAR      1

/*Button (dependencies: lv_cont*/
#define LV_USE_BTN      1
#if LV_USE_BTN != 0
/*Enable button-state animations - draw a circle on click (dependencies: LV_USE_ANIMATION)*/
#  define LV_BTN_INK_EFFECT   0
#endif

/*Button matrix (dependencies: -)*/
#define LV_USE_BTNMATRIX     1

/*Calendar (dependencies: -)*/
#define LV_USE_CALENDAR 0

/*Canvas (dependencies: lv_img)*/
#define LV_USE_CANVAS   1

/*Check box (dependencies: lv_btn, lv_label)*/
#define LV_USE_CHECKBOX       1

/*Chart (dependencies: -)*/
#define LV_USE_CHART    0
#if LV_USE_CHART
#  define LV_CHART_AXIS_TICK_LABEL_MAX_LEN    20
#endif

/*Container (dependencies: -*/
#define LV_USE_CONT     1

/*Color picker (dependencies: -*/
#define LV_USE_CPICKER   1

/*Drop down list (dependencies: lv_page, lv_label, lv_symbol_def.h)*/
#define LV_USE_DROPDOWN    1
#if LV_USE_DROPDOWN != 0
/*Open and close default animation time [ms] (0: no animation)*/
#  define LV_DROPDOWN_DEF_ANIM_TIME     200
#endif

/*Linemeter (dependencies: -*/
#define LV_USE_LINEMETER     1
#if LV_USE_LINEMETER

/* Set how precisely should the lines of the line meter be calculated.
 * Higher precision means slower rendering.
 * 0: normal
 * 1: extra precision in the inner ring
 * 2. extra precision on the outer ring too
 */
#  define LV_LINEMETER_PRECISE  2
#endif

/*Gauge (dependencies:lv_bar, lv_linemeter)*/
#define LV_USE_GAUGE    1

/*Image (dependencies: lv_label*/
#define LV_USE_IMG      1

/*Image Button (dependencies: lv_btn*/
#define LV_USE_IMGBTN   1
#if LV_USE_IMGBTN
/*1: The imgbtn requires left, mid and right parts and the width can be set freely*/
#  define LV_IMGBTN_TILED 1
#endif

/*Keyboard (dependencies: lv_btnm)*/
#define LV_USE_KEYBOARD       1

/*Label (dependencies: -*/
#define LV_USE_LABEL    1
#if LV_USE_LABEL != 0
/*Hor, or ver. scroll speed [px/sec] in 'LV_LABEL_LONG_ROLL/ROLL_CIRC' mode*/
#  define LV_LABEL_DEF_SCROLL_SPEED       20 // default 25

/* Waiting period at beginning/end of animation cycle */
#  define LV_LABEL_WAIT_CHAR_COUNT        5  // default 3

/*Enable selecting text of the label */
#  define LV_LABEL_TEXT_SEL               0

/*Store extra some info in labels (12 bytes) to speed up drawing of very long texts*/
#  define LV_LABEL_LONG_TXT_HINT          0
#endif

/*LED (dependencies: -)*/
#define LV_USE_LED      1
#define LV_LED_BRIGHT_MIN 0

/*Line (dependencies: -*/
#define LV_USE_LINE     1

/*List (dependencies: lv_page, lv_btn, lv_label, (lv_img optionally for icons ))*/
#define LV_USE_LIST     1
#if LV_USE_LIST != 0
/*Default animation time of focusing to a list element [ms] (0: no animation)  */
#  define LV_LIST_DEF_ANIM_TIME  100
#endif

/*Line meter (dependencies: *;)*/
#define LV_USE_LMETER   1

/*Mask (dependencies: -)*/
#define LV_USE_OBJMASK  0

/*Message box (dependencies: lv_rect, lv_btnm, lv_label)*/
#define LV_USE_MSGBOX     1

/*Page (dependencies: lv_cont)*/
#define LV_USE_PAGE     1
#if LV_USE_PAGE != 0
/*Focus default animation time [ms] (0: no animation)*/
#  define LV_PAGE_DEF_ANIM_TIME     400
#endif

/*Preload (dependencies: lv_arc, lv_anim)*/
#define LV_USE_SPINNER      1
#if LV_USE_SPINNER != 0
#  define LV_SPINNER_DEF_ARC_LENGTH   60      /*[deg]*/
#  define LV_SPINNER_DEF_SPIN_TIME    1000    /*[ms]*/
#  define LV_SPINNER_DEF_ANIM         LV_SPINNER_TYPE_SPINNING_ARC
#endif

/*Roller (dependencies: lv_ddlist)*/
#define LV_USE_ROLLER    1
#if LV_USE_ROLLER != 0
/*Focus animation time [ms] (0: no animation)*/
#  define LV_ROLLER_DEF_ANIM_TIME     200

/*Number of extra "pages" when the roller is infinite*/
#  define LV_ROLLER_INF_PAGES         7
#endif

/*Slider (dependencies: lv_bar)*/
#define LV_USE_SLIDER    1

/*Spinbox (dependencies: lv_ta)*/
#define LV_USE_SPINBOX       0

/*Switch (dependencies: lv_slider)*/
#define LV_USE_SWITCH       1

/*Text area (dependencies: lv_label, lv_page)*/
#define LV_USE_TEXTAREA       1
#if LV_USE_TEXTAREA != 0
#  define LV_TEXTAREA_DEF_CURSOR_BLINK_TIME 400     /*ms*/
#  define LV_TEXTAREA_DEF_PWD_SHOW_TIME     1500    /*ms*/
#endif

/*Table (dependencies: lv_label)*/
#define LV_USE_TABLE    0 //(LV_HIGH_RESOURCE_MCU)
#if LV_USE_TABLE
#  define LV_TABLE_COL_MAX    12
#endif

/*Tab (dependencies: lv_page, lv_btnm)*/
#define LV_USE_TABVIEW      1
#  if LV_USE_TABVIEW != 0
/*Time of slide animation [ms] (0: no animation)*/
#  define LV_TABVIEW_DEF_ANIM_TIME    300
#endif

/*Tileview (dependencies: lv_page) */
#define LV_USE_TILEVIEW     0
#if LV_USE_TILEVIEW
/*Time of slide animation [ms] (0: no animation)*/
#  define LV_TILEVIEW_DEF_ANIM_TIME   300
#endif

/*Window (dependencies: lv_cont, lv_btn, lv_label, lv_img, lv_page)*/
#define LV_USE_WIN      0

/*==================
 * Non-user section
 *==================*/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)    /* Disable warnings for Visual Studio*/
#  define _CRT_SECURE_NO_WARNINGS
#endif

 /*--END OF LV_CONF_H--*/

#endif /*LV_CONF_H*/

/*Be sure every define has a default value*/
#include "src/lv_conf_internal.h"

#endif /*End of "Content enable"*/
