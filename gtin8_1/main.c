////////////////////////////////
//~NOTE(tbt): header files

#include <windows.h>   // NOTE(tbt): windows system headers
#include <windowsx.h>  // NOTE(tbt): windows system headers
#include <wincrypt.h>  // NOTE(tbt): use the windows crypt api to seed a pseudo random number generator
#include <stdint.h>    // NOTE(tbt): fixed size integers
#include <stdbool.h>   // NOTE(tbt): bool, true, false
#include <stddef.h>    // NOTE(tbt): NULL
#include <stdio.h>     // NOTE(tbt): snprintf()
#include <ctype.h>     // NOTE(tbt): isprint(), isdigit()
#include <string.h>    // NOTE(tbt): strcmp

////////////////////////////////
//~NOTE(tbt): libraries

#pragma comment(lib, "user32.lib")   // NOTE(tbt): basic window functions
#pragma comment(lib, "gdi32.lib")    // NOTE(tbt): graphics functions
#pragma comment(lib, "comdlg32.lib") // NOTE(tbt): GetSaveFileNameA()


////////////////////////////////
//~NOTE(tbt): misc macros and constants

enum{
    WINDOW_DIMENSIONS_X = 720,
    WINDOW_DIMENSIONS_Y = 480,
    
    MAX_UI_WIDGET_TEXT = 512,
    MAX_UI_WIDGETS = 997,
    MAX_UI_FG_COL_STACK = 256,
    
    FONT_SIZE = 8,
    
    UI_FONT_SCALE = 1,
    UI_PADDING = 2,
};

enum{
    DUMMY_INVENTORY_ITEM_ITEM_NOT_FOUND = 0,
    DUMMY_INVENTORY_ITEM_INVALID_CODE = 1,
    
    DUMMY_INVENTORY_ITEM_MAX,
};

// NOTE(tbt): stringify and identifier in the preprocessor - has to be done in two steps due to the order in which the preprocessor expand macros
#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)
#define WSTRINGIFY(X) GLUE(L, STRINGIFY(X))

// NOTE(tbt): join two tokens in the preprocessor - has to be done in two steps due to the order in which the preprocessor expand macros
#define GLUE_(A, B) A ## B
#define GLUE(A, B) GLUE_(A, B)

// NOTE(tbt): the number of elements in a static array
#define ARRAY_COUNT(A) (sizeof(A)/sizeof(A[0]))

////////////////////////////////
//~NOTE(tbt): types

typedef struct Pixel{
    uint8_t b; // NOTE(tbt): blue colour component
    uint8_t g; // NOTE(tbt): green colour component
    uint8_t r; // NOTE(tbt): red colour component
    uint8_t x; // NOTE(tbt): pad to align to 32 bits
}Pixel;

typedef uint32_t UIWidgetFlags;
typedef enum UIWidgetFlags{
    UI_WIDGET_FLAGS_CLICKABLE             = (1 << 0), // NOTE(tbt): active on mouse down
    UI_WIDGET_FLAGS_TOGGLEABLE            = (1 << 1), // NOTE(tbt): toggle when active
    UI_WIDGET_FLAGS_DETOGGLE_ON_ANY_CLICK = (1 << 3), // NOTE(tbt): becomes untoglled on any mouse click outside
}UIWidgetFlags_ENUM;

typedef struct UIWidget{
    UIWidgetFlags flags;
    char text[MAX_UI_WIDGET_TEXT];
    int min[2];
    int max[2];
    bool is_clicked;
    bool is_toggled;
    Pixel fg_col;
    
    bool is_alive;
}UIWidget;

typedef struct UIState{
    UIWidget widgets[MAX_UI_WIDGETS];
    size_t widgets_count;
    UIWidget *hot;
    UIWidget *active;
    
    bool is_mouse_down;
    int mouse_x;
    int mouse_y;
    char char_input;
    
    Pixel fg_col_stack[MAX_UI_FG_COL_STACK];
    size_t fg_col_stack_count;
}UIState;

typedef enum ReceiptItemError{
    RECEIPT_ITEM_ERROR_NONE,
    RECEIPT_ITEM_ERROR_PARSE_ERROR,
    RECEIPT_ITEM_ERROR_ITEM_NOT_FOUND,
    RECEIPT_ITEM_ERROR_INVALID_GTIN8_CODE
} ReceiptItemError;

// NOTE(tbt): the receipt type is used both for the produced receipt
//            and the parsed structure for the input inventory file

typedef struct ReceiptItem{
    ReceiptItemError error;
    char gtin8_code[9];
    char name[MAX_UI_WIDGET_TEXT];
    double unit_price;
    int qty;
    
    // NOTE(tbt): used only for inventory, not receipt
    int restock_level;
    int target_stock;
}ReceiptItem;

typedef struct Receipt{
    ReceiptItem *items;
    size_t items_count;
}Receipt;

typedef enum GTIN8VerifyResult{
    VERIFY_GTIN8_RESULT_INVALID_INPUT_STRING = -1,
    VERIFY_GTIN8_RESULT_FAILURE,
    VERIFY_GTIN8_RESULT_SUCCESS,
}GTIN8VerifyResult;

typedef enum ProgramMode{
    PROGRAM_STATE_MENU,
    PROGRAM_STATE_CALCULATE_CHECK_DIGIT,
    PROGRAM_STATE_VERIFY_CODE,
    PROGRAM_STATE_CREATE_RECEIPT,
    PROGRAM_STATE_CHECK_STOCK,
}ProgramMode;

////////////////////////////////
//~NOTE(tbt): global variables

HWND g_window_handle;                                                  // NOTE(tbt): handle to the main window

static bool g_is_running = true;                                       // NOTE(tbt): true while the program is running, set to false to exit

static Pixel g_window_pixels[WINDOW_DIMENSIONS_X*WINDOW_DIMENSIONS_Y]; // NOTE(tbt): array of pixels representing the window
static BITMAPINFO g_bitmap_info;                                       // NOTE(tbt): structure specifying the format of the image to stretch over the window

const unsigned char g_font[128][8] =
{
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0000 (nul)
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0001
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0002
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0003
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0004
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0005
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0006
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0007
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0008
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0009
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000A
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000B
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000C
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000D
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000E
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+000F
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0010
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0011
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0012
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0013
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0014
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0015
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0016
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0017
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0018
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0019
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001A
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001B
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001C
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001D
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001E
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+001F
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0020 (space)
    { 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00 },   // U+0021 (!)
    { 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0022 (")
    { 0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00 },   // U+0023 (#)
    { 0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00 },   // U+0024 ($)
    { 0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00 },   // U+0025 (%)
    { 0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00 },   // U+0026 (&)
    { 0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0027 (')
    { 0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00 },   // U+0028 (()
    { 0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00 },   // U+0029 ())
    { 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00 },   // U+002A (*)
    { 0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00 },   // U+002B (+)
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06 },   // U+002C (,)
    { 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00 },   // U+002D (-)
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00 },   // U+002E (.)
    { 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00 },   // U+002F (/)
    { 0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00 },   // U+0030 (0)
    { 0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00 },   // U+0031 (1)
    { 0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00 },   // U+0032 (2)
    { 0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00 },   // U+0033 (3)
    { 0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00 },   // U+0034 (4)
    { 0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00 },   // U+0035 (5)
    { 0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00 },   // U+0036 (6)
    { 0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00 },   // U+0037 (7)
    { 0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00 },   // U+0038 (8)
    { 0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00 },   // U+0039 (9)
    { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00 },   // U+003A (:)
    { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06 },   // U+003B (;)
    { 0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00 },   // U+003C (<)
    { 0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00 },   // U+003D (=)
    { 0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00 },   // U+003E (>)
    { 0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00 },   // U+003F (?)
    { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00 },   // U+0040 (@)
    { 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00 },   // U+0041 (A)
    { 0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00 },   // U+0042 (B)
    { 0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00 },   // U+0043 (C)
    { 0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00 },   // U+0044 (D)
    { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00 },   // U+0045 (E)
    { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00 },   // U+0046 (F)
    { 0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00 },   // U+0047 (G)
    { 0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00 },   // U+0048 (H)
    { 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },   // U+0049 (I)
    { 0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00 },   // U+004A (J)
    { 0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00 },   // U+004B (K)
    { 0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00 },   // U+004C (L)
    { 0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00 },   // U+004D (M)
    { 0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00 },   // U+004E (N)
    { 0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00 },   // U+004F (O)
    { 0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00 },   // U+0050 (P)
    { 0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00 },   // U+0051 (Q)
    { 0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00 },   // U+0052 (R)
    { 0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00 },   // U+0053 (S)
    { 0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },   // U+0054 (T)
    { 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00 },   // U+0055 (U)
    { 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00 },   // U+0056 (V)
    { 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00 },   // U+0057 (W)
    { 0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00 },   // U+0058 (X)
    { 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00 },   // U+0059 (Y)
    { 0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00 },   // U+005A (Z)
    { 0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00 },   // U+005B ([)
    { 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00 },   // U+005C (\)
    { 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00 },   // U+005D (])
    { 0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00 },   // U+005E (^)
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF },   // U+005F (_)
    { 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+0060 (`)
    { 0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00 },   // U+0061 (a)
    { 0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00 },   // U+0062 (b)
    { 0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00 },   // U+0063 (c)
    { 0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00 },   // U+0064 (d)
    { 0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00 },   // U+0065 (e)
    { 0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00 },   // U+0066 (f)
    { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F },   // U+0067 (g)
    { 0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00 },   // U+0068 (h)
    { 0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },   // U+0069 (i)
    { 0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E },   // U+006A (j)
    { 0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00 },   // U+006B (k)
    { 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },   // U+006C (l)
    { 0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00 },   // U+006D (m)
    { 0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00 },   // U+006E (n)
    { 0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00 },   // U+006F (o)
    { 0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F },   // U+0070 (p)
    { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78 },   // U+0071 (q)
    { 0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00 },   // U+0072 (r)
    { 0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00 },   // U+0073 (s)
    { 0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00 },   // U+0074 (t)
    { 0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00 },   // U+0075 (u)
    { 0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00 },   // U+0076 (v)
    { 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00 },   // U+0077 (w)
    { 0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00 },   // U+0078 (x)
    { 0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F },   // U+0079 (y)
    { 0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00 },   // U+007A (z)
    { 0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00 },   // U+007B ({)
    { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18 },   // U+007C (|)
    { 0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00 },   // U+007D (})
    { 0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // U+007E (~)
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }    // U+007F
};

// NOTE(tbt): use X-macros to create a table to convert from WM_* constants to strings
#define X(IDENTIFIER) { .id = (IDENTIFIER), .name_str = #IDENTIFIER, .id_str = STRINGIFY(IDENTIFIER), },
struct StringFromWindowMessageTableEntry{
    const int id;
	const char *name_str;
	const char *id_str;
}g_string_from_window_message_table[] = {
#include "win32_wm_x_macros.h"
};

static ProgramMode g_program_mode;

static UIState g_ui_state = {0};

////////////////////////////////
//~NOTE(tbt): main program

// NOTE(tbt): compute a hash of a string
static size_t
HashString(char *string, size_t modulo){
    size_t result = 5381;
    while(*string){
        result = ((result << 5) + result) + *string;
        string += 1;
    }
    return result % modulo;
}

// NOTE(tbt): lookup a window message in the string conversion table
static const char *
StringFromWindowMessage(int wm){
	const char *result = "ERROR - no such window message";
	bool keep_searching = true;
    
	int bottom = 0;
	int top = ARRAY_COUNT(g_string_from_window_message_table);
	while(keep_searching){
		int i = (bottom + top)/2;
		if(g_string_from_window_message_table[i].id == wm){
			result = g_string_from_window_message_table[i].name_str;
			keep_searching = false;
		}else if(wm < g_string_from_window_message_table[i].id){
			int next_i = (bottom + i)/2;
			if(i == next_i){
				keep_searching = false;
			}else{
				top = i;
				i = next_i;
			}
		}else if(wm > g_string_from_window_message_table[i].id){
			int next_i = (i + top)/2;
			if(i == next_i){
				keep_searching = false;
			}else{
				bottom = i;
				i = next_i;
			}
		}
	}
    
	return result;
}

static void
FilenameFromSaveDialogue(char *extension,
                         char *buffer, size_t buffer_size){
    char filters[256];
    {
        int extension_index = snprintf(filters, sizeof(filters), "%s files (*.%s)\0*.*\0\0", extension, extension) + 1;
        snprintf(filters + extension_index, sizeof(filters) - extension_index, "*.%s", extension);
    }
    
    char file[MAX_PATH] = {0};
    OPENFILENAMEA open_file_name =
    {
        .lStructSize = sizeof(open_file_name),
        .hwndOwner = g_window_handle,
        .lpstrFilter = filters,
        .lpstrFile = file,
        .nMaxFile = sizeof(file),
        .lpstrInitialDir = (LPSTR)NULL,
        .lpstrTitle = "save",
        .Flags = 0,
        .lpstrDefExt = extension,
    };
    
    if(GetSaveFileNameA(&open_file_name))
    {
        memset(buffer, 0, buffer_size);
        strncpy(buffer, open_file_name.lpstrFile, buffer_size - 1);
    }
    
}

static void
MeasureString(const char *string,
              int x, int y,
              int scale_factor,
              int padding,
              int result_min[2],
              int result_max[2]){
    int row_start = x;
    
    result_min[0] = x - padding;
    result_min[1] = y - padding;
    result_max[0] = x;
    result_max[1] = y + (FONT_SIZE << scale_factor);
    
    char c;
    while(c = *string++){
        if('\n' == c){
            y += (FONT_SIZE << scale_factor);
            x = row_start;
        }else if(c < 128){
            x += (FONT_SIZE << scale_factor);
        }
        
        if(x > result_max[0]){
            result_max[0] = x;
        }
        
        if(y > result_max[1]){
            result_max[1] = y;
        }
    }
    
    result_max[0] += padding;
    result_max[1] += padding;
}

// NOTE(tbt): render a string using a bitmap font to the window pixels buffer
static void
DrawString(const char *string,
           int x, int y,
           int scale_factor,
           Pixel colour){
    int row_start = x;
    
    char c;
    while(c = *string++){
        if(c == '\n'){
            y += FONT_SIZE << scale_factor;
            x = row_start;
        }else if(c < 128){
            for(int _y = 0;
                _y < (FONT_SIZE << scale_factor);
                _y += 1){
                unsigned char row = g_font[c][_y >> scale_factor];
                for(int _x = 0;
                    _x < (FONT_SIZE << scale_factor);
                    _x += 1){
                    if(row & (1 << (_x >> scale_factor))){
                        int window_x = x + _x;
                        int window_y = y + _y;
                        if(0 <= window_x && window_x < WINDOW_DIMENSIONS_X &&
                           0 <= window_y && window_y < WINDOW_DIMENSIONS_Y){
                            g_window_pixels[window_x + window_y*WINDOW_DIMENSIONS_X] = colour;
                        }
                    }
                }
            }
            x += FONT_SIZE << scale_factor;
        }
    }
}

static void
DrawRectangleFill(Pixel col,
                  int min[2],
                  int max[2]){
    for(int y = min[1];
        y < max[1];
        y += 1){
        for(int x = min[0];
            x < max[0];
            x += 1){
            if(x >= 0 && x < WINDOW_DIMENSIONS_X &&
               y >= 0 && y < WINDOW_DIMENSIONS_Y){
                g_window_pixels[x + y*WINDOW_DIMENSIONS_X] = col;
            }
        }
    }
}

// NOTE(tbt): hit test a widget
static bool
UIWidgetHasPoint(UIWidget *widget,
                 int x, int y){
    if(x >= widget->min[0] &&
       x <= widget->max[0] &&
       y >= widget->min[1] &&
       y <= widget->max[1]){
        return true;
    }else{
        return false;
    }
}

static void
UIPushColour(Pixel colour){
    if(g_ui_state.fg_col_stack_count < MAX_UI_FG_COL_STACK - 1){
        g_ui_state.fg_col_stack_count += 1;
        g_ui_state.fg_col_stack[g_ui_state.fg_col_stack_count] = colour;
    }
}

static void
UIPopColour(){
    if(g_ui_state.fg_col_stack_count > 0){
        g_ui_state.fg_col_stack_count -= 1;
    }
}

static void
UIPrepare(void){
    g_ui_state.hot = NULL;
    for(size_t widget_index = 0;
        widget_index < MAX_UI_WIDGETS;
        widget_index += 1){
        g_ui_state.widgets[widget_index].is_alive = false;
    }
    g_ui_state.fg_col_stack[0] = (Pixel){ 255, 255, 255 };
}

static void
UIFinish(void){
    if(!g_ui_state.is_mouse_down){
        g_ui_state.active = NULL;
    }
    g_ui_state.char_input = 0;
    
    for(size_t widget_index = 0;
        widget_index < MAX_UI_WIDGETS;
        widget_index += 1){
        UIWidget *widget = &g_ui_state.widgets[widget_index];
        if(widget->is_alive){
            Pixel bg_col = (Pixel){ 200, 15, 15 };
            int x_offset = 0;
            if(widget->is_toggled){
                bg_col = (Pixel){ 45, 45, 100 };
                x_offset += UI_PADDING;
            }else if(widget == g_ui_state.active){
                bg_col = (Pixel){ 100, 45, 45 };
            }else if(widget == g_ui_state.hot){
                bg_col = (Pixel){ 150, 30, 30 };
            }
            int min[2] = { widget->min[0] + x_offset, widget->min[1] };
            int max[2] = { widget->max[0] + x_offset, widget->max[1] };
            DrawRectangleFill(bg_col, min, max);
            DrawString(widget->text, widget->min[0] + UI_PADDING + x_offset, widget->min[1] + UI_PADDING, UI_FONT_SCALE, widget->fg_col);
        }else{
            memset(widget, 0, sizeof(widget));
        }
    }
}

static UIWidget *
UIPushWidget(char *str_id,
             UIWidgetFlags flags){
    UIWidget *result = &g_ui_state.widgets[HashString(str_id, MAX_UI_WIDGETS)];
    if(result->flags != flags){
        memset(result, 0, sizeof(*result));
        result->flags = flags;
    }
    result->fg_col = g_ui_state.fg_col_stack[g_ui_state.fg_col_stack_count];
    result->is_alive = true;
    return result;
}

static void
UIDoWidget(UIWidget *widget){
    if(UIWidgetHasPoint(widget, g_ui_state.mouse_x, g_ui_state.mouse_y)){
        g_ui_state.hot = widget;
        if((widget->flags & UI_WIDGET_FLAGS_CLICKABLE) &&
           g_ui_state.is_mouse_down){
            g_ui_state.active = widget;
        }
    }else if((widget->flags & UI_WIDGET_FLAGS_DETOGGLE_ON_ANY_CLICK) &&
             g_ui_state.is_mouse_down){
        widget->is_toggled = false;
    }
    
    if(widget->flags & UI_WIDGET_FLAGS_CLICKABLE){
        if(g_ui_state.hot == widget &&
           g_ui_state.active == widget &&
           !g_ui_state.is_mouse_down){
            widget->is_clicked = true;
        }else{
            widget->is_clicked = false;
        }
    }
    
    if((widget->flags & UI_WIDGET_FLAGS_TOGGLEABLE) &&
       widget->is_clicked){
        widget->is_toggled = !widget->is_toggled;
    }
}

static void
UILabel(char *text, int x, int y){
    UIWidget *widget = UIPushWidget(text, 0);
    strncpy(widget->text, text, sizeof(widget->text) - 1);
    widget->min[0] = widget->max[0] = x;
    widget->min[1] = widget->max[1] = y;
    UIDoWidget(widget);
}

static void
UILabelF(int x, int y, char *fmt, ...){
    char text[MAX_UI_WIDGET_TEXT];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text) - 1, fmt, args);
    va_end(args);
    UIWidget *widget = UIPushWidget(text, 0);
    strncpy(widget->text, text, sizeof(widget->text) - 1);
    widget->min[0] = widget->max[0] = x;
    widget->min[1] = widget->max[1] = y;
    UIDoWidget(widget);
}

static bool
UIButton(char *text, int x, int y){
    UIWidget *widget = UIPushWidget(text, UI_WIDGET_FLAGS_CLICKABLE);
    strncpy(widget->text, text, sizeof(widget->text) - 1);
    MeasureString(text, x, y, UI_FONT_SCALE, UI_PADDING, widget->min, widget->max);
    UIDoWidget(widget);
    return widget->is_clicked;
}

static bool
UIToggleButton(char *text, int x, int y){
    UIWidget *widget = UIPushWidget(text,
                                    UI_WIDGET_FLAGS_CLICKABLE |
                                    UI_WIDGET_FLAGS_TOGGLEABLE);
    strncpy(widget->text, text, sizeof(widget->text) - 1);
    MeasureString(text, x, y, UI_FONT_SCALE, UI_PADDING, widget->min, widget->max);
    UIDoWidget(widget);
    return widget->is_toggled;
}

static char *
UILineEdit(char *str_id,
           int x, int y,
           int max_characters){
    max_characters += 1; // NOTE(tbt): allow for NULL terminator
    
    if(max_characters > MAX_UI_WIDGET_TEXT){
        max_characters = MAX_UI_WIDGET_TEXT;
    }
    
    UIWidget *widget = UIPushWidget(str_id,
                                    UI_WIDGET_FLAGS_CLICKABLE |
                                    UI_WIDGET_FLAGS_TOGGLEABLE |
                                    UI_WIDGET_FLAGS_DETOGGLE_ON_ANY_CLICK);
    widget->min[0] = x - UI_PADDING;
    widget->min[1] = y - UI_PADDING;
    widget->max[0] = x + UI_PADDING + (max_characters - 1)*(FONT_SIZE << UI_FONT_SCALE);
    widget->max[1] = y + UI_PADDING + (FONT_SIZE << UI_FONT_SCALE);
    UIDoWidget(widget);
    if(widget->is_toggled && g_ui_state.char_input){
        char c = g_ui_state.char_input;
        size_t len = strlen(widget->text);
        if(g_ui_state.char_input == 8 && len > 0){
            widget->text[len - 1] = '\0';
        }else if(isprint(c) && len < max_characters - 1){
            widget->text[len] = c;
            widget->text[len + 1] ='\0';
        }
    }
    return widget->text;
}

// NOTE(tbt): draw the window pixels buffer to the window surface
static void
RefreshScreen(HDC device_context_handle){
    // NOTE(tbt): set the width of the image to stretch over the window
    g_bitmap_info.bmiHeader.biWidth  =  WINDOW_DIMENSIONS_X;
    g_bitmap_info.bmiHeader.biHeight = -WINDOW_DIMENSIONS_Y;
    
    StretchDIBits(device_context_handle,                          // NOTE(tbt): the handle of the GDI context to draw to
                  0, 0, WINDOW_DIMENSIONS_X, WINDOW_DIMENSIONS_Y, // NOTE(tbt): the rectangle to stretch over
                  0, 0, WINDOW_DIMENSIONS_X, WINDOW_DIMENSIONS_Y, // NOTE(tbt): the source rectangle of the input pixels
                  g_window_pixels,                                // NOTE(tbt): the image to stretch
                  &g_bitmap_info,                                 // NOTE(tbt): BITMAPINFO structure specifying pixel format
                  DIB_RGB_COLORS, SRCCOPY);                       // NOTE(tbt): the raster operation mode to use - in this case just copy and overwrite what was there
    
    memset(g_window_pixels, 0, sizeof(g_window_pixels));
}

static void
GTIN8FromFirst7Digits(char result[9],
                      char input[8]){
    if(7 == strlen(input)){
        memcpy(result, input, 8);
        int sum = 0;
        bool is_error = false;
        for(int i = 0;
            i < 7 && !is_error;
            i += 1){
            if(isdigit(result[i])){
                int digit = result[i] - '0';
                if(0 == i % 2){
                    digit *= 3;
                }
                sum += digit;
            }else{
                strncpy(result, "error", 7);
                is_error = true;
            }
        }
        if(!is_error){
            int next_multiple_of_ten = sum + (10 - (sum % 10));
            if(0 == sum % 10){
                next_multiple_of_ten = sum;
            }
            int check_digit = next_multiple_of_ten - sum;
            result[7] = check_digit + '0';
            result[8] = '\0';
        }
    }else{
        strncpy(result, "error", 7);
    }
}

static GTIN8VerifyResult
GTIN8Verify(char input[9]){
    GTIN8VerifyResult result = VERIFY_GTIN8_RESULT_SUCCESS;
    if(8 == strlen(input)){
        int sum = 0;
        for(int i = 0;
            i < 7 && VERIFY_GTIN8_RESULT_SUCCESS == result;
            i += 1){
            if(isdigit(input[i])){
                int digit = input[i] - '0';
                if(0 == i % 2){
                    digit *= 3;
                }
                sum += digit;
            }else{
                result = VERIFY_GTIN8_RESULT_INVALID_INPUT_STRING;
            }
        }
        if(VERIFY_GTIN8_RESULT_SUCCESS == result){
            int next_multiple_of_ten = sum + (10 - (sum % 10));
            if(0 == sum % 10){
                next_multiple_of_ten = sum;
            }
            int check_digit = next_multiple_of_ten - sum;
            if(input[7] - '0' != check_digit){
                result = VERIFY_GTIN8_RESULT_FAILURE;
            }
        }
    }else{
        result = VERIFY_GTIN8_RESULT_INVALID_INPUT_STRING;
    }
    return result;
}

static ReceiptItem *
ReceiptPushItem(Receipt *receipt){
    int max_receipt_items = 4194304;
    if(NULL == receipt->items){
        // NOTE(tbt): reserve enough virtual address space for a stupid amount of receipt items
        //            more than are likely to fit in physical memory
        receipt->items = VirtualAlloc(NULL, max_receipt_items*sizeof(ReceiptItem), MEM_RESERVE, PAGE_NOACCESS);
    }
    
    // NOTE(tbt): commit to physical memory as needed
    ReceiptItem *result = &receipt->items[receipt->items_count];
    VirtualAlloc(result, sizeof(ReceiptItem), MEM_COMMIT, PAGE_READWRITE);
    
    receipt->items_count += 1;
    
    return result;
}

static void
ReceiptClear(Receipt *receipt){
    if(NULL != receipt->items){
        VirtualFree(receipt->items, sizeof(receipt->items[0])*receipt->items_count, MEM_DECOMMIT);
        VirtualFree(receipt->items, sizeof(receipt->items[0])*receipt->items_count, MEM_RELEASE);
        receipt->items = NULL;
    }
    receipt->items_count = 0;
}

static void
ReceiptParseInventoryFile(Receipt *inventory,
                          char *path){
    ReceiptClear(inventory);
    
    // NOTE(tbt): the inventory stores 2 'dummy' items which can be used to represent
    //            errors in the final receipt
    {
        ReceiptItem *item_not_found = ReceiptPushItem(inventory);
        item_not_found->error = RECEIPT_ITEM_ERROR_ITEM_NOT_FOUND;
        ReceiptItem *invalid_gtin8_code = ReceiptPushItem(inventory);
        invalid_gtin8_code->error = RECEIPT_ITEM_ERROR_INVALID_GTIN8_CODE;
    }
    
    char *file_buffer = NULL;
    size_t file_size = 0;
    
    HANDLE file_handle = CreateFileA(path,
                                     GENERIC_READ,
                                     0, 0,
                                     OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL,
                                     0);
    if(INVALID_HANDLE_VALUE != file_handle){
        size_t n_total_bytes_to_read;{
            DWORD hi_size, lo_size;
            lo_size = GetFileSize(file_handle, &hi_size);
            n_total_bytes_to_read = 0;
            n_total_bytes_to_read |= ((uint64_t)hi_size) << 32;
            n_total_bytes_to_read |= ((uint64_t)lo_size) <<  0;
        }
        file_buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n_total_bytes_to_read + 1);
        DWORD n_bytes_read;
        if(!ReadFile(file_handle, file_buffer, n_total_bytes_to_read, &n_bytes_read, 0) ||
           n_bytes_read != n_total_bytes_to_read){
            HeapFree(GetProcessHeap(), 0, file_buffer);
            file_buffer = NULL;
        }else{
            file_size = n_total_bytes_to_read;
        }
        CloseHandle(file_handle);
    }
    
    if(NULL != file_buffer){
        ReceiptItem *item = ReceiptPushItem(inventory);
        
        enum{
            PARSE_STATE_GTIN8_CODE,
            PARSE_STATE_NAME,
            PARSE_STATE_PRICE,
            PARSE_STATE_QTY,
            PARSE_STATE_RESTOCK_LEVEL,
            PARSE_STATE_TARGET_STOCK,
            
            PARSE_STATE_MAX
        } parse_state = 0;
        
        for(size_t i = 0;
            i < file_size;
            i += 1){
            if('\n' == file_buffer[i]){
                if(i < file_size - 1 &&
                   '\n' != file_buffer[i + 1]){
                    item = ReceiptPushItem(inventory);
                    parse_state = 0;
                }
            }else if(',' == file_buffer[i]){
                parse_state += 1;
                parse_state %= PARSE_STATE_MAX;
                // NOTE(tbt): skip over white space after commas
                while(i < file_size - 1 &&
                      (' '  == file_buffer[i + 1] ||
                       '\t' == file_buffer[i + 1] ||
                       '\v' == file_buffer[i + 1])){
                    i += 1;
                }
            }else{
                if(PARSE_STATE_GTIN8_CODE == parse_state){
                    size_t len = strlen(item->gtin8_code);
                    if(len < 8){
                        item->gtin8_code[len] = file_buffer[i];
                    }else{
                        item->error = RECEIPT_ITEM_ERROR_PARSE_ERROR;
                    }
                }else if(PARSE_STATE_NAME == parse_state){
                    size_t len = strlen(item->name);
                    if(len < MAX_UI_WIDGET_TEXT - 1){
                        item->name[len] = file_buffer[i];
                    }else{
                        item->error = RECEIPT_ITEM_ERROR_PARSE_ERROR;
                    }
                }else if(PARSE_STATE_PRICE == parse_state){
                    char *end_ptr;
                    item->unit_price = strtod(&file_buffer[i], &end_ptr);
                    if(NULL == end_ptr){
                        item->error = RECEIPT_ITEM_ERROR_PARSE_ERROR;
                    }else{
                        i = end_ptr - file_buffer - 1;
                    }
                }else if(PARSE_STATE_QTY == parse_state){
                    char *end_ptr;
                    item->qty = strtol(&file_buffer[i], &end_ptr, 10);
                    if(NULL == end_ptr){
                        item->error = RECEIPT_ITEM_ERROR_PARSE_ERROR;
                    }else{
                        i = end_ptr - file_buffer - 1;
                    }
                }else if(PARSE_STATE_RESTOCK_LEVEL == parse_state){
                    char *end_ptr;
                    item->restock_level = strtol(&file_buffer[i], &end_ptr, 10);
                    if(NULL == end_ptr){
                        item->error = RECEIPT_ITEM_ERROR_PARSE_ERROR;
                    }else{
                        i = end_ptr - file_buffer - 1;
                    }
                }else if(PARSE_STATE_TARGET_STOCK == parse_state){
                    char *end_ptr;
                    item->target_stock = strtol(&file_buffer[i], &end_ptr, 10);
                    if(NULL == end_ptr){
                        item->error = RECEIPT_ITEM_ERROR_PARSE_ERROR;
                    }else{
                        i = end_ptr - file_buffer - 1;
                    }
                }
            }
        }
        
        HeapFree(GetProcessHeap(), 0, file_buffer);
    }
}

static void
ReceiptSerialiseInventoryFile(Receipt *inventory,
                              char *path){
    HANDLE file_handle = CreateFileA(path,
                                     GENERIC_WRITE,
                                     0, 0,
                                     CREATE_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL,
                                     0);
    if(INVALID_HANDLE_VALUE != file_handle){
        for(size_t i = DUMMY_INVENTORY_ITEM_MAX;
            i < inventory->items_count;
            i += 1){
            char line[4096] = {0};
            int n_bytes_to_write = snprintf(line, sizeof(line) - 1,
                                            "%.8s, %s, %.2f, %d, %d, %d,\n",
                                            inventory->items[i].gtin8_code,
                                            inventory->items[i].name,
                                            inventory->items[i].unit_price,
                                            inventory->items[i].qty,
                                            inventory->items[i].restock_level,
                                            inventory->items[i].target_stock);
            DWORD n_bytes_written;
            WriteFile(file_handle, line, n_bytes_to_write, &n_bytes_written, NULL);
            
        }
        CloseHandle(file_handle);
    }
}

static ReceiptItem *
ReceiptItemFromGTIN8Code(Receipt *receipt,
                         char gtin8_code[9]){
    ReceiptItem *result = &receipt->items[DUMMY_INVENTORY_ITEM_ITEM_NOT_FOUND];
    if(VERIFY_GTIN8_RESULT_SUCCESS == GTIN8Verify(gtin8_code)){
        for(size_t i = 0;
            i < receipt->items_count;
            i += 1){
            if(0 == strncmp(receipt->items[i].gtin8_code, gtin8_code, 8)){
                result = &receipt->items[i];
                break;
            }
        }
    }else{
        result = &receipt->items[DUMMY_INVENTORY_ITEM_INVALID_CODE];
    }
    return result;
}

// NOTE(tbt): callback for window messages
static LRESULT
Wndproc(HWND window_handle,
        UINT message,
        WPARAM w_param,
        LPARAM l_param){
    int result = 0;
    
    OutputDebugStringA(StringFromWindowMessage(message));
    OutputDebugStringA("\n");
    
    switch(message){
        case(WM_CREATE):{
            // NOTE(tbt): initialise game on window creation
        }break;
        
        case(WM_SIZING):{
            // NOTE(tbt): ensure the window is always the correct fixed size
            RECT *drag_rectangle   = (RECT *)l_param;
            drag_rectangle->right  = drag_rectangle->left + WINDOW_DIMENSIONS_X;
            drag_rectangle->bottom = drag_rectangle->top  + WINDOW_DIMENSIONS_Y;
            result = TRUE; // NOTE(tbt): return true to show we have handled this messag
        }break;
        
        case(WM_MOUSEMOVE):{
            g_ui_state.mouse_x = GET_X_LPARAM(l_param);
            g_ui_state.mouse_y = GET_Y_LPARAM(l_param);
        }break;
        
        case(WM_LBUTTONDOWN):{
            g_ui_state.is_mouse_down = true;
        }break;
        
        case(WM_LBUTTONUP):{
            g_ui_state.is_mouse_down = false;
        }break;
        
        case(WM_CHAR):{
            if(!(w_param & 0xFF80)){
                g_ui_state.char_input = w_param & 0x7F;
            }
        }break;
        
        case(WM_QUIT):
        case(WM_DESTROY):{
            g_is_running = false;
        }break;
        
        default:{
            result = DefWindowProc(window_handle, message, w_param, l_param);
        }break;
    }
    
    return result;
}

int WINAPI
wWinMain(HINSTANCE instance_handle,
         HINSTANCE prev_instance_handle,
         PWSTR command_line,
         int show_mode){
    // NOTE(tbt): the name of the class we are going to register with windows for out window
    wchar_t *window_class_name = L"HANG_MAN";
    
    // NOTE(tbt): fill out the window class structure
    WNDCLASSEXW window_class = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = Wndproc,
        .hInstance = instance_handle,
        .hCursor = LoadCursorW(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)COLOR_WINDOW,
        .lpszClassName = window_class_name,
    };
    // NOTE(tbt): register the window class with windows
    RegisterClassExW(&window_class);
    
    // NOTE(tbt): some variables to control the style of our window
    DWORD window_styles = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    int window_x = CW_USEDEFAULT;
    int window_y = CW_USEDEFAULT;
    int window_w = WINDOW_DIMENSIONS_X;
    int window_h = WINDOW_DIMENSIONS_Y;
    
    // NOTE(tbt): adjust the window rect to caculate the correct window size from the
    //            specified client area size (e.g. add additional height for the title bar)
    RECT window_rect = { 0, 0, window_w, window_h };
    AdjustWindowRect(&window_rect, window_styles, FALSE);
    
    // NOTE(tbt): create a window using the class and styles we set up earlier
    HWND window_handle = CreateWindowW(window_class_name,
                                       L"GTIN-8 Utils",
                                       window_styles,
                                       window_x, window_y,
                                       window_rect.right - window_rect.left,
                                       window_rect.bottom - window_rect.top,
                                       NULL,
                                       NULL,
                                       instance_handle,
                                       NULL);
    
    // NOTE(tbt): save the window handle globally
    g_window_handle = window_handle;
    
    // NOTE(tbt): setup the bitmap we will use to represent our window's pixels
    BITMAPINFOHEADER *bih = &g_bitmap_info.bmiHeader;
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biPlanes = 1;
    bih->biBitCount = 32;
    bih->biCompression = BI_RGB;
    bih->biSizeImage = 0;
    bih->biClrUsed = 0;
    bih->biClrImportant = 0;
    
    // NOTE(tbt): show our window
    ShowWindow(window_handle, show_mode);
    
    HDC device_context_handle = GetDC(window_handle);
    
    // NOTE(tbt): main loop
    while(g_is_running){
        MSG message;
        while(PeekMessage(&message, NULL, 0, 0, PM_REMOVE)){
            // NOTE(tbt): dispatch the message to the callback registered for our window class
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
        
        UIPrepare();{
            static char *inventory_path = "inventory.csv";
            static Receipt inventory = {0};
            
            switch(g_program_mode)
            {
                case(PROGRAM_STATE_MENU):{
                    int y = 75;
                    int x = 200;
                    
                    UILabel("GTIN-8 UTILS\n~~~~~~~~~~~~", x, y);
                    UILabel("select an operation:", x, (y += 96));
                    if(UIButton("calculate check digit", x, (y += 24))){
                        g_program_mode = PROGRAM_STATE_CALCULATE_CHECK_DIGIT;
                    }
                    if(UIButton("verify code", x, (y += 24))){
                        g_program_mode = PROGRAM_STATE_VERIFY_CODE;
                    }
                    if(UIButton("create receipt", x, (y += 24))){
                        g_program_mode = PROGRAM_STATE_CREATE_RECEIPT;
                        ReceiptParseInventoryFile(&inventory, inventory_path);
                    }
                    if(UIButton("check stock", x, (y += 24))){
                        g_program_mode = PROGRAM_STATE_CHECK_STOCK;
                        ReceiptParseInventoryFile(&inventory, inventory_path);
                    }
                    if(UIButton("quit", x, (y += 24))){
                        g_is_running = false;
                    }
                }break;
                
                case(PROGRAM_STATE_CALCULATE_CHECK_DIGIT):{
                    UILabel("calculate check digit", 80, UI_PADDING*2);
                    if(UIButton("back", UI_PADDING*2, UI_PADDING*2)){
                        g_program_mode = PROGRAM_STATE_MENU;
                    }
                    UILabel("input first 7 digits:", 34, 196);
                    char *input = UILineEdit("calculate check digit entry", 375, 196, 7);
                    UILabel("full code:", 34 + 176, 220);
                    char output[9];
                    GTIN8FromFirst7Digits(output, input);
                    UILabel(output, 375, 220);
                }break;
                
                case(PROGRAM_STATE_VERIFY_CODE):{
                    UILabel("verify code", 80, UI_PADDING*2);
                    if(UIButton("back", UI_PADDING*2, UI_PADDING*2)){
                        g_program_mode = PROGRAM_STATE_MENU;
                    }
                    UILabel("input GTIN-8 code:", 82, 196);
                    char *input = UILineEdit("verify code entry", 375, 196, 8);
                    GTIN8VerifyResult result = GTIN8Verify(input);
                    if(VERIFY_GTIN8_RESULT_INVALID_INPUT_STRING == result){
                        UILabel("malformed input string", 82, 220);
                    }else if(VERIFY_GTIN8_RESULT_FAILURE == result){
                        UILabel("invalid code :(", 82, 220);
                    }else if(VERIFY_GTIN8_RESULT_SUCCESS == result){
                        UILabel("valid code :)", 82, 220);
                    }
                }break;
                
                case(PROGRAM_STATE_CREATE_RECEIPT):{
                    static Receipt receipt = {0};
                    
                    UILabel("create receipt", 80, UI_PADDING*2);
                    if(UIButton("back", UI_PADDING*2, UI_PADDING*2)){
                        ReceiptClear(&receipt);
                        g_program_mode = PROGRAM_STATE_MENU;
                    }
                    
                    static int qty = 1;
                    
                    char *input_gtin8_code = UILineEdit("receipt add item entry", 25, 48, 8);
                    int max_qty;{
                        ReceiptItem *inventory_item = ReceiptItemFromGTIN8Code(&inventory, input_gtin8_code);
                        max_qty = inventory_item->qty;
                    }
                    UILabelF(167, 48, "* %d", qty);
                    DrawRectangleFill((Pixel){ 119, 120, 120 },
                                      (int[]){ 155, 46 },
                                      (int[]){ 228, 66 });
                    if(UIButton("+", 210, 68) && qty < max_qty){
                        qty += 1;
                    }
                    if(UIButton("-", 190, 68) && qty > 1){
                        qty -= 1;
                    }
                    if(UIButton("add", 240, 48)){
                        ReceiptItem *inventory_item = ReceiptItemFromGTIN8Code(&inventory, input_gtin8_code);
                        ReceiptItem *receipt_item = ReceiptPushItem(&receipt);
                        memcpy(receipt_item, inventory_item, sizeof(*receipt_item));
                        memset(input_gtin8_code, 0, MAX_UI_WIDGET_TEXT);
                        receipt_item->qty = qty;
                        qty = 1;
                    }
                    
                    int y = 100;
                    double total = 0.0;
                    for(size_t i = 0;
                        i < receipt.items_count;
                        i += 1){
                        int x = UI_PADDING*2;
                        if(RECEIPT_ITEM_ERROR_NONE == receipt.items[i].error){
                            double sub_total = receipt.items[i].qty*receipt.items[i].unit_price;
                            total += sub_total;
                            
                            // NOTE(tbt): had to use $ instead of £ as £ symbol not in ASCII
                            UILabelF(x, y, "%8s|%18.18s|%2d|$%.2f|$%2.2f",
                                     receipt.items[i].gtin8_code,
                                     receipt.items[i].name,
                                     receipt.items[i].qty,
                                     receipt.items[i].unit_price,
                                     sub_total);
                        }else if(RECEIPT_ITEM_ERROR_PARSE_ERROR == receipt.items[i].error){
                            UIPushColour((Pixel){ 0, 0, 255 });
                            UILabel("error parsing item from inventory file", x, y);
                            UIPopColour();
                        }
                        else if(RECEIPT_ITEM_ERROR_INVALID_GTIN8_CODE == receipt.items[i].error){
                            UIPushColour((Pixel){ 0, 0, 255 });
                            UILabel("invalid GTIN-8 code", x, y);
                            UIPopColour();
                        }
                        else if(RECEIPT_ITEM_ERROR_ITEM_NOT_FOUND == receipt.items[i].error){
                            UIPushColour((Pixel){ 0, 0, 255 });
                            UILabel("item not found", x, y);
                            UIPopColour();
                        }
                        y += FONT_SIZE << UI_FONT_SCALE;
                    }
                    
                    y += 24;
                    UILabelF(UI_PADDING*2 + 440, y, "total: %.2f", total);
                    
                    y += 24;
                    if(UIButton("save", UI_PADDING*2 + 440, y)){
                        for(size_t i = 0;
                            i < receipt.items_count;
                            i += 1){
                            ReceiptItem *inventory_item = ReceiptItemFromGTIN8Code(&inventory, receipt.items[i].gtin8_code);
                            inventory_item->qty -= receipt.items[i].qty;
                        }
                        ReceiptSerialiseInventoryFile(&inventory, inventory_path);
                        ReceiptClear(&receipt);
                    }
                } break;
                
                case(PROGRAM_STATE_CHECK_STOCK):{
                    UILabel("check stock", 80, UI_PADDING*2);
                    if(UIButton("back", UI_PADDING*2, UI_PADDING*2)){
                        g_program_mode = PROGRAM_STATE_MENU;
                    }
                    
                    int y = 100;
                    bool are_out_of_stock_items = false;
                    for(size_t i = DUMMY_INVENTORY_ITEM_MAX;
                        i < inventory.items_count;
                        i += 1){
                        int x = UI_PADDING*2;
                        if(RECEIPT_ITEM_ERROR_NONE == inventory.items[i].error){
                            if(inventory.items[i].qty < inventory.items[i].restock_level){
                                UIPushColour((Pixel){ 0, 75, 255 });
                                are_out_of_stock_items = true;
                            }else{
                                UIPushColour((Pixel){ 0, 255, 0 });
                            }
                            UILabelF(x, y, "%8s|%18.18s|%3d|%2d|%3d",
                                     inventory.items[i].gtin8_code,
                                     inventory.items[i].name,
                                     inventory.items[i].qty,
                                     inventory.items[i].restock_level,
                                     inventory.items[i].target_stock);
                            UIPopColour();
                        }else if(RECEIPT_ITEM_ERROR_PARSE_ERROR == inventory.items[i].error){
                            UIPushColour((Pixel){ 0, 0, 255 });
                            UILabel("error parsing item from inventory file", x, y);
                            UIPopColour();
                        }
                        else if(RECEIPT_ITEM_ERROR_INVALID_GTIN8_CODE == inventory.items[i].error){
                            UIPushColour((Pixel){ 0, 0, 255 });
                            UILabel("invalid GTIN-8 code", x, y);
                            UIPopColour();
                        }
                        else if(RECEIPT_ITEM_ERROR_ITEM_NOT_FOUND == inventory.items[i].error){
                            UIPushColour((Pixel){ 0, 0, 255 });
                            UILabel("item not found", x, y);
                            UIPopColour();
                        }
                        y += FONT_SIZE << UI_FONT_SCALE;
                    }
                    if(are_out_of_stock_items){
                        y += 24;
                        if(UIButton("order_restock", UI_PADDING*2 + 440, y)){
                            char path[MAX_PATH];
                            FilenameFromSaveDialogue("csv", path, sizeof(path));
                            HANDLE file_handle = CreateFileA(path,
                                                             GENERIC_WRITE,
                                                             0, 0,
                                                             CREATE_ALWAYS,
                                                             FILE_ATTRIBUTE_NORMAL,
                                                             0);
                            if(INVALID_HANDLE_VALUE != file_handle){
                                DWORD n_bytes_written;
                                
                                char headers[] = "\"product code\",\"description\",\"qty\",\"price\",\"sub-total\",\n";
                                WriteFile(file_handle, headers, sizeof(headers) - 1, &n_bytes_written, NULL);
                                
                                double total = 0;
                                for(size_t i = DUMMY_INVENTORY_ITEM_MAX;
                                    i < inventory.items_count;
                                    i += 1){
                                    int qty = inventory.items[i].target_stock - inventory.items[i].qty;
                                    if(inventory.items[i].qty < inventory.items[i].restock_level){
                                        double sub_total = qty * inventory.items[i].unit_price;
                                        total += sub_total;
                                        
                                        char line[4096] = {0};
                                        int n_bytes_to_write = snprintf(line, sizeof(line) - 1,
                                                                        "\"%.8s\",\"%s\",\"%d\",\"$%.2f\",\"$%.2f\",\n",
                                                                        inventory.items[i].gtin8_code,
                                                                        inventory.items[i].name,
                                                                        qty,
                                                                        inventory.items[i].unit_price,
                                                                        sub_total);
                                        WriteFile(file_handle, line, n_bytes_to_write, &n_bytes_written, NULL);
                                        
                                        inventory.items[i].qty = inventory.items[i].target_stock;
                                    }
                                }
                                
                                char line[4096] = {0};
                                int n_bytes_to_write = snprintf(line, sizeof(line) - 1, "\"\",\"\",\"\",\"total:\",\"$%.2f\",\n", total);
                                WriteFile(file_handle, line, n_bytes_to_write, &n_bytes_written, NULL);
                                
                                CloseHandle(file_handle);
                            }
                            ReceiptSerialiseInventoryFile(&inventory, inventory_path);
                            ReceiptParseInventoryFile(&inventory, inventory_path);
                        }
                    }
                } break;
            }
        }UIFinish();
        
        RefreshScreen(device_context_handle);
    }
    
    ReleaseDC(window_handle, device_context_handle);
    
    // NOTE(tbt): hide window to perform cleanup in the background
    ShowWindow(window_handle, SW_HIDE);
    
    // NOTE(tbt): cleanup the window
    DestroyWindow(window_handle);
    
    // NOTE(tbt): return a status code to the OS - in this case 0, meaning exit successfully
    return 0;
}
