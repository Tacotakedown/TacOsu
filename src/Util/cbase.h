#ifndef CBASE_H
#define CBASE_H

#include <math.h>
#include <vector>
#include <stack>
#include <string>
#include <random>
#include <memory>
#include <atomic>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include <fstream>
#include <iostream>

#include <cstdarg>
#include <cwctype>



#include "FastDelegate/FastDelegate.h"

#include "Graphics/Graphics.h"
#include "Environment/Environment.h"
#include "input/KeyboardEvent/KeyboardEvent.h"

#include "Vectors/Vectors.h"
#include "Matrices/Matrices.h"
#include "UString/Ustring.h"
#include "Rect/Rect.h"



#ifndef NULL
#define NULL nullptr
#endif

typedef unsigned char COLORPART;

#define SAFE_DELETE(p) { if(p) { delete (p); (p) = NULL; } }

#define COLOR(a,r,g,b) \
    ((Color)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))

#define COLORf(a,r,g,b) \
    ((Color)(((((int)( clamp<float>(a,0.0f,1.0f)*255.0f ))&0xff)<<24)|((((int)( clamp<float>(r,0.0f,1.0f)*255.0f ))&0xff)<<16)|((((int)( clamp<float>(g,0.0f,1.0f)*255.0f ))&0xff)<<8)|(((int)( clamp<float>(b,0.0f,1.0f)*255.0f ))&0xff)))

#define COLOR_GET_Ri(color) \
	(((COLORPART)(color >> 16)))

#define COLOR_GET_Gi(color) \
	(((COLORPART)(color >> 8)))

#define COLOR_GET_Bi(color) \
	(((COLORPART)(color >> 0)))

#define COLOR_GET_Ai(color) \
	(((COLORPART)(color >> 24)))

#define COLOR_GET_Rf(color) \
	(((COLORPART)(color >> 16))  / 255.0f)

#define COLOR_GET_Gf(color) \
	(((COLORPART)(color >> 8)) / 255.0f)

#define COLOR_GET_Bf(color) \
	(((COLORPART)(color >> 0)) / 255.0f)

#define COLOR_GET_Af(color) \
	(((COLORPART)(color >> 24)) / 255.0f)

#define COLOR_INVERT(color) \
	(COLOR(255, 255-COLOR_GET_Ri(color), 255-COLOR_GET_Gi(color), 255-COLOR_GET_Bi(color)))

#define COLOR_MULTIPLY(color1, color2) \
	(COLORf(1.0f, COLOR_GET_Rf(color1)*COLOR_GET_Rf(color2), COLOR_GET_Gf(color1)*COLOR_GET_Gf(color2), COLOR_GET_Bf(color1)*COLOR_GET_Bf(color2)))

#define COLOR_ADD(color1, color2) \
	(COLORf(1.0f, clamp<float>(COLOR_GET_Rf(color1)+COLOR_GET_Rf(color2),0.0f,1.0f), clamp<float>(COLOR_GET_Gf(color1)+COLOR_GET_Gf(color2),0.0f,1.0f), clamp<float>(COLOR_GET_Bf(color1)+COLOR_GET_Bf(color2),0.0f,1.0f)))

#define COLOR_SUBTRACT(color1, color2) \
	(COLORf(1.0f, clamp<float>(COLOR_GET_Rf(color1)-COLOR_GET_Rf(color2),0.0f,1.0f), clamp<float>(COLOR_GET_Gf(color1)-COLOR_GET_Gf(color2),0.0f,1.0f), clamp<float>(COLOR_GET_Bf(color1)-COLOR_GET_Bf(color2),0.0f,1.0f)))



#define PI 3.1415926535897932384626433832795

#define PIOVER180 0.01745329251994329576923690768489


template <class T>
inline T clamp(T x, T a, T b)
{
	return x < a ? a : (x > b ? b : x);
}

template <class T>
inline T lerp(T x1, T x2, T percent)
{
	return x1 * (1 - percent) + x2 * percent;
}

template <class T>
inline int sign(T val)
{
	return val > 0 ? 1 : (val == 0 ? 0 : -1);
}

inline float deg2rad(float deg)
{
	return deg * PI / 180.0f;
}

inline float rad2deg(float rad)
{
	return rad * 180.0f / PI;
}

inline bool isInt(float f)
{
	return (f == static_cast<float>(static_cast<int>(f)));
}



// ANSI/IEEE 754-1985

inline unsigned long& floatBits(float& f)
{
	return *reinterpret_cast<unsigned long*>(&f);
}

inline bool isFinite(float f)
{
	return ((floatBits(f) & 0x7F800000) != 0x7F800000);
}

#endif // CBASE_H
