#pragma once

#include <stddef.h>

#include <SDL2/SDL.h>

enum menu_types {
	MT_BUTTON,
	MT_INPUT,
	MT_IMAGE,
	MT_TEXT,
};

enum border_styles {
	BD_NONE,
	BD_SOLID,
	BD_DASHED,
	BD_INSET,
	BD_OUTSET,
	BD_DOTTED,
};

typedef struct ElementStyle {
	int x, y;
	int w, h;
	int borderColor;
	int hoverColor;
	int textHeight;
	int fgColor, bgColor;
	int bgImageId;
	enum border_styles borderStyle;
} Style_t;

struct ContextMenuElement; // forward declare for pointer

typedef struct {
	int nCtxElms;
	struct ElementStyle style;
	struct ContextMenuElement *elems;
} CtxMenu_t;

struct ContextMenuElement {
	enum ctx_elem_type {
		CE_POPOUT, // > popout more on hover/click
		CE_INFO, // do nothing on click
		CE_BUTTON, // do something
	} type;
	char *txt;
	void *onClick;
	CtxMenu_t pMenu; // recursive popout menu
	bool _beingHovered;
};

struct CustomMenu;

typedef struct MenuElement {
	enum menu_types type;
	char *text;
	char *id;
	void *onClick;
	void *onInput;
	void *onRemove;
	void *onHover;
	void *extraData; // random extra data
	bool _beingHovered;
	bool _hasCtxMenu;
	CtxMenu_t _ctxMenu;
	bool _ctxMenuActive;
	struct ElementStyle style;
	struct CustomMenu *parentMenu;
	bool _hasHoverBar;
	char *hoverBarText;
} MenuElem_t;

typedef struct CustomMenu {
	struct MenuElement *elems;
	size_t len;
	bool canScroll;
	int scroll;
	int ctxMenuIdx;
	int hoverBarIdx;
	SDL_Renderer *rend; // renderer to draw menu on (useful for multiple windows)
	Style_t style;
} Menu_t;

bool MenuInit(Menu_t *m);
bool MenuDraw(Menu_t *m);
bool MenuRun(Menu_t *m);
MenuElem_t *MenuAddButton(Menu_t *m, const char *id, int x, int y, int w, int h, const char *t);
MenuElem_t *MenuAddText(Menu_t *m, const char *id, int x, int y, const char *t, int h);
struct MenuElement *MenuGetElement(Menu_t *m, const char *id);
struct MenuElement *MenuGetLastElement(Menu_t *m);
void MenuFree(Menu_t *m);
void MenuRemoveElement(Menu_t *m, const char *id);
void MenuReset(Menu_t *m);
void MenuToggleScroll(Menu_t *m, bool t);
void MenuSetBackgroundImage(MenuElem_t *e, int bgId);
void MenuRemoveElementIdx(Menu_t *m, int idx);
void ElementAddContextMenu(MenuElem_t *e, size_t n, const struct ContextMenuElement c[]);
void MenuSetRenderer(Menu_t *m, SDL_Renderer *r);
void ElementSetHoverBar(MenuElem_t *e, const char *text);
const char *EvalStringFormat(const char *fmt, ...);
// special funcs
char *CreateDialoguePopup(const char *title, const char *msg, int numButtons, ...);
char *CreateFileDialoguePopup(void);