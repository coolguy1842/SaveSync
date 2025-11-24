#include <clay_renderer_C2D.hpp>
#include <map>
#include <memory>

#define C2D_CLAY_COLOR(color) C2D_Color32(color.r, color.g, color.b, color.a);

#define DEG_RAD(deg) deg*(M_PI / 180.0f)

constexpr uint32_t hashStr(const char* str, size_t len) {
    uint32_t hash = 5381;
    for(size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + static_cast<uint8_t>(str[i]);
    }

    return hash;
}

struct CacheEntry {
    C2D_Text text;
    uint32_t textHash;
};

// key is clay id, use ptr to clear memory fully
static std::map<uint32_t, CacheEntry> s_textCache;

void C2D_Clay_Init() {
    gfxInitDefault();

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);

    C2D_Prepare();
}

void C2D_Clay_Exit() {
    for(auto entry : s_textCache) {
        if(entry.second.text.buf == nullptr) {
            continue;
        }

        C2D_TextBufDelete(entry.second.text.buf);
    }

    s_textCache.clear();

    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

constexpr bool C2D_Clay_Intersects(Clay_BoundingBox a, Clay_BoundingBox b) {
    return (a.x <= (b.x + b.width) && b.x <= (a.x + a.width)) &&
           (a.y <= (b.y + b.height) && b.y <= (a.y + a.height));
}

void C2D_Clay_ScissorStart(gfxScreen_t screen, Clay_BoundingBox box, GPU_SCISSORMODE mode) {
    float width, height;
    switch(screen) {
    case GFX_TOP:
        width  = TOP_SCREEN_WIDTH;
        height = TOP_SCREEN_HEIGHT;
        break;
    case GFX_BOTTOM:
        width  = BOTTOM_SCREEN_WIDTH;
        height = BOTTOM_SCREEN_HEIGHT;
        break;
    default: return;
    }

    Clay_BoundingBox scissor = {
        height - box.height - box.y,
        width - box.width - box.x,
        height - box.y,
        width - box.x
    };

    C2D_Flush();
    C3D_SetScissor(mode, scissor.x, scissor.y, scissor.width, scissor.height);
}

void C2D_Clay_ScissorEnd() {
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

const int NUM_CIRCLE_SEGMENTS = 16;
void C2D_Clay_Arc(const float centerX, const float centerY, const float radius, const float startAngle, const float endAngle, const float thickness, const Clay_Color color) {
    const u32 clr      = C2D_CLAY_COLOR(color);
    const u32 segments = CLAY__MAX(NUM_CIRCLE_SEGMENTS, radius * 1.5f);

    const float innerRadius = radius - thickness / 2.0f;
    const float outerRadius = radius + thickness / 2.0f;

    float step  = DEG_RAD((endAngle - startAngle) / segments);
    float angle = DEG_RAD(startAngle);

    float prevXInner = -1, prevYInner = -1;
    float prevXOuter = -1, prevYOuter = -1;

    for(u32 i = 0; i <= segments; ++i) {
        float xInner = centerX + innerRadius * cos(angle), yInner = centerY + innerRadius * sin(angle);
        float xOuter = centerX + outerRadius * cos(angle), yOuter = centerY + outerRadius * sin(angle);
        angle += step;

        if(prevXInner != -1 && prevYInner != -1 && prevXOuter != -1 && prevYOuter != -1) {
            C2D_DrawTriangle(xInner, yInner, clr, prevXInner, prevYInner, clr, prevXOuter, prevYOuter, clr, 0.0f);
            C2D_DrawTriangle(xInner, yInner, clr, prevXOuter, prevYOuter, clr, xOuter, yOuter, clr, 0.0f);
        }

        prevXInner = xInner;
        prevYInner = yInner;
        prevXOuter = xOuter;
        prevYOuter = yOuter;
    }
}

void C2D_Clay_FilledArc(const float centerX, const float centerY, const float radius, const float startAngle, const float endAngle, const Clay_Color color) {
    const u32 clr      = C2D_CLAY_COLOR(color);
    const u32 segments = CLAY__MAX(NUM_CIRCLE_SEGMENTS, radius * 1.5f);

    float angle = DEG_RAD(startAngle);
    float step  = DEG_RAD((endAngle - startAngle) / segments);

    float prevX = -1, prevY = -1;
    for(u32 i = 0; i <= segments; i++) {
        float x = centerX + radius * cos(angle);
        float y = centerY + radius * sin(angle);
        angle += step;

        if(prevX != -1 && prevY != -1) {
            C2D_DrawTriangle(centerX, centerY, clr, x, y, clr, prevX, prevY, clr, 0.f);
        }

        prevX = x;
        prevY = y;
    }
}

void C2D_Clay_Rect(const Clay_BoundingBox bounds, const Clay_CornerRadius radius, const Clay_Color color) {
    const u32 clr = C2D_CLAY_COLOR(color);
    if(radius.topLeft == 0 && radius.topRight == 0 && radius.bottomLeft == 0 && radius.bottomRight == 0) {
        C2D_DrawRectSolid(bounds.x, bounds.y, 0.0, bounds.width, bounds.height, clr);
        return;
    }

    const float top    = CLAY__MAX(radius.topLeft, radius.topRight);
    const float bottom = CLAY__MAX(radius.bottomLeft, radius.bottomRight);

    const float topX    = bounds.x + radius.topLeft;
    const float bottomX = bounds.x + radius.bottomLeft;
    const float y       = bounds.y + top;

    const float topWidth    = bounds.width - (radius.topLeft + radius.topRight);
    const float bottomWidth = bounds.width - (radius.bottomLeft + radius.bottomRight);

    const float height = bounds.height - (top + bottom);

    if(radius.topLeft != 0) C2D_Clay_FilledArc(topX, y, radius.topLeft, 180.0f, 270.0f, color);              // top left
    C2D_DrawRectSolid(topX, bounds.y, 0.0f, topWidth, top, clr);                                             // top
    if(radius.topRight != 0) C2D_Clay_FilledArc(topX + topWidth, y, radius.topRight, 270.0f, 360.0f, color); // top right

    C2D_DrawRectSolid(bounds.x, y, 0.0, bounds.width, height, clr); // center

    if(radius.bottomLeft != 0) C2D_Clay_FilledArc(bottomX, y + height, radius.bottomLeft, 90.0f, 180.0f, color);               // bottom left
    C2D_DrawRectSolid(bottomX, y + height, 0.0f, bottomWidth, bottom, clr);                                                    // bottom
    if(radius.bottomRight != 0) C2D_Clay_FilledArc(bottomX + bottomWidth, y + height, radius.bottomRight, 0.0f, 90.0f, color); // bottom right
}

void C2D_Clay_Square(const Clay_BoundingBox bounds, const Clay_CornerRadius radius, const Clay_BorderWidth thickness, const Clay_Color color) {
    const u32 clr = C2D_CLAY_COLOR(color);

    const float minRadius                = CLAY__MIN(bounds.width, bounds.height) / 2.0f;
    const Clay_CornerRadius clampedRadii = {
        .topLeft     = CLAY__MIN(radius.topLeft, minRadius),
        .topRight    = CLAY__MIN(radius.topRight, minRadius),
        .bottomLeft  = CLAY__MIN(radius.bottomLeft, minRadius),
        .bottomRight = CLAY__MIN(radius.bottomRight, minRadius)
    };

    if(thickness.left > 0) {
        const float startY = bounds.y + clampedRadii.topLeft;
        const float length = bounds.height - clampedRadii.topLeft - clampedRadii.bottomLeft;

        C2D_DrawRectSolid(bounds.x - 1, startY, 0.0f, thickness.left, length, clr);
    }

    if(thickness.right > 0) {
        const float startX = bounds.x + bounds.width - static_cast<float>(thickness.right) + 1;
        const float startY = bounds.y + clampedRadii.topRight;
        const float length = bounds.height - clampedRadii.topRight - clampedRadii.bottomRight;

        C2D_DrawRectSolid(startX, startY, 0.0f, thickness.right, length, clr);
    }

    if(thickness.top > 0) {
        const float startX = bounds.x + clampedRadii.topLeft;
        const float length = bounds.width - clampedRadii.topLeft - clampedRadii.topRight;
        C2D_DrawRectSolid(startX, bounds.y - 1, 0.0f, length, thickness.top, clr);
    }

    if(thickness.bottom > 0) {
        const float startX = bounds.x + clampedRadii.bottomLeft;
        const float startY = bounds.y + bounds.height - static_cast<float>(thickness.bottom) + 1;
        const float length = bounds.width - clampedRadii.bottomLeft - clampedRadii.bottomRight;

        C2D_DrawRectSolid(startX, startY, 0.0f, length, thickness.bottom, clr);
    }

    if(radius.topLeft > 0) {
        const float centerX = bounds.x + clampedRadii.topLeft;
        const float centerY = bounds.y + clampedRadii.topLeft;

        C2D_Clay_Arc(centerX, centerY, clampedRadii.topLeft, 180.0f, 270.0f, thickness.top, color);
    }

    if(radius.topRight > 0) {
        const float centerX = bounds.x + bounds.width - clampedRadii.topRight;
        const float centerY = bounds.y + clampedRadii.topRight;

        C2D_Clay_Arc(centerX, centerY, clampedRadii.topRight, 270.0f, 360.0f, thickness.top, color);
    }

    if(radius.bottomLeft > 0) {
        const float centerX = bounds.x + clampedRadii.bottomLeft;
        const float centerY = bounds.y + bounds.height - clampedRadii.bottomLeft;

        C2D_Clay_Arc(centerX, centerY, clampedRadii.bottomLeft, 90.0f, 180.0f, thickness.bottom, color);
    }

    if(radius.bottomRight > 0) {
        const float centerX = bounds.x + bounds.width - clampedRadii.bottomRight;
        const float centerY = bounds.y + bounds.height - clampedRadii.bottomRight;

        C2D_Clay_Arc(centerX, centerY, clampedRadii.bottomLeft, 0.0f, 90.0f, thickness.bottom, color);
    }
}

Clay_Dimensions C2D_Clay_MeasureText(Clay_StringSlice slice, Clay_TextElementConfig* config, void* userData) {
    const uint8_t* chars = reinterpret_cast<const uint8_t*>(slice.chars);

    float maxTextWidth = 0.0f;
    float textWidth    = 0.0f;

    std::vector<C2D_Font>& fonts = *reinterpret_cast<std::vector<C2D_Font>*>(userData);
    C2D_Font& font               = fonts[config->fontId];

    FINF_s* fontInfo  = C2D_FontGetInfo(font);
    float scaleFactor = static_cast<float>(config->fontSize) / fontInfo->height;

    for(int32_t i = 0; i < slice.length;) {
        uint32_t code;
        ssize_t units = decode_utf8(&code, chars + i);

        if(units == -1) {
            code  = 0xFFFD;
            units = 1;
        }
        else if(code == '\0' || i + units > slice.length) {
            break;
        }

        i += units;
        if(code == '\n') {
            maxTextWidth = CLAY__MAX(maxTextWidth, textWidth);
            textWidth    = 0;

            continue;
        }

        charWidthInfo_s* info = C2D_FontGetCharWidthInfo(font, C2D_FontGlyphIndexFromCodePoint(font, code));
        textWidth += info->charWidth * scaleFactor;
    }

    return {
        .width  = CLAY__MAX(maxTextWidth, textWidth),
        .height = scaleFactor * fontInfo->lineFeed
    };
}

void C2D_Clay_RenderClayCommands(Clay_C2DRendererData* rendererData, Clay_RenderCommandArray rcommands, gfxScreen_t screen) {
    for(int32_t i = 0; i < rcommands.length; i++) {
        Clay_RenderCommand* rcmd      = Clay_RenderCommandArray_Get(&rcommands, i);
        const Clay_BoundingBox bounds = rcmd->boundingBox;

        switch(rcmd->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            Clay_RectangleRenderData* config = &rcmd->renderData.rectangle;
            C2D_Clay_Rect(bounds, config->cornerRadius, config->backgroundColor);

            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData* config = &rcmd->renderData.text;

            C2D_Font font    = rendererData->fonts[config->fontId];
            FINF_s* fontInfo = C2D_FontGetInfo(font);

            const float scale = config->fontSize / static_cast<float>(fontInfo->height);

            const char* contents = config->stringContents.chars;
            size_t len           = static_cast<size_t>(config->stringContents.length);

            uint32_t hash = hashStr(contents, len);

            auto it = s_textCache.find(rcmd->id);
            if(it == s_textCache.end()) {
                C2D_TextBuf buf = C2D_TextBufNew(len + 1);
                C2D_Text text;

                char* str = strndup(contents, len);
                if(str == nullptr || C2D_TextFontParse(&text, font, buf, str) == nullptr) {
                    if(str != nullptr) {
                        fprintf(stderr, "Failed to parse text: %s\n", str);
                    }

                    free(str);
                    break;
                }

                free(str);

                C2D_TextOptimize(&text);
                it = s_textCache.insert({ rcmd->id, { text, hash } }).first;
            }
            else if(it->second.textHash != hash) {
                C2D_Text text   = it->second.text;
                C2D_TextBuf buf = text.buf;

                C2D_TextBufClear(buf);
                buf = C2D_TextBufResize(buf, len + 1);

                char* str = strndup(contents, len);
                if(str == nullptr || C2D_TextFontParse(&text, font, buf, str) == nullptr) {
                    if(str != nullptr) {
                        fprintf(stderr, "Failed to parse text: %s\n", str);
                    }

                    free(str);
                    break;
                }

                free(str);

                C2D_TextOptimize(&text);
                it->second = CacheEntry{ text, hash };
            }

            C2D_DrawText(&it->second.text, C2D_WithColor, bounds.x, bounds.y, 0.0f, scale, scale, C2D_Color32(config->textColor.r, config->textColor.g, config->textColor.b, config->textColor.a));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData* config = &rcmd->renderData.border;

            C2D_Clay_Square(
                Clay_BoundingBox{
                    .x = bounds.x,
                    .y = bounds.y,

                    .width  = bounds.width,
                    .height = bounds.height,
                },
                config->cornerRadius,
                config->width,
                config->color
            );

            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            Clay_ImageRenderData* config = &rcmd->renderData.image;
            C2D_Image* image             = reinterpret_cast<C2D_Image*>(config->imageData);

            if(image == nullptr || (image->subtex == nullptr && image->tex == nullptr)) {
                break;
            }

            Clay_Color tintColour = rcmd->renderData.image.backgroundColor;
            float blend           = 1.0f;

            if(tintColour.r == 0 && tintColour.g == 0 && tintColour.b == 0 && tintColour.a == 0) {
                tintColour = { 0xFF, 0xFF, 0xFF, 0xFF };
                blend      = 0.0f;
            }

            C2D_ImageTint tint;
            C2D_PlainImageTint(&tint, C2D_Color32(tintColour.r, tintColour.g, tintColour.b, tintColour.a), blend);

            float scaleX = bounds.width / static_cast<float>(image->subtex->width);
            float scaleY = bounds.height / static_cast<float>(image->subtex->height);

            C2D_DrawImageAt(*image, bounds.x, bounds.y, 0.0f, &tint, scaleX, scaleY);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
            C2D_Clay_ScissorStart(screen, bounds);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
            C2D_Clay_ScissorEnd();
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
            Clay_CustomRenderData config = rcmd->renderData.custom;
            if(config.customData == nullptr) {
                break;
            }

            CustomElementData* data = reinterpret_cast<CustomElementData*>(config.customData);

            switch(data->type) {
            case CUSTOM_ELEMENT_TYPE_CIRCLE: {
                // azahar doesnt show ellipse properly in vulkan mode, use filled arc instead
                // C2D_DrawEllipseSolid(bounds.x, bounds.y, 0.0f, bounds.width, bounds.height, C2D_Color32(config.backgroundColor.r, config.backgroundColor.g, config.backgroundColor.b, config.backgroundColor.a));
                C2D_Clay_FilledArc(bounds.x + (bounds.width / 2), bounds.y + (bounds.width / 2), bounds.width / 2, 0, 360.0f, config.backgroundColor);

                break;
            }
            case CUSTOM_ELEMENT_TYPE_POLYGON: {
                u32 clr = C2D_CLAY_COLOR(config.backgroundColor);

                float x = bounds.x, y = bounds.y, w = bounds.width, h = bounds.height;
                PolygonData poly = data->polygon;
#define X_POS(i) x + (CLAY__MIN(CLAY__MAX(poly.i.x, 0.0f), 1.0f) * w)
#define Y_POS(i) y + (CLAY__MIN(CLAY__MAX(poly.i.y, 0.0f), 1.0f) * h)

                C2D_DrawTriangle(
                    X_POS(a), Y_POS(a), clr,
                    X_POS(b), Y_POS(b), clr,
                    X_POS(c), Y_POS(c), clr,
                    0.0f
                );

#undef X_POS
#undef Y_POS
                break;
            }
            default:
                fprintf(stderr, "Unknown custom render command type: %d\n", data->type);
                break;
            }

            break;
        }
        default: {
            fprintf(stderr, "Unknown render command type: %d\n", rcmd->commandType);
            break;
        }
        }
    }
}