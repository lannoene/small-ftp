#include "sdlwrp.h"

#include <stdio.h>

#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <pthread.h>
//#include <windows.h>
//#include <SDL_syswm.h>

#include "re_array.h"
#include "address_dbg.h"

//window rend vars
SDL_Window *window = NULL;
SDL_Renderer *rend = NULL;
//text vars
TTF_Font* font;
const int text_load_size = 50; // px

SDL_Texture **texture_array = NULL;
size_t texArrayLen = 0;
pthread_mutex_t textDrawMutex;

void Wrp_LoadTexture(const char *filepath);

char texPaths[][TEX_PATH_MAX_LEN] = {
	"romfs/icons/folder.png",
	"romfs/icons/file.png",
	"romfs/icons/image.png",
	"romfs/icons/video.png",
	"romfs/icons/audio.png"
};

void Wrp_InitSDL(const char *windowName) {
	SDL_Init(SDL_INIT_EVERYTHING);
	int width = WINDOW_WIDTH, height = WINDOW_HEIGHT;
	
	window = SDL_CreateWindow(windowName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN/* | SDL_WINDOW_MAXIMIZED*/);
	rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	
	TTF_Init();
	font = TTF_OpenFont("romfs/fonts/Arial.ttf", text_load_size);
	
	if (font == NULL)
		puts("Could not load font.");
	
	texture_array = malloc(1);
	Wrp_LoadTextures(texPaths, sizeof(texPaths)/sizeof(texPaths[0]));
	pthread_mutex_init(&textDrawMutex, NULL);
}

void Wrp_ExitSDL(void) {
	printf("exit sdl\n");
	TTF_CloseFont(font);
	TTF_Quit();
	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(rend);
	SDL_Quit();
}

void Wrp_LoadTextures(const char texPathsArr[][TEX_PATH_MAX_LEN], size_t rows) {
	for (size_t i = 0; i < rows; i++) {
		Wrp_LoadTexture(texPathsArr[i]);
	}
}

void Wrp_LoadTexture(const char *filePath) {
	SDL_Surface* image_sur;
	image_sur = IMG_Load(filePath);
	if (image_sur == NULL)
		printf("Could not load texture: %s\n", filePath);
	texture_array = realloc(texture_array, ++texArrayLen*sizeof(void*));
	texture_array[texArrayLen - 1] = SDL_CreateTextureFromSurface(rend, image_sur);
	SDL_FreeSurface(image_sur);
}

void Wrp_ClearScreen(void) {
	SDL_SetRenderDrawColor(rend, 0xff, 0xff, 0xff, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(rend);
}

void Wrp_DrawImage(int imageId, int x, int y, int width, int height) {
	if (width <= 0)
		width = Wrp_ImageWidth(imageId);
	if (height <= 0)
		height = Wrp_ImageHeight(imageId);
	SDL_Rect rect = {x, y, width, height};
	SDL_RenderCopy(rend, texture_array[imageId], NULL, &rect);
}

void Wrp_DrawTextWrap(const char *inputText, int x, int y, float textSize, int maxWidth) {
	Wrp_DrawTextWrapMaxHeight(inputText, x, y, textSize, maxWidth, 0);
}

void Wrp_DrawText(const char *inputText, int x, int y, float textSize) {
	Wrp_DrawTextWrap(inputText, x, y, textSize, 0);
}

void Wrp_DrawTextWrapMaxHeight_r(const char *inputText, int x, int y, float textSize, int maxWidth, int maxHeight, SDL_Renderer *r) {
	pthread_mutex_lock(&textDrawMutex); // ttf never bothered to allow multithreading
	
	SDL_Surface* textSurf;
	SDL_Texture* textTexture;
	SDL_Rect textRect;
	
	textSurf = TTF_RenderUTF8_Blended_Wrapped(font, inputText, (SDL_Color){0, 0, 0, 255}, maxWidth*(text_load_size/textSize));
	
	if (textSurf == NULL || textSurf->w <= 0 || textSurf->h <= 0) {
		goto textDrawEnd;
	}
	
	float textMultiple = textSize/text_load_size;
	int textWidth = textSurf->w*textMultiple;
	
	SDL_Rect sourceRect;
	
	if (maxHeight > 0) {
		sourceRect = (SDL_Rect){0, (textSurf->h < maxHeight/textMultiple) ? 0 : textSurf->h - (maxHeight/textMultiple), textSurf->w, textSurf->h};
		textRect = (SDL_Rect){x, y, textWidth, (textSurf->h*textMultiple < maxHeight) ? textSurf->h*textMultiple : maxHeight};
	} else {
		sourceRect = (SDL_Rect){0, 0, textSurf->w, textSurf->h};
		textRect = (SDL_Rect){x, y, textWidth, textSurf->h*textMultiple};
	}
	
	textTexture = SDL_CreateTextureFromSurface(r, textSurf);
	SDL_FreeSurface(textSurf);
	SDL_RenderCopy(r, textTexture, &sourceRect, &textRect);
	SDL_DestroyTexture(textTexture);
textDrawEnd:
	pthread_mutex_unlock(&textDrawMutex);
}

void Wrp_DrawTextWrap_r(const char *inputText, int x, int y, float textSize, int maxWidth, SDL_Renderer *r) {
	Wrp_DrawTextWrapMaxHeight_r(inputText, x, y, textSize, maxWidth, 0, r);
}

void Wrp_DrawText_r(const char *inputText, int x, int y, float textSize, SDL_Renderer *r) {
	Wrp_DrawTextWrap_r(inputText, x, y, textSize, 0, r);
}

void Wrp_DrawTextWrapMaxHeight(const char *inputText, int x, int y, float textSize, int maxWidth, int maxHeight) {
	Wrp_DrawTextWrapMaxHeight_r(inputText, x, y, textSize, maxWidth, maxHeight, rend);
}

int Wrp_DrawTextMaxWidth(const char *inputText, int x, int y, float textSize, int maxWidth, int flags) {
	pthread_mutex_lock(&textDrawMutex);
	SDL_Surface* textSurf;
	SDL_Texture* textTexture;
	SDL_Rect textRect;
	textSurf = TTF_RenderUTF8_Blended(font, inputText, (SDL_Color){0, 0, 0, 255});

	float textMultiple = textSurf->h/textSize;
	
	int textWidth = textSurf->w/textMultiple;
	int textHeight = textSize;

	
	SDL_Rect sourceRect = {(textSurf->w > maxWidth*textMultiple && flags == OFFSET_RIGHT) ? textSurf->w - (maxWidth*textMultiple) : 0, 0, maxWidth*textMultiple, textSurf->h}; // source rect is of rectangle source WITHOUT scaleing!!!!!!!! (float textSize). source is programmed to be in the start if the text is on screen, then move the window if the text is offscreen

	textRect = (SDL_Rect){x, y, (textSurf->w > maxWidth*textMultiple) ? (maxWidth) : textWidth, textHeight}; // width is textwidth until it goes offscreen. then it is max width.
	textTexture = SDL_CreateTextureFromSurface(rend, textSurf);
	int surfaceWidth = textSurf->w; // for return
	SDL_FreeSurface(textSurf);

	SDL_RenderCopy(rend, textTexture, &sourceRect, &textRect);
	SDL_DestroyTexture(textTexture);
	pthread_mutex_unlock(&textDrawMutex);
	return (surfaceWidth > maxWidth*textMultiple) ? maxWidth : textWidth;
}

int GetTextHeight(const char *inputText, float textSize, int maxWidth) {
	// todo
	return -9999;
}

void Wrp_FinishDrawing(void) {
	SDL_RenderPresent(rend);
}

void Wrp_DrawRectangle(int x, int y, int width, int height, int color) {
	SDL_Rect rect = {x, y, width, height};
	Wrp_SetRendColor(color);
	SDL_RenderFillRect(rend, &rect);
}

void Wrp_DrawRectangleNoFill(int x, int y, int width, int height, int color) {
	SDL_Rect rect = {x, y, width, height};
	Wrp_SetRendColor(color);
	SDL_RenderDrawRect(rend, &rect);
}

void Wrp_DrawImageWithDir(int imageId, int x, int y, int width, int height, bool flip) {
	SDL_RendererFlip flipType;
	if (flip)
		flipType = SDL_FLIP_HORIZONTAL;
	else
		flipType = SDL_FLIP_NONE;
	
	SDL_Rect rect = {x, y, width, height};
	
	SDL_RenderCopyEx(rend, texture_array[imageId], NULL/*src rect.. could be useful for spritesheet*/, &rect, 0, NULL, flipType);

}

int Wrp_ImageWidth(int imageId) {
	int x;
	SDL_QueryTexture(texture_array[imageId], NULL, NULL, &x, NULL);
	return x;
}

int Wrp_ImageHeight(int imageId) {
	int y;
	SDL_QueryTexture(texture_array[imageId], NULL, NULL, NULL, &y);
	return y;
}

inline void Wrp_ToggleWindowFullscreen(void) {
	if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
		SDL_SetWindowFullscreen(window, 0);
		SDL_ShowCursor(true);
	} else {
		SDL_ShowCursor(false);
		SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
	}
}

bool Wrp_IsFullscreen(void) {
	if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
		return true;
	} else {
		return false;
	}
}

void Wrp_DrawImageWithRotation(int imageId, int x, int y, int width, int height, int rotation) {
	SDL_Rect rect = {x, y, width, height};
	
	SDL_RenderCopyEx(rend, texture_array[imageId], NULL, &rect, rotation, NULL, SDL_FLIP_NONE);
}

void Wrp_DrawLine(int x1, int y1, int x2, int y2, int color) {
	Wrp_SetRendColor(color);
	SDL_RenderDrawLine(rend, x1, y1, x2, y2);
}

void Wrp_SetTitle(const char *title) {
	SDL_SetWindowTitle(window, title);
}

int Wrp_ScanInputs(void) {
	return 0;
}

void Wrp_GetWindowSize(int *w, int *h) {
	SDL_GetWindowSize(window, w, h);
}

int Wrp_GetWinWidth(void) {
	int width = 0;
	SDL_GetWindowSize(window, &width, NULL);
	return width;
}

int Wrp_GetWinHeight(void) {
	int height = 0;
	SDL_GetWindowSize(window, NULL, &height);
	return height;
}

void Wrp_DrawImageWithDirXY(int imageId, int x, int y, int width, int height, bool flipX, bool flipY) {
	int flipType = SDL_FLIP_NONE;
	if (flipX)
		flipType |= (SDL_RendererFlip)SDL_FLIP_HORIZONTAL;
	
	if (flipY)
		flipType |= (SDL_RendererFlip)SDL_FLIP_VERTICAL;
	
	SDL_Rect rect = {x, y, width, height};
	
	SDL_RenderCopyEx(rend, texture_array[imageId], NULL/*src rect.. could be useful for spritesheet*/, &rect, 0, NULL, (SDL_RendererFlip)flipType);
}

SDL_Window *Wrp_GetWindow() {
	return window;
}

void SetFullscreen(bool full) {
	SDL_SetWindowFullscreen(window, (full) ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

bool GetFullscreen(void) {
	return SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
}

const char *GetClipboardText(void) {
	
}

void Wrp_SetRendColor(int color) {
	switch (color) {
		case CLR_BLU:
			SDL_SetRenderDrawColor(rend, 0x00, 0x00, 0xff, SDL_ALPHA_OPAQUE);
			break;
		case CLR_WHT:
			SDL_SetRenderDrawColor(rend, 0xff, 0xff, 0xff, SDL_ALPHA_OPAQUE);
			break;
		case CLR_RED:
			SDL_SetRenderDrawColor(rend, 0xff, 0x00, 0x00, SDL_ALPHA_OPAQUE);
			break;
		case CLR_GRN:
			SDL_SetRenderDrawColor(rend, 0x00, 0xff, 0x00, SDL_ALPHA_OPAQUE);
			break;
		case CLR_BLK:
			SDL_SetRenderDrawColor(rend, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
			break;
		case CLR_YLW:
			SDL_SetRenderDrawColor(rend, 0xff, 0xff, 0x00, SDL_ALPHA_OPAQUE);
			break;
		case CLR_GRY:
			SDL_SetRenderDrawColor(rend, 0x80, 0x80, 0x80, SDL_ALPHA_OPAQUE);
			break;
		case CLR_LBLU:
			SDL_SetRenderDrawColor(rend, 135, 206, 235, SDL_ALPHA_OPAQUE);
			break;
		case CLR_LGRY:
			SDL_SetRenderDrawColor(rend, 0x90, 0x90, 0x90, SDL_ALPHA_OPAQUE);
			break;
	}
}

void Wrp_CheckDefault(void) { // prevent windows from marking window as unresponsive
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch(event.type) {
			case SDL_QUIT:
				exit(0);
				break;
		}
	}
}

void Wrp_DrawTextF_r(const char *inputText, int x, int y, float textSize, SDL_Renderer *r, ...) {
	va_list args;
	va_start(args, r);
	size_t needed = vsnprintf(NULL, 0, inputText, args);
	char *buffer = malloc(needed + 1);
	if (buffer == NULL)
		puts("COULD NOT ALLOCATE DRAWTEXTF BUFFER");
	vsprintf(buffer, inputText, args);
	va_end(args);
	Wrp_DrawText_r(buffer, x, y, textSize, r);
	free(buffer);
}

void Wrp_DrawTextF(const char *inputText, int x, int y, float textSize, ...) {
	va_list args;
	va_start(args, textSize);
	size_t needed = vsnprintf(NULL, 0, inputText, args);
	char *buffer = malloc(needed + 1);
	vsprintf(buffer, inputText, args);
	va_end(args);
	Wrp_DrawText(buffer, x, y, textSize);
	free(buffer);
}

SDL_Renderer *Wrp_GetMainRenderer(void) {
	return rend;
}
/*
void AttachMenu();

void AddEditorBar(void) {
	AttachMenu();
}

// small example for adding a file bar
HWND GetHwnd() {
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(Wrp_GetWindow(), &wmInfo);
	HWND win = wmInfo.info.win.window;
	if (win == NULL) {
		std::cout << "win is null!" << std::endl;
	}
	return win;
}

#define IDM_FILE_NEW 1
#define IDM_FILE_OPEN 2
#define IDM_FILE_QUIT 3
#define IDM_VIEW_STB 1
HMENU ghMenu;
HMENU hMenubar;
HMENU barElems[2]; 
void AttachMenu() {
    hMenubar = CreateMenu();
    ghMenu = CreateMenu();
	CreatePopupMenu();
    AppendMenuW(ghMenu, MF_STRING | MF_POPUP, IDM_VIEW_STB, L"&Save");
    AppendMenuW(ghMenu, MF_STRING, IDM_VIEW_STB, L"&Don't save");
    //CheckMenuItem(ghMenu, IDM_VIEW_STB, MF_CHECKED);  

	AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR) ghMenu, L"&File");
    SetMenu(GetHwnd(), hMenubar);
	//SetMenu(myWindow, myMenu);
}*/