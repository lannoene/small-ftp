#include "menu.h"

#include "sdlwrp.h"
#include "address_dbg.h"

#define DEFAULT_FONT_HEIGHT 22
#define DEFAULT_CTX_MENU_WIDTH 150
#define AABB(x1, y1, width1, height1, x2, y2, width2, height2)\
	(y1 + height1 > y2 \
		&& y1 < y2 + height2 \
		&& x1 + width1 > x2 \
		&& x1 < x2 + width2)

bool MenuInit(Menu_t *m) {
	memset(m, 0, sizeof(Menu_t));
	m->elems = malloc(1);
	if (!m->elems) {
		puts("Could not malloc menu elements.");
		return false;
	}
	m->rend = Wrp_GetMainRenderer();
	return true;
}

static inline void MenuAddElement(Menu_t *m, struct MenuElement e) {
	m->elems = realloc(m->elems, ++m->len*sizeof(e));
	m->elems[m->len - 1] = e;
	m->ctxMenuIdx = -1;
	m->hoverBarIdx = -1;
}

static void MenuElementInit(Menu_t *m, struct MenuElement *e) {
	memset(e, 0, sizeof(struct MenuElement));
	e->style.textHeight = DEFAULT_FONT_HEIGHT;
	e->style.bgImageId = -1;
	e->style.borderStyle = BD_SOLID;
	e->style.borderColor = CLR_BLK;
	e->style.bgColor = CLR_WHT;
	e->style.hoverColor = CLR_WHT;
	e->parentMenu = m;
}

static int MenuGetElementIndex(Menu_t *m, const char *id) {
	int idx = -1;
	for (size_t i = 0; i < m->len; i++) {
		if (m->elems[i].id != NULL) {
			if (strcmp(m->elems[i].id, id) == 0) {
				idx = i;
			}
		}
	}
	return idx;
}

MenuElem_t *MenuAddButton(Menu_t *m, const char *id, int x, int y, int w, int h, const char *t) {
	struct MenuElement e;
	MenuElementInit(m, &e);
	e.style.x = x;
	e.style.y = y;
	e.style.w = w;
	e.style.h = h;
	e.text = strdup(t);
	e.type = MT_BUTTON;
	if (id != NULL)
		e.id = strdup(id);
	MenuAddElement(m, e);
	return MenuGetLastElement(m);
}

MenuElem_t *MenuAddText(Menu_t *m, const char *id, int x, int y, const char *t, int h) {
	struct MenuElement e;
	MenuElementInit(m, &e);
	e.style.x = x;
	e.style.y = y;
	e.text = strdup(t);
	e.type = MT_TEXT;
	if (id != NULL)
		e.id = strdup(id);
	MenuAddElement(m, e);
	return MenuGetLastElement(m);
}

void ElementAddContextMenu(MenuElem_t *e, size_t n, const struct ContextMenuElement c[]) {
	e->_ctxMenu.elems = malloc(n*sizeof(struct ContextMenuElement));
	memcpy(e->_ctxMenu.elems, c, n*sizeof(struct ContextMenuElement));
	e->_hasCtxMenu = true;
	e->_ctxMenu.nCtxElms = n;
}

struct MenuElement *MenuGetElement(Menu_t *m, const char *id) {
	for (size_t i = 0; i < m->len; i++) {
		if (m->elems[i].id != NULL) {
			if (strcmp(m->elems[i].id, id) == 0) {
				return &m->elems[i];
			}
		}
	}
	return NULL;
}

// menu draw

static void MenuDrawButton(Menu_t *m, struct MenuElement e) {
	struct ElementStyle s = e.style;
	Wrp_DrawRectangle(s.x, s.y + m->scroll, s.w, s.h, s.bgColor);
	if (e._beingHovered) {
		Wrp_DrawRectangle(s.x, s.y + m->scroll, s.w, s.h, s.hoverColor);
	}
	if (e.style.bgImageId != -1) {
		Wrp_DrawImage(s.bgImageId, s.x, s.y + m->scroll, s.w, s.h);
	}
	switch (s.borderStyle) {
		default:
			printf("Unknown border style property: %d\n", s.borderStyle);
			break;
		case BD_NONE:break;
		case BD_SOLID:
			Wrp_DrawRectangleNoFill(s.x, s.y + m->scroll, s.w, s.h, s.borderColor);
			break;
	}
	if (e.text != NULL) {
		Wrp_DrawTextWrap(e.text, s.x, s.y + m->scroll, s.textHeight, s.w);
	}
}

static void MenuDrawText(Menu_t *m, struct MenuElement e) {
	struct ElementStyle s = e.style;
	if (e.text != NULL)
		Wrp_DrawText(e.text, s.x, s.y + m->scroll, s.textHeight);
}

[[maybe_unused]]
static void DrawImage(struct MenuElement e) {
	
}

[[maybe_unused]]
static void DrawInput(struct MenuElement e) {
	
}

void MenuDrawContextMenu(MenuElem_t *e) {
	CtxMenu_t *c = &e->_ctxMenu;
	Wrp_DrawRectangle(e->_ctxMenu.style.x, e->_ctxMenu.style.y, DEFAULT_CTX_MENU_WIDTH, 100, CLR_WHT);
	Wrp_DrawRectangleNoFill(e->_ctxMenu.style.x, e->_ctxMenu.style.y, DEFAULT_CTX_MENU_WIDTH, 100, CLR_BLK);
	
	for (int i = 0; i < c->nCtxElms; i++) {
		if (c->elems[i]._beingHovered)
			Wrp_DrawRectangle(e->_ctxMenu.style.x, e->_ctxMenu.style.y + 5 + 17*i, DEFAULT_CTX_MENU_WIDTH, 17, CLR_LGRY);
		Wrp_DrawText(c->elems[i].txt, e->_ctxMenu.style.x, e->_ctxMenu.style.y + 5 + 17*i, 15);
	}
}

void MenuDrawHoverMenu(MenuElem_t *e) {
	Wrp_DrawRectangle(0, WINDOW_HEIGHT - 25, 200, 25, CLR_WHT);
	Wrp_DrawRectangleNoFill(0, WINDOW_HEIGHT - 25, 200, 25, CLR_BLK);
	
	Wrp_DrawText_r(e->hoverBarText, 0, WINDOW_HEIGHT - 24, 20, e->parentMenu->rend);
}

bool MenuDraw(Menu_t *m) {
	for (size_t i = 0; i < m->len; i++) {
		struct MenuElement *e = &m->elems[i];
		switch (e->type) {
			default:
				printf("Unrecognized menu element type: %d\n", m->elems[i].type);
				break;
			case MT_BUTTON:
				MenuDrawButton(m, *e);
				break;
			case MT_TEXT:
				MenuDrawText(m, *e);
				break;
			case MT_INPUT:
				
				break;
		}
	}
	
	if (m->hoverBarIdx != -1)
		MenuDrawHoverMenu(&m->elems[m->hoverBarIdx]);
	
	if (m->ctxMenuIdx != -1)
		MenuDrawContextMenu(&m->elems[m->ctxMenuIdx]);
	
	return true;
}

// menu run

bool MenuRun(Menu_t *m) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				return false;
			case SDL_WINDOWEVENT: {
				if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
					extern bool fileInfoOpen;
					extern SDL_Window *win;
					fileInfoOpen = false;
					SDL_DestroyWindow(win);
				}
			}
			case SDL_KEYDOWN:
				switch (event.key.keysym.scancode) {
					
				}
				break;
			case SDL_MOUSEBUTTONDOWN: {
				break;
			}
			case SDL_MOUSEBUTTONUP: {
				if (event.button.windowID != 1)
					continue;
				int x = event.button.x, y = event.button.y;
				if (m->ctxMenuIdx != -1) { // register clicks for ctx menu first
					struct MenuElement *e = &m->elems[m->ctxMenuIdx];
					if (AABB(e->_ctxMenu.style.x, e->_ctxMenu.style.y, DEFAULT_CTX_MENU_WIDTH, 100, x, y, 0, 0)) {
						CtxMenu_t *c = &e->_ctxMenu;
						for (int i = 0; i < c->nCtxElms; i++) {
							if (AABB(c->style.x, c->style.y + 5 + 17*i, DEFAULT_CTX_MENU_WIDTH, 15, x, y, 0, 0)) {
								switch (c->elems[i].type) {
									default:
										printf("Unknown ctx type: %d\n", c->elems[i].type);
										break;
									case CE_POPOUT: // oh boy...
										break;
									case CE_BUTTON:
										if (c->elems[i].onClick != NULL) {
											((void(*)(struct MenuElement*, int, int))c->elems[i].onClick)(e, x, y);
											m->ctxMenuIdx = -1; // close ctx menu
										}
										break;
									case CE_INFO:break;
								}
							}
						}
						break;
					}
				}
				
				m->ctxMenuIdx = -1;
				for (size_t i = 0; i < m->len; i++) {
					struct MenuElement *e = &m->elems[i];
					e->_beingHovered = false;
					if (AABB(e->style.x, e->style.y + m->scroll, e->style.w, e->style.h, x, y, 0, 0)) {
						if (event.button.button == 1) { // left click
							e->_ctxMenuActive = false;
							e->_beingHovered = true;
							if (e->onClick != NULL) {
								((void(*)(struct MenuElement*, int, int))e->onClick)(e, x, y);
								break;
							}
						} else if (event.button.button == 3) { // right click
							// bring up context menu (idek what that is im just calling it that)
							if (e->_hasCtxMenu) {
								e->_ctxMenuActive = true;
								e->_ctxMenu.style.x = x;
								e->_ctxMenu.style.y = y;
								m->ctxMenuIdx = i;
							}
							e->_beingHovered = true;
						}
					}
				}
				break;
			}
			case SDL_MOUSEWHEEL: {
				if (event.wheel.windowID != 1)
					continue;
				if (m->canScroll)
					m->scroll += (event.wheel.preciseY)*20;
				break;
			}
			case SDL_MOUSEMOTION: {
				if (event.motion.windowID != 1)
					continue;
				int x = event.motion.x, y = event.motion.y;
				// look for ctx menu first (cuz it's on top)
				
				if (m->ctxMenuIdx != -1) { // register clicks for ctx menu first
					struct MenuElement *e = &m->elems[m->ctxMenuIdx];
					CtxMenu_t *c = &e->_ctxMenu;
					for (int i = 0; i < c->nCtxElms; i++) {
						c->elems[i]._beingHovered = false;
						if (AABB(c->style.x, c->style.y + 5 + 17*i, DEFAULT_CTX_MENU_WIDTH, 15, x, y, 0, 0)) {
							if (c->elems[i].type != CE_INFO)
								c->elems[i]._beingHovered = true;
						}
					}
					break; // if ctx is open, don't update hover to new element
				}
				
				m->hoverBarIdx = -1;
				for (size_t i = 0; i < m->len; i++) {
					struct MenuElement *e = &m->elems[i];
					if (AABB(e->style.x, e->style.y + m->scroll, e->style.w, e->style.h, x, y, 0, 0)) {
						if (e->_hasHoverBar)
							m->hoverBarIdx = i;
						e->_beingHovered = true;
						if (e->onHover != NULL)
							((void(*)(struct MenuElement*, int, int))e->onHover)(e, x, y); // remember: NEVER PERFORM ACTIONS AFTER CALLBACK
					} else {
						e->_beingHovered = false;
					}
				}
			}
		}
	}
	return true;
}

// menu free
void MenuFree(Menu_t *m) {
	for (int i = m->len - 1; i >= 0; i--) { // doing it from the end to the start means i won't have to shift everything over
		MenuRemoveElementIdx(m, i);
	}
	free(m->elems);
	m->len = -1;
}

void MenuReset(Menu_t *m) {
	MenuFree(m);
	MenuInit(m);
}

void MenuRemoveElement(Menu_t *m, const char *id) {
	int idx;
	if ((idx = MenuGetElementIndex(m, id)) == -1)
		return;
	
	MenuRemoveElementIdx(m, idx);
}

struct MenuElement *MenuGetLastElement(Menu_t *m) {
	return &m->elems[m->len - 1];
}

void MenuToggleScroll(Menu_t *m, bool t) {
	m->canScroll = t;
}

void MenuSetBackgroundImage(MenuElem_t *e, int bgId) {
	e->style.bgImageId = bgId;
}

void MenuRemoveElementIdx(Menu_t *m, int idx) {
	if (m->elems[idx].onRemove != NULL) // call destructor
		((void(*)(MenuElem_t*))m->elems[idx].onRemove)(&m->elems[idx]);
	
	// now free everything
	if (m->elems[idx].id != NULL)
		free(m->elems[idx].id);
	
	if (m->elems[idx].text != NULL)
		free(m->elems[idx].text);
	
	if (m->elems[idx].hoverBarText != NULL)
		free(m->elems[idx].hoverBarText);
	
	for (size_t i = idx; i < m->len - 1; i++) { // now shift elements over cur idx 1 place forward (which erases cur idx in the process, leaving a duplicate at the end which realloc releases)
		m->elems[i] = m->elems[i + 1];
	}
	--m->len;
	m->elems = realloc(m->elems, m->len*sizeof(struct MenuElement) + ((m->len == 0) ? 1 : 0));
}

void MenuSetRenderer(Menu_t *m, SDL_Renderer *r) {
	m->rend = r;
}

void ElementSetHoverBar(MenuElem_t *e, const char *text) {
	e->_hasHoverBar = true;
	e->hoverBarText = strdup(text);
}

const char *EvalStringFormat(const char *fmt, ...) { // this is not thread safe... hm...
	#define MAX_EVAL_STRING_LEN 500
	va_list args;
	va_start(args, fmt);
	static char buffer[MAX_EVAL_STRING_LEN];
	vsnprintf(buffer, MAX_EVAL_STRING_LEN, fmt, args);
	va_end(args);
	return buffer;
}

// ----------

#include <windows.h>

bool done = true;
char contents[100];
LRESULT CALLBACK WndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	// textbox params
	static int numCharsWritten = 0;
	static HWND textbox;
	static HWND buttonOk;
	static HWND buttonCancel;
    switch(Msg) {
		case WM_CREATE:
			textbox = CreateWindow("EDIT", "", WS_BORDER | WS_CHILD | WS_VISIBLE, 10, 10, 400, 20, hwnd, NULL, NULL, NULL);
			buttonOk = CreateWindow("BUTTON", "OK", WS_VISIBLE | WS_CHILD | WS_BORDER, 420, 10, 40, 20, hwnd, (HMENU)1, NULL, NULL);
			buttonCancel = CreateWindow("BUTTON", "Cancel", WS_VISIBLE | WS_CHILD | WS_BORDER, 420, 40, 40, 20, hwnd, (HMENU)2, NULL, NULL);
			break;
		case WM_DESTROY:
			done = true;
			DestroyWindow(hwnd);
			break;
		case WM_PAINT: {
				/* */
			PAINTSTRUCT ps;
			HDC         hdc;
			RECT        rc;
			hdc = BeginPaint(hwnd, &ps);

			GetClientRect(hwnd, &rc);
			SetTextColor(hdc, 0);
			SetBkMode(hdc, TRANSPARENT);
			DrawText(hdc, contents, numCharsWritten, &rc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

			EndPaint(hwnd, &ps);
			/* */
			break;
		}
		case WM_CHAR: {
			contents[numCharsWritten++] = wParam;
			contents[numCharsWritten] = '\0';
			break;
		}
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case 1: // OK
					puts("Ok");
					int gwstat = 0;
					gwstat = GetWindowText(textbox, contents, 100);
					DestroyWindow(hwnd);
					done = true;
					break;
				case 2: // CANCEL
					puts("Cancel");
					DestroyWindow(hwnd);
					done = true;
					break;
			}
			break;
		default:
			return DefWindowProc(hwnd, Msg, wParam, lParam);
		}
    return 0;
}

char *CreateDialoguePopup(const char *title, const char *msg, int numButtons, ...) {
	HWND hwnd;
	if (!(hwnd = CreateWindowEx(0,
                          "DialogueBox",
                          title,
                          WS_POPUPWINDOW | WS_CAPTION,
                          100,
                          120,
                          600,
                          100,
                          NULL,
                          NULL,
                          GetModuleHandle(NULL),
                          NULL)))
		puts("Could not create popup");
	done = false;
	ShowWindow(hwnd, SW_NORMAL);
	UpdateWindow(hwnd);
	SetForegroundWindow(hwnd);
	while (!done) {
		Wrp_CheckDefault();
	}
	return strdup(contents);
}

#include <SDL_syswm.h>

HWND GetHwnd() {
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(Wrp_GetWindow(), &wmInfo);
	HWND win = wmInfo.info.win.window;
	if (win == NULL) {
		puts("Win is null!");
	}
	return win;
}

char *CreateFileDialoguePopup(void) {
	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetHwnd();
	ofn.hInstance = GetModuleHandle(NULL);
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = L"Please Select A File To Open";
	ofn.Flags = OFN_NONETWORKBUTTON | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	char *f = malloc(500);
	f[0] = '\0';
	ofn.lpstrFile = f;
	
	if (!GetOpenFileName(&ofn)) {
		return NULL;
	}
	return ofn.lpstrFile;
}