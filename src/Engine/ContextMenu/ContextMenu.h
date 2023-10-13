#ifndef CONTEXTMENU_H
#define CONTEXTMENU_H

#include "cbase.h"

class ContextMenu {
public:
	ContextMenu();
	virtual ~ContextMenu();

	static ContextMenu* get();

	virtual void begin() = 0;
	virtual void addItem(UString text, int returnValue) = 0;
	virtual void addSeperator() = 0;
	virtual void end() = 0;
};

extern ContextMenu* cmenu;

#endif // !CONTEXTMENU_H
