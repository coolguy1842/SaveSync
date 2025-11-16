#ifndef __CLAY_RENDERER_C2D_HPP__
#define __CLAY_RENDERER_C2D_HPP__

#include <citro2d.h>
#include <clay.h>

#include <vector>

#define TOP_SCREEN_WIDTH  GSP_SCREEN_HEIGHT_TOP
#define TOP_SCREEN_HEIGHT GSP_SCREEN_WIDTH

#define BOTTOM_SCREEN_WIDTH  GSP_SCREEN_HEIGHT_BOTTOM
#define BOTTOM_SCREEN_HEIGHT GSP_SCREEN_WIDTH

struct Clay_C2DRendererData {
    std::vector<C2D_Font> fonts;
};

enum CustomElementType {
    CUSTOM_ELEMENT_TYPE_CIRCLE = 0,
    CUSTOM_ELEMENT_TYPE_POLYGON
};

// vectors clamped from 0 to 1
struct PolygonData {
    Clay_Vector2 a;
    Clay_Vector2 b;
    Clay_Vector2 c;
};

struct CustomElementData {
    CustomElementType type;
    union {
        PolygonData polygon;
    };
};

void C2D_Clay_Init();
void C2D_Clay_Exit();

void C2D_Clay_ScissorStart(gfxScreen_t screen, Clay_BoundingBox box, GPU_SCISSORMODE mode = GPU_SCISSOR_NORMAL);
void C2D_Clay_ScissorEnd();

bool C2D_Clay_InView(Clay_BoundingBox bounds);

void C2D_Clay_Arc(const float centerX, const float centerY, const float radius, const float startAngle, const float endAngle, const float thickness, const Clay_Color color);
void C2D_Clay_FilledArc(const float centerX, const float centerY, const float radius, const float startAngle, const float endAngle, const Clay_Color color);
void C2D_Clay_Rect(const Clay_BoundingBox bounds, const Clay_CornerRadius radius, const Clay_Color color);
void C2D_Clay_Square(const Clay_BoundingBox bounds, const Clay_CornerRadius radius, const Clay_BorderWidth thickness, const Clay_Color color);

Clay_Dimensions C2D_Clay_MeasureText(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData);
void C2D_Clay_RenderClayCommands(Clay_C2DRendererData* rendererData, Clay_RenderCommandArray rcommands, gfxScreen_t screen);

#endif