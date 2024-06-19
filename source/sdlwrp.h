#ifndef __SDL_WRAPPER_DEF__
#define __SDL_WRAPPER_DEF__

#include <SDL2/SDL.h>

//#define WINDOW_WIDTH 1920
//#define WINDOW_HEIGHT 1080
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define OFFSET_RIGHT 1
#define OFFSET_LEFT	 0

#define TEX_PATH_MAX_LEN 400

void Wrp_InitSDL(const char *windowName);
void Wrp_ExitSDL(void);
void Wrp_ClearScreen(void);
void Wrp_DrawImage(int imageId, int x, int y, int width, int height);
void Wrp_DrawText(const char *inputText, int x, int y, float textSize);
void Wrp_DrawTextF(const char *inputText, int x, int y, float textSize, ...);
void Wrp_DrawTextF_r(const char *inputText, int x, int y, float textSize, SDL_Renderer *r, ...);
void Wrp_DrawTextWrap(const char *inputText, int x, int y, float textSize, int maxWidth);
void Wrp_FinishDrawing(void);
void Wrp_DrawImageWithDir(int imageId, int x, int y, int width, int height, bool flip);
void Wrp_DrawImageWithDirXY(int imageId, int x, int y, int width, int height, bool flipX, bool flipY);
int Wrp_ImageWidth(int imageId);
int Wrp_ImageHeight(int imageId);
//inline void Wrp_ToggleWindowFullscreen(void);
bool Wrp_IsFullscreen(void);
void Wrp_DrawImageWithRotation(int imageId, int x, int y, int width, int height, int rotation);
void Wrp_DrawLine(int x1, int y1, int x2, int y2, int color);
void Wrp_SetTitle(const char *title);
int Wrp_ScanInputs(void);
void Wrp_DrawRectangle(int x, int y, int width, int height, int color);
void Wrp_DrawRectangleNoFill(int x, int y, int width, int height, int color);
int Wrp_DrawTextMaxWidth(const char *inputText, int x, int y, float textSize, int maxWidth, int flags);
void Wrp_GetWindowSize(int *w, int *h);
int Wrp_GetWinWidth(void);
int Wrp_GetWinHeight(void);
SDL_Window *Wrp_GetWindow();
void SetFullscreen(bool full);
bool GetFullscreen(void);
const char *GetClipboardText(void);
void Wrp_SetRendColor(int color);
//void AddEditorBar(void);
void Wrp_DrawTextWrapMaxHeight(const char *inputText, int x, int y, float textSize, int maxWidth, int maxHeight);
int GetTextHeight(const char *inputText, float textSize, int maxWidth);
void Wrp_LoadTextures(const char texPathsArr[][TEX_PATH_MAX_LEN], size_t rows);
void Wrp_CheckDefault(void);
SDL_Renderer *Wrp_GetMainRenderer(void);
void Wrp_DrawText_r(const char *inputText, int x, int y, float textSize, SDL_Renderer *r);

typedef enum {
	IMG_FOLDER_ICON,
	IMG_FILE_ICON,
	IMG_IMAGE_ICON,
	IMG_VIDEO_ICON,
	IMG_AUDIO_ICON,
	nulldas
} ImageId_t;

enum RECT_COLORS {
	CLR_BLU = 0,
	CLR_WHT,
	CLR_RED,
	CLR_GRN,
	CLR_BLK,
	CLR_YLW,
	CLR_GRY,
	CLR_LBLU,
	CLR_LGRY,
};

#endif // ndef sdl wrapper