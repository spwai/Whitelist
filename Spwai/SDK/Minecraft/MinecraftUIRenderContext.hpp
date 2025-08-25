#pragma once

#include <cstdint>
#include <string>

struct BitmapFont;
struct TextHolder;
struct UIColor;
struct Vector4;
struct CaretMeasureData;

class ComponentRenderBatch;
class NinesliceInfo;
class HashedString;

struct Color {
    float r, g, b, a;
    
    constexpr Color(float r, float g, float b, float a) 
        : r(r), g(g), b(b), a(a) {}
};

struct Vec2 {
    float x, y;
    
    constexpr Vec2(float x, float y) 
        : x(x), y(y) {}
};

struct RectangleArea {
    float left, right, top, bottom;
    
    constexpr RectangleArea(float left, float top, float right, float bottom) 
        : left(left), right(right), top(top), bottom(bottom) {}
};

enum class TextAlignment {
    LEFT,
    RIGHT,
    CENTER,
};

struct TextMeasureData {
    float textSize = 10.f;
    float linePadding = 0.f;
    bool displayShadow = false;
    bool showColorSymbols = false;
    bool hideHyphen = false;

    constexpr TextMeasureData(float textSize, bool displayShadow, bool showColorSymbols)
        : textSize(textSize), displayShadow(displayShadow), showColorSymbols(showColorSymbols) {}
};

struct CaretMeasureData {
    int position = -1;
    bool shouldRender = false;
};

class Font {
};

class MinecraftUIRenderContext {
public:
    virtual ~MinecraftUIRenderContext() = 0;
    virtual void getLineLength(class Font*, std::string const&, float, bool) = 0;
    virtual float getTextAlpha() = 0;
    virtual void setTextAlpha(float) = 0;
    virtual void drawDebugText(RectangleArea const&, std::string const&, Color const&, float, TextAlignment, TextMeasureData const&, CaretMeasureData const&) = 0;
    virtual void drawText(class Font*, RectangleArea const&, std::string const&, Color const&, float, TextAlignment, TextMeasureData const&, CaretMeasureData const&) = 0;
    virtual void flushText_(float) = 0;
    virtual void drawImage_(void* const& texture, Vec2 const& pos, Vec2 const& size, Vec2 const& uvPos, Vec2 const& uvSize) = 0;
    virtual void drawNineslice(void* const&, void* const&) = 0;
    virtual void flushImages(Color const&, float, void* const&) = 0;
    virtual void beginSharedMeshBatch(void*) = 0;
    virtual void endSharedMeshBatch(void*) = 0;
    virtual void drawRectangle(RectangleArea const&, Color const&, float, int) = 0;
    virtual void fillRectangle(RectangleArea const&, Color const&, float) = 0;
    virtual void increaseStencilRef() = 0;
    virtual void decreaseStencilRef() = 0;
    virtual void resetStencilRef() = 0;
    virtual void fillRectangleStencil(RectangleArea const&) = 0;
    virtual void enableScissorTest(RectangleArea const&) = 0;
    virtual void disableScissorTest() = 0;
    virtual void setClippingRectangle(RectangleArea const&) = 0;
    virtual void setFullClippingRectangle() = 0;
    virtual void saveCurrentClippingRectangle() = 0;
    virtual void restoreSavedClippingRectangle() = 0;
    virtual RectangleArea getFullClippingRectangle() = 0;
    virtual void updateCustom(void*) = 0;
    virtual void renderCustom(void*, int, RectangleArea const&) = 0;
    virtual void cleanup() = 0;
    virtual void removePersistentMeshes() = 0;
    virtual void* getTexture(void* ptr, void* const&, bool) = 0;
    virtual void* getZippedTexture(void* ptr, std::string const&, void* const&, bool) = 0;
    virtual void unloadTexture(void* const&) = 0;
    virtual void getUITextureInfo(void* const&, bool) = 0;
    virtual void touchTexture(void* const&) = 0;
    virtual void getMeasureStrategy() = 0;
    virtual void snapImageSizeToGrid(Vec2&) = 0;
    virtual void snapImagePositionToGrid(Vec2&) = 0;
    virtual void notifyImageEstimate(unsigned long) = 0;
};