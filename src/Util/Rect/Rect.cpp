#include "Rect.h"

Rects::Rects(float x, float y, float width, float height, bool isCentered)
{
	set(x, y, width, height, isCentered);
}

Rects::Rects(Vector2 pos, Vector2 size, bool isCentered)
{
	set(pos.x, pos.y, size.x, size.y, isCentered);
}

void Rects::set(float x, float y, float width, float height, bool isCentered)
{
	if (isCentered)
	{
		m_fMinX = x - (width / 2.0f);
		m_fMaxX = x + (width / 2.0f);
		m_fMinY = y - (height / 2.0f);
		m_fMaxY = y + (height / 2.0f);
	}
	else
	{
		m_fMinX = x;
		m_fMaxX = x + width;
		m_fMinY = y;
		m_fMaxY = y + height;
	}
}

Rects Rects::intersect(const Rects& rect) const {
	Rects intersection;

	intersection.m_fMinX = std::max(m_fMinX, rect.m_fMinX);
	intersection.m_fMinY = std::max(m_fMinY, rect.m_fMinY);
	intersection.m_fMaxX = std::min(m_fMaxX, rect.m_fMaxX);
	intersection.m_fMaxY = std::min(m_fMaxY, rect.m_fMaxY);

	// if the rects don't intersect
	if (intersection.m_fMinX > intersection.m_fMaxX || intersection.m_fMinY > intersection.m_fMaxY)
	{
		// then reset the rect to a null rect
		intersection.m_fMinX = 0.0f;
		intersection.m_fMaxX = 0.0f;
		intersection.m_fMinY = 0.0f;
		intersection.m_fMaxY = 0.0f;
	}

	return intersection;
}

Rects Rects::Union(const Rects& rect) const {
	Rects Union;

	Union.m_fMinX = std::min(m_fMinX, rect.m_fMinX);
	Union.m_fMinY = std::min(m_fMinY, rect.m_fMinY);
	Union.m_fMaxX = std::max(m_fMaxX, rect.m_fMaxX);
	Union.m_fMaxY = std::max(m_fMaxY, rect.m_fMaxY);

	return Union;
}

bool Rects::intersects(const Rects& rect) const {
	const float minx = std::max(m_fMinX, rect.m_fMinX);
	const float miny = std::max(m_fMinY, rect.m_fMinY);
	const float maxx = std::min(m_fMaxX, rect.m_fMaxX);
	const float maxy = std::min(m_fMaxY, rect.m_fMaxY);

	return (minx < maxx && miny < maxy);
}

Rects& Rects::operator = (const Rects& rect) {
	m_fMinX = rect.m_fMinX;
	m_fMaxX = rect.m_fMaxX;
	m_fMinY = rect.m_fMinY;
	m_fMaxY = rect.m_fMaxY;

	return *this;
}