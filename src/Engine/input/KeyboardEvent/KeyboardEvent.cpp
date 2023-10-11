#include "KeyboardEvent.h"

KeyboardEvent::KeyboardEvent(KEYCODE keyCode)
{
	m_keyCode = keyCode;
	m_bConsumed = false;
}

void KeyboardEvent::consume()
{
	m_bConsumed = true;
}