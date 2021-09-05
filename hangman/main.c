////////////////////////////////
//~NOTE(tbt): header files

#include <windows.h>   // NOTE(tbt): windows system headers
#include <windowsx.h>  // NOTE(tbt): windows system headers
#include <wincrypt.h>  // NOTE(tbt): use the windows crypt api to seed a pseudo random number generator
#include <stdint.h>    // NOTE(tbt): fixed size integers
#include <stdbool.h>   // NOTE(tbt): bool, true, false
#include <stddef.h>    // NOTE(tbt): NULL
#include <stdio.h>    // NOTE(tbt): snprintf

#include "resources.h" // NOTE(tbt): generated header file containg artwork as array literals

////////////////////////////////
//~NOTE(tbt): libraries

#pragma comment(lib, "user32.lib")   // NOTE(tbt): basic window functions
#pragma comment(lib, "gdi32.lib")    // NOTE(tbt): graphics functions
#pragma comment(lib, "advapi32.lib") // NOTE(tbt): crypto functions used for RNG

////////////////////////////////
//~NOTE(tbt): types

typedef struct Pixel{
    uint8_t b; // NOTE(tbt): blue colour component
    uint8_t g; // NOTE(tbt): green colour component
    uint8_t r; // NOTE(tbt): red colour component
    uint8_t x; // NOTE(tbt): pad to align to 32 bits
} Pixel;

typedef struct GenericImage{
    int width;       // NOTE(tbt): width of the image in pixels
    int height;      // NOTE(tbt): height of the image in pixels
    Pixel pixels[0]; // NOTE(tbt): pixel data follows rest of struct
} GenericImage;

////////////////////////////////
//~NOTE(tbt): macros and constants

// NOTE(tbt): fixed size window to make my life a bit easier
enum WindowDimensions{
    WINDOW_DIMENSIONS_X = 640,
    WINDOW_DIMENSIONS_Y = 480,
};

// NOTE(tbt): keep track of which state the game is in
typedef enum GameState{
    GAME_STATE_PLAYING,
    GAME_STATE_WON,
    GAME_STATE_LOST,
} GameState;

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
//~NOTE(tbt): global variables

HWND g_window_handle;                                                  // NOTE(tbt): handle to the main window

static bool g_is_running = true;                                       // NOTE(tbt): true while the program is running, set to false to exit

static GameState g_game_state = GAME_STATE_PLAYING;

static GenericImage *g_hangman_art[] = {                               // NOTE(tbt): frames of artwork for each number of lives left
    &life_10,
    &life_9,
    &life_8,
    &life_7,
    &life_6,
    &life_5,
    &life_4,
    &life_3,
    &life_2,
    &life_1,
};
static int g_lives_left = ARRAY_COUNT(g_hangman_art);                  // NOTE(tbt): number of lives left

static const char *g_word_to_guess = NULL;                             // NOTE(tbt): the selected word to guess
static char g_guessed_word[4096];                                      // NOTE(tbt): the word as guessed so far, with un-guessed letters replaced with _
static char g_guessed_letters[4096];                                   // NOTE(tbt): an array of all letters which have been guessed alread
static int g_guessed_letters_count = 0;                                // NOTE(tbt): the number of used elements in the guessed letters array

static Pixel g_window_pixels[WINDOW_DIMENSIONS_X*WINDOW_DIMENSIONS_Y]; // NOTE(tbt): array of pixels representing the window
static BITMAPINFO g_bitmap_info;                                       // NOTE(tbt): structure specifying the format of the image to stretch over the window

// NOTE(tbt): use X-macros to create a table to convert from WM_* constants to strings
#define X(IDENTIFIER) { .id = (IDENTIFIER), .name_str = #IDENTIFIER, .id_str = STRINGIFY(IDENTIFIER), },
struct StringFromWindowMessageTableEntry{
    const int id;
	const char *name_str;
	const char *id_str;
} g_string_from_window_message_table[] = {
#include "win32_wm_x_macros.h"
};

////////////////////////////////
//~NOTE(tbt): main program

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
		}
		else if(wm > g_string_from_window_message_table[i].id){
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

// NOTE(tbt): compute a hash of an integer, useful for generating a pseudorandom sequence
static unsigned int
HashInt(unsigned int i){
    int result = i;
    result = ((result >> 16) ^ result)*0x45d9f3b;
    result = ((result >> 16) ^ result)*0x45d9f3b;
    result = ((result >> 16) ^ result);
    return result;
}

// NOTE(tbt): retrieve the next integer in a pseudo random sequence
static int
RandIntNext(void){
    int result;
    
    static bool is_seed_initialised = false;
    static unsigned int seed = 0;
    
    // NOTE(tbt): initialise seed using the windows crypt api the first time this function is called
    if(!is_seed_initialised){
        HCRYPTPROV ctx = 0;
        CryptAcquireContextW(&ctx, NULL, NULL, PROV_DSS, CRYPT_VERIFYCONTEXT);
        CryptGenRandom(ctx, sizeof(seed), (BYTE *)&seed);
        CryptReleaseContext(ctx, 0);
        
        is_seed_initialised = true;
    }
    
    result = HashInt(seed);
    seed += 1;
    
    return result;
}

// NOTE(tbt): copy an images pixels to the window pixels buffer
static void
DrawImage(GenericImage *image,
          int x, int y){
    for(int _x = 0;
        _x < image->width;
        _x += 1){
        for(int _y = 0;
            _y < image->height;
            _y += 1){
            int window_x = x + _x;
            int window_y = y + _y;
            if(0 <= window_x && window_x < WINDOW_DIMENSIONS_X &&
               0 <= window_y && window_y < WINDOW_DIMENSIONS_Y){
                g_window_pixels[window_x + window_y * WINDOW_DIMENSIONS_X] = image->pixels[_x + _y * image->width];
            }
        }
    }
}

// NOTE(tbt): render a string using a bitmap font to the window pixels buffer
static void
DrawString(const char *string,
           int x, int y,
           int scale_factor,
           Pixel colour){
    int row_start = x;
    
    for (char c = *string;
         c != '\0';
         string += 1, c = *string){
        if (c == '\n'){
            y += (FONT_SIZE << scale_factor);
            x = row_start;
        }else if (c < 128){
            for (int _y = 0;
                 _y < (FONT_SIZE << scale_factor);
                 _y += 1){
                unsigned char row = g_font[c][_y >> scale_factor];
                for (int _x = 0;
                     _x < (FONT_SIZE << scale_factor);
                     _x += 1){
                    if (row & (1 << (_x >> scale_factor))){
                        int window_x = x + _x;
                        int window_y = y + _y;
                        if(0 <= window_x && window_x < WINDOW_DIMENSIONS_X &&
                           0 <= window_y && window_y < WINDOW_DIMENSIONS_Y){
                            g_window_pixels[window_x + window_y * WINDOW_DIMENSIONS_X] = colour;
                        }
                    }
                }
            }
            x += FONT_SIZE << scale_factor;
        }
    }
}

// NOTE(tbt): draw the window pixels buffer to the window surface
static void
DrawWindowPixels(HDC device_context_handle){
    // NOTE(tbt): set the width of the image to stretch over the window
    g_bitmap_info.bmiHeader.biWidth  =  WINDOW_DIMENSIONS_X;
    g_bitmap_info.bmiHeader.biHeight = -WINDOW_DIMENSIONS_Y;
    
    StretchDIBits(device_context_handle,                          // NOTE(tbt): the handle of the GDI context to draw to
                  0, 0, WINDOW_DIMENSIONS_X, WINDOW_DIMENSIONS_Y, // NOTE(tbt): the rectangle to stretch over
                  0, 0, WINDOW_DIMENSIONS_X, WINDOW_DIMENSIONS_Y, // NOTE(tbt): the source rectangle of the input pixels
                  g_window_pixels,                                // NOTE(tbt): the image to stretch
                  &g_bitmap_info,                                 // NOTE(tbt): BITMAPINFO structure specifying pixel format
                  DIB_RGB_COLORS, SRCCOPY);                       // NOTE(tbt): the raster operation mode to use - in this case just copy and overwrite what was there
}

// NOTE(tbt): reset gameplay global variable to defaut values, e.g. start a new game
static void
ResetGame(void){
    g_word_to_guess = NULL;
    g_guessed_letters_count = 0;
    g_lives_left = ARRAY_COUNT(g_hangman_art);
    strncpy(g_guessed_word, "type a letter to\nguess", ARRAY_COUNT(g_guessed_word) - 1);
    g_game_state = GAME_STATE_PLAYING;
    
    // NOTE(tbt): refresh the screen
    RedrawWindow(g_window_handle, NULL, NULL, RDW_INVALIDATE);
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
            ResetGame();
        }
        
        case(WM_SIZING):{
            // NOTE(tbt): ensure the window is always the correct fixed size
            RECT *drag_rectangle   = (RECT *)l_param;
            drag_rectangle->right  = drag_rectangle->left + WINDOW_DIMENSIONS_X;
            drag_rectangle->bottom = drag_rectangle->top  + WINDOW_DIMENSIONS_Y;
            result = TRUE; // NOTE(tbt): return true to show we have handled this messag
        } break;
        
        case(WM_PAINT):{
            if(g_lives_left >= ARRAY_COUNT(g_hangman_art)){
                // NOTE(tbt): clear the screen with white if all lives are remaining
                memset(g_window_pixels, 255, sizeof(g_window_pixels));
            }else{
                // NOTE(tbt): otherwise lookup and draw the appropriate artwork
                DrawImage(g_hangman_art[g_lives_left], 0, 0);
            }
            
            // NOTE(tbt): draw the word
            DrawString(g_guessed_word, 16, 16, 2, (Pixel){ 0, 0, 0 });
            
            // NOTE(tbt): draw already guessed letters
            DrawString(g_guessed_letters, 16, WINDOW_DIMENSIONS_Y - (FONT_SIZE << 1) - 16, 1, (Pixel){ 0, 0, 0 });
            
            // NOTE(tbt): draw game state message
            if(GAME_STATE_WON == g_game_state){
                DrawString("you won!\npress any\nkey to play\nagain", 60, 60, 2, (Pixel){ 0, 255, 0 });
            }else if(GAME_STATE_LOST == g_game_state){
                char message[4096] = {0};
                snprintf(message, sizeof(message) - 1, "you lost...\n\nthe word was\n'%s'\n\npress any\nkey to try\nagain", g_word_to_guess);
                DrawString(message, 60, 60, 2, (Pixel){ 0, 0, 255 });
            }
            
            // NOTE(tbt): redraw the window
            PAINTSTRUCT ps;
            HDC device_context_handle = BeginPaint(window_handle, &ps);
            DrawWindowPixels(device_context_handle);
            EndPaint(window_handle, &ps);
        } break;
        
        case(WM_CHAR):{
            if(GAME_STATE_PLAYING == g_game_state){
                bool is_repeat = LOWORD(l_param) > 1;
                if(!is_repeat && w_param < 128){
                    char letter = w_param;
                    
                    // NOTE(tbt): check to see if the letter has already been guessed
                    bool is_letter_guessed = false;
                    for(int guessed_letter_index = 0;
                        guessed_letter_index < g_guessed_letters_count;
                        guessed_letter_index += 1){
                        if(g_guessed_letters[guessed_letter_index] == letter){
                            is_letter_guessed = true;
                            break;
                        }
                    }
                    
                    // NOTE(tbt): ignore already guessed letters
                    if(!is_letter_guessed){
                        // NOTE(tbt): insert guessed letter into guessed letters array
                        g_guessed_letters[g_guessed_letters_count] = letter;
                        g_guessed_letters_count += 1;
                        
                        // NOTE(tbt): select a random word if neccessary
                        if(NULL == g_word_to_guess){
                            g_word_to_guess = g_words[RandIntNext() % ARRAY_COUNT(g_words)];
                        }
                        
                        // NOTE(tbt): make a copy of the word
                        int word_len = strlen(g_word_to_guess);
                        memcpy(g_guessed_word, g_word_to_guess, word_len);
                        g_guessed_word[word_len] = '\0';
                        
                        bool is_word_guessed_correctly = true;
                        bool is_guessed_letter_in_word = false;
                        
                        // NOTE(tbt): loop through each letter
                        for(int char_index = 0;
                            char_index < word_len;
                            char_index += 1){
                            // NOTE(tbt): check if the guessed letter is correct
                            if(g_word_to_guess[char_index] == letter){
                                is_guessed_letter_in_word = true;
                            }
                            // NOTE(tbt): check if the current letter of the word has already been guessed
                            bool is_letter_guessed = false;
                            for(int guessed_letter_index = 0;
                                guessed_letter_index < g_guessed_letters_count;
                                guessed_letter_index += 1){
                                if(g_guessed_word[char_index] == g_guessed_letters[guessed_letter_index]){
                                    is_letter_guessed = true;
                                    break;
                                }
                            }
                            // NOTE(tbt): replace letters that have not yet been guessed with underscores
                            if(!is_letter_guessed){
                                // NOTE(tbt): keep track of whether the entire word has been guessed
                                is_word_guessed_correctly = false;
                                g_guessed_word[char_index] = '_';
                            }
                        }
                        
                        if(is_word_guessed_correctly){
                            // NOTE(tbt): win the game if the entire word was guessed
                            g_game_state = GAME_STATE_WON;
                        }else if(!is_guessed_letter_in_word){
                            // NOTE(tbt): decrease lives if the guess was incorrect
                            g_lives_left -= 1;
                            if(g_lives_left == 0){
                                // NOTE(tbt): loose if there are no lives left
                                g_game_state = GAME_STATE_LOST;
                            }
                        }
                        
                        // NOTE(tbt): refresh the screen
                        RedrawWindow(window_handle, NULL, NULL, RDW_INVALIDATE);
                    }
                }
            }else{
                // NOTE(tbt): reset game on any key press
                ResetGame();
            }
        } break;
        
        case(WM_QUIT):
        case(WM_DESTROY):{
            g_is_running = false;
        } break;
        
        default:{
            // NOTE(tbt): otherwise, use the default behaviour
            result = DefWindowProc(window_handle, message, w_param, l_param);
        } break;
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
                                       L"Hang Man",
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
    
    // DEBUG(tbt): test bitmap
    for(int pixel_index = 0;
        pixel_index < ARRAY_COUNT(g_window_pixels);
        pixel_index += 1)
    {
        g_window_pixels[pixel_index] = (Pixel){
            255.0f * ((float)(pixel_index % WINDOW_DIMENSIONS_X) / (float)WINDOW_DIMENSIONS_X),
            255.0f * ((float)(pixel_index / WINDOW_DIMENSIONS_X) / (float)WINDOW_DIMENSIONS_Y),
            255, 0,
        };
    }
    
    // NOTE(tbt): show our window
    ShowWindow(window_handle, show_mode);
    
    // NOTE(tbt): main loop
    while(g_is_running){
        // NOTE(tbt): loop through each message in our window's message queue,
        //            removing each message after it has been retrieved
        MSG message;
        while(PeekMessage(&message, NULL, 0, 0, PM_REMOVE)){
            // NOTE(tbt): dispatch the message to the callback registered for our window class
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
        // NOTE(tbt): wait for the next message
        if(g_is_running){
            WaitMessage();
        }
    }
    
    // NOTE(tbt): hide window to perform cleanup in the background
    ShowWindow(window_handle, SW_HIDE);
    
    // NOTE(tbt): cleanup the window
    DestroyWindow(window_handle);
    
    // NOTE(tbt): return a status code to the OS - in this case 0, meaning exit successfully
    return 0;
}
