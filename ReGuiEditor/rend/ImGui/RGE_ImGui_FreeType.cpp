#include "RGE_ImGui_FreeType.hpp"

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include <freetype/freetype.h>
#include <freetype/ftimage.h>
#include <freetype/ftmodapi.h>
#include <freetype/ftsizes.h>
#include <freetype/ftsynth.h>
#include <freetype/ftsystem.h>
#include <freetype/fttypes.h>

#include <freetype/config/integer-types.h>

#include <cfloat>
#include <cstdint>
#include <cstring>

#define FT_CEIL(X) (((X + 63) & -64) / 64)
#define FT_SCALEFACTOR 64.0f

#define DEMULTIPLY(color, alpha) ImMin(static_cast<uint32_t>(255.f * static_cast<float>(color) / (static_cast<float>(alpha) + FLT_MIN) + .5f), static_cast<uint32_t>(255))

static void* FreeType_Memory_Alloc(FT_Memory memory, long size) {
	if (size <= 0)
		return nullptr;

	return new uint8_t[size];
}

static void FreeType_Memory_Free(FT_Memory memory, void* block) {
	delete[] static_cast<uint8_t*>(block);
}

static void* FreeType_Memory_Realloc(FT_Memory memory, long currentSize, long newSize, void* block) {
	if (!block)
		return FreeType_Memory_Alloc(memory, newSize);

	if (newSize == 0) {
		FreeType_Memory_Free(memory, block);
		return nullptr;
	}

	if (newSize <= currentSize)
		return block;

	void* newBlock = FreeType_Memory_Alloc(memory, newSize);
	memcpy(newBlock, block, currentSize);
	FreeType_Memory_Free(memory, block);
	return newBlock;
}

struct RGE_ImGui_FreeType_Data {
	FT_Library ftLibrary = nullptr;
	FT_MemoryRec_ ftMemory = {};
};

struct RGE_ImGui_FreeType_FontSourceData {
	FT_Face ftFace = nullptr;
	RGE_ImGui_FreeType_LoaderFlags userFlags = RGE_ImGui_FreeType_LoaderFlags_None;
	FT_Int32 loadFlags = 0;
	FT_Size ftSizeLastActivated = nullptr;

	~RGE_ImGui_FreeType_FontSourceData() { this->destroyFont(); }

	bool initFont(FT_Library ftLibrary, const ImFontConfig* sourceConfig, RGE_ImGui_FreeType_LoaderFlags extraLoaderFlags) {
		FT_Error error = FT_New_Memory_Face(ftLibrary, static_cast<const FT_Byte*>(sourceConfig->FontData), static_cast<FT_Long>(sourceConfig->FontDataSize), static_cast<FT_Long>(sourceConfig->FontNo), &this->ftFace);
		if (error != 0) {
			CONSOLE_ERROR("Failed to create FreeType font face (Error: 0x%08X)", error);
			return false;
		}

		error = FT_Select_Charmap(this->ftFace, FT_ENCODING_UNICODE);
		if (error != 0) {
			CONSOLE_ERROR("Failed to select Unicode charmap for FreeType font face (Error: 0x%08X)", error);
			
			FT_Done_Face(this->ftFace);
			this->ftFace = nullptr;

			return false;
		}

		this->userFlags = static_cast<RGE_ImGui_FreeType_LoaderFlags>(sourceConfig->FontLoaderFlags | extraLoaderFlags);

		this->loadFlags = 0;
		if (!(this->userFlags & RGE_ImGui_FreeType_LoaderFlags_Bitmap)) this->loadFlags |= FT_LOAD_NO_BITMAP;
		if (this->userFlags & RGE_ImGui_FreeType_LoaderFlags_NoHinting) this->loadFlags |= FT_LOAD_NO_HINTING;
		if (this->userFlags & RGE_ImGui_FreeType_LoaderFlags_NoAutoHint) this->loadFlags |= FT_LOAD_NO_AUTOHINT;
		if (this->userFlags & RGE_ImGui_FreeType_LoaderFlags_ForceAutoHint) this->loadFlags |= FT_LOAD_FORCE_AUTOHINT;
		if (this->userFlags & RGE_ImGui_FreeType_LoaderFlags_LoadColor) this->loadFlags |= FT_LOAD_COLOR;

		if (this->userFlags & RGE_ImGui_FreeType_LoaderFlags_MonoHinting) this->loadFlags |= FT_LOAD_TARGET_MONO;
		else this->loadFlags |= FT_LOAD_TARGET_LIGHT; // Vertical-only hinting keeps glyph shapes true to the font and spacing even.

		return true;
	}

	void destroyFont() {
		if (this->ftFace) {
			FT_Done_Face(this->ftFace);
			this->ftFace = nullptr;
		}
	}
};

struct RGE_ImGui_FreeType_FontSourceBakedData {
	FT_Size ftSize = nullptr;
};

static void blitGlyph(const FT_Bitmap* bitmap, uint32_t* destination, uint32_t destinationPitch) {
	IM_ASSERT(bitmap != nullptr && "Invalid bitmap");
	IM_ASSERT(destination != nullptr && "Invalid destination buffer");

	uint32_t bitmapWidth = bitmap->width;
	uint32_t bitmapHeight = bitmap->rows;

	uint8_t* source = bitmap->buffer;
	int sourcePitch = bitmap->pitch;

	if (sourcePitch < 0) {
		source += static_cast<size_t>(-sourcePitch) * static_cast<size_t>(bitmapHeight - 1);
		sourcePitch = -sourcePitch;
	}

	switch (bitmap->pixel_mode) {
	case FT_PIXEL_MODE_GRAY:
		for (uint32_t y = 0; y < bitmapHeight; y++, source += sourcePitch, destination += destinationPitch) {
			for (uint32_t x = 0; x < bitmapWidth; x++)
				destination[x] = IM_COL32(255, 255, 255, source[x]);
		}
		break;
	case FT_PIXEL_MODE_MONO:
		for (uint32_t y = 0; y < bitmapHeight; y++, source += sourcePitch, destination += destinationPitch) {
			uint8_t bits = 0;
			const uint8_t* bitsPointer = source;
			
			for (uint32_t x = 0; x < bitmapWidth; x++, bits <<= 1) {
				if ((x & 7) == 0)
					bits = *bitsPointer++;
				destination[x] = IM_COL32(255, 255, 255, (bits & 0x80) ? 255 : 0);
			}
		}
		break;
	case FT_PIXEL_MODE_BGRA:
		for (uint32_t y = 0; y < bitmapHeight; y++, source += sourcePitch, destination += destinationPitch) {
			for (uint32_t x = 0; x < bitmapWidth; x++) {
				uint8_t b = source[x * 4 + 0];
				uint8_t g = source[x * 4 + 1];
				uint8_t r = source[x * 4 + 2];
				uint8_t a = source[x * 4 + 3];

				destination[x] = IM_COL32(
					DEMULTIPLY(r, a),
					DEMULTIPLY(g, a),
					DEMULTIPLY(b, a),
					a
				);
			}
		}
		break;
	default:
		IM_ASSERT(false && "Unsupported FreeType pixel mode");
		break;
	}
}

static bool loaderInit(ImFontAtlas* fontAtlas) {
	IM_ASSERT(fontAtlas != nullptr && "Invalid font atlas");
	IM_ASSERT(fontAtlas->FontLoaderData == nullptr && "Font loader data already initialized");

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GREEN, "Initializing FreeType font loader");

	RGE_ImGui_FreeType_Data* loaderData = IM_NEW(RGE_ImGui_FreeType_Data)();
	loaderData->ftMemory.user = nullptr;
	loaderData->ftMemory.alloc = FreeType_Memory_Alloc;
	loaderData->ftMemory.free = FreeType_Memory_Free;
	loaderData->ftMemory.realloc = FreeType_Memory_Realloc;

	FT_Error error = FT_New_Library(&loaderData->ftMemory, &loaderData->ftLibrary);
	if (error != 0) {
		CONSOLE_ERROR("Failed to initialize FreeType library (Error: 0x%08X)", error);

		IM_DELETE(loaderData);
		return false;
	}

	FT_Int versionMajour = 0;
	FT_Int versionMinor = 0;
	FT_Int versionPatch = 0;
	FT_Library_Version(loaderData->ftLibrary, &versionMajour, &versionMinor, &versionPatch);
	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GREEN, "FreeType library initialized (Version: %d.%d.%d)", versionMajour, versionMinor, versionPatch);

	FT_Add_Default_Modules(loaderData->ftLibrary);
	fontAtlas->FontLoaderData = loaderData;

	return true;
}

static void loaderShutdown(ImFontAtlas* fontAtlas) {
	IM_ASSERT(fontAtlas != nullptr && "Invalid font atlas");
	IM_ASSERT(fontAtlas->FontLoaderData != nullptr && "Invalid font loader data");

	CONSOLE_LOG(LEVEL_DEFAULT, COLOR_GREEN, "Shutting down FreeType font loader");

	RGE_ImGui_FreeType_Data* loaderData = static_cast<RGE_ImGui_FreeType_Data*>(fontAtlas->FontLoaderData);
	if (loaderData->ftLibrary) {
		FT_Done_Library(loaderData->ftLibrary);
		loaderData->ftLibrary = nullptr;
	}

	IM_DELETE(loaderData);
	fontAtlas->FontLoaderData = nullptr;
}

static bool fontSourceInit(ImFontAtlas* fontAtlas, ImFontConfig* sourceConfig) {
	IM_ASSERT(fontAtlas != nullptr && "Invalid font atlas");
	IM_ASSERT(fontAtlas->FontLoaderData != nullptr && "Invalid font loader data");
	IM_ASSERT(sourceConfig != nullptr && "Invalid source font config");

	RGE_ImGui_FreeType_Data* loaderData = static_cast<RGE_ImGui_FreeType_Data*>(fontAtlas->FontLoaderData);
	RGE_ImGui_FreeType_FontSourceData* fontSourceData = IM_NEW(RGE_ImGui_FreeType_FontSourceData)();
	if (!fontSourceData->initFont(loaderData->ftLibrary, sourceConfig, RGE_ImGui_FreeType_LoaderFlags_None)) {
		IM_DELETE(fontSourceData);
		return false;
	}

	sourceConfig->FontLoaderData = fontSourceData;
	return true;
}

static void fontSourceDestroy(ImFontAtlas* fontAtlas, ImFontConfig* sourceConfig) {
	IM_UNUSED(fontAtlas);

	IM_ASSERT(sourceConfig != nullptr && "Invalid source font config");

	RGE_ImGui_FreeType_FontSourceData* fontSourceData = static_cast<RGE_ImGui_FreeType_FontSourceData*>(sourceConfig->FontLoaderData);
	IM_DELETE(fontSourceData);
	sourceConfig->FontLoaderData = nullptr;
}

static bool fontSourceContainsGlyph(ImFontAtlas* fontAtlas, ImFontConfig* sourceConfig, ImWchar codepoint) {
	IM_UNUSED(fontAtlas);

	IM_ASSERT(sourceConfig != nullptr && "Invalid source font config");

	RGE_ImGui_FreeType_FontSourceData* fontData = static_cast<RGE_ImGui_FreeType_FontSourceData*>(sourceConfig->FontLoaderData);
	IM_ASSERT(fontData != nullptr && "Invalid font loader data");

	return FT_Get_Char_Index(fontData->ftFace, codepoint) != 0;
}

static bool bakedInit(ImFontAtlas* fontAtlas, ImFontConfig* sourceConfig, ImFontBaked* bakedFont, void* loaderDataForBakedSource) {
	IM_UNUSED(fontAtlas);

	IM_ASSERT(sourceConfig != nullptr && "Invalid source font config");

	RGE_ImGui_FreeType_FontSourceData* fontData = reinterpret_cast<RGE_ImGui_FreeType_FontSourceData*>(sourceConfig->FontLoaderData);

	float fontSize = bakedFont->Size;
	float referenceSize = bakedFont->OwnerFont->Sources[0]->SizePixels;
	if (sourceConfig->MergeMode && sourceConfig->SizePixels != 0.f)
		fontSize *= (sourceConfig->SizePixels / referenceSize);
	fontSize *= sourceConfig->ExtraSizeScale;

	RGE_ImGui_FreeType_FontSourceBakedData* bakedData = reinterpret_cast<RGE_ImGui_FreeType_FontSourceBakedData*>(loaderDataForBakedSource);
	IM_ASSERT(bakedData != nullptr && "Invalid font source data");
	IM_PLACEMENT_NEW(bakedData) RGE_ImGui_FreeType_FontSourceBakedData();

	FT_Error error = FT_New_Size(fontData->ftFace, &bakedData->ftSize);
	if (error) {
		CONSOLE_ERROR("Failed to create FreeType size object (Error: 0x%08X)", error);
		return false;
	}

	error = FT_Activate_Size(bakedData->ftSize);
	if (error) {
		CONSOLE_ERROR("Failed to activate FreeType size object (Error: 0x%08X)", error);
		FT_Done_Size(bakedData->ftSize);
		return false;
	}

	fontData->ftSizeLastActivated = bakedData->ftSize;

	float rasterizerDensity = sourceConfig->RasterizerDensity * bakedFont->RasterizerDensity;

	FT_Size_RequestRec sizeRequest;
	memset(&sizeRequest, 0, sizeof(sizeRequest));
	sizeRequest.type = (fontData->userFlags & RGE_ImGui_FreeType_LoaderFlags_Bitmap) ? FT_SIZE_REQUEST_TYPE_NOMINAL : FT_SIZE_REQUEST_TYPE_REAL_DIM;
	sizeRequest.width = 0;
	sizeRequest.height = static_cast<FT_Long>(fontSize * 64.f * rasterizerDensity);
	sizeRequest.horiResolution = 0;
	sizeRequest.vertResolution = 0;

	error = FT_Request_Size(fontData->ftFace, &sizeRequest);
	if (error) {
		CONSOLE_ERROR("Failed to request FreeType size object (Error: 0x%08X)", error);
		FT_Done_Size(bakedData->ftSize);
		return false;
	}

	if (sourceConfig->MergeMode)
		return true;

	FT_Size_Metrics sizeMetrics = bakedData->ftSize->metrics;

	float scale = 1.f / (rasterizerDensity * sourceConfig->ExtraSizeScale);
	bakedFont->Ascent = static_cast<float>(FT_CEIL(sizeMetrics.ascender)) * scale;
	bakedFont->Descent = static_cast<float>(FT_CEIL(sizeMetrics.descender)) * scale;

	return true;
}

static void bakedDestroy(ImFontAtlas* fontAtlas, ImFontConfig* sourceConfig, ImFontBaked* baked, void* loaderDataForBakedSource) {
	IM_ASSERT(fontAtlas != nullptr && "Invalid font atlas");
	IM_ASSERT(fontAtlas->FontLoaderData != nullptr && "Invalid font loader data");
	IM_ASSERT(sourceConfig != nullptr && "Invalid source font config");

	RGE_ImGui_FreeType_FontSourceBakedData* bakedData = reinterpret_cast<RGE_ImGui_FreeType_FontSourceBakedData*>(loaderDataForBakedSource);
	IM_ASSERT(bakedData != nullptr && "Invalid font source data");

	if (fontAtlas->FontLoaderData) {
		RGE_ImGui_FreeType_FontSourceData* fontData = reinterpret_cast<RGE_ImGui_FreeType_FontSourceData*>(sourceConfig->FontLoaderData);

		if (fontData->ftSizeLastActivated == bakedData->ftSize)
			fontData->ftSizeLastActivated = nullptr;
	}

	if (bakedData->ftSize) {
		FT_Done_Size(bakedData->ftSize);
		bakedData->ftSize = nullptr;
	}
}

static bool bakedLoadGlyph(ImFontAtlas* fontAtlas, ImFontConfig* sourceConfig, ImFontBaked* bakedFont, void* loaderDataForBakedSource, ImWchar codepoint, ImFontGlyph* outputGlyph, float* outputAdvanceX) {
	IM_ASSERT(fontAtlas != nullptr && "Invalid font atlas");
	IM_ASSERT(fontAtlas->FontLoaderData != nullptr && "Invalid font loader data");
	IM_ASSERT(sourceConfig != nullptr && "Invalid source font config");

	RGE_ImGui_FreeType_FontSourceData* fontData = reinterpret_cast<RGE_ImGui_FreeType_FontSourceData*>(sourceConfig->FontLoaderData);
	IM_ASSERT(fontData != nullptr && "Invalid font source data");

	RGE_ImGui_FreeType_FontSourceBakedData* bakedData = reinterpret_cast<RGE_ImGui_FreeType_FontSourceBakedData*>(loaderDataForBakedSource);
	IM_ASSERT(bakedData != nullptr && "Invalid font source data");

	uint32_t glyphIndex = FT_Get_Char_Index(fontData->ftFace, codepoint);
	if (glyphIndex == 0)
		return false;

	if (fontData->ftSizeLastActivated != bakedData->ftSize) {
		FT_Error error = FT_Activate_Size(bakedData->ftSize);
		if (error) {
			CONSOLE_ERROR("Failed to activate FreeType size object (Error: 0x%08X)", error);
			return false;
		}

		fontData->ftSizeLastActivated = bakedData->ftSize;
	}

	FT_Error error = FT_Load_Glyph(fontData->ftFace, glyphIndex, fontData->loadFlags);
	if (error) {
		CONSOLE_ERROR("Failed to load FreeType glyph (Error: 0x%08X)", error);
		return false;
	}

	FT_Face ftFace = fontData->ftFace;
	FT_GlyphSlot glyphSlot = ftFace->glyph;
	IM_ASSERT(glyphSlot->format != FT_GLYPH_FORMAT_SVG && "SVG glyphs are for now unsupported");

	if (fontData->userFlags & RGE_ImGui_FreeType_LoaderFlags_Bold) FT_GlyphSlot_Embolden(glyphSlot);
	if (fontData->userFlags & RGE_ImGui_FreeType_LoaderFlags_Oblique) FT_GlyphSlot_Oblique(glyphSlot);

	float rasterizerDensity = sourceConfig->RasterizerDensity * bakedFont->RasterizerDensity;

	float advanceX = (glyphSlot->advance.x / FT_SCALEFACTOR) / rasterizerDensity;
	if (outputAdvanceX) {
		IM_ASSERT(outputGlyph == nullptr && "Output glyph is provided with advance X");

		*outputAdvanceX = advanceX;
		return true;
	}

	error = FT_Render_Glyph(glyphSlot, (fontData->userFlags & RGE_ImGui_FreeType_LoaderFlags_Monochrome) ? FT_RENDER_MODE_MONO : FT_RENDER_MODE_NORMAL);
	if (error) {
		CONSOLE_ERROR("Failed to render FreeType glyph (Error: 0x%08X)", error);
		return false;
	}

	const FT_Bitmap* ftBitmap = &glyphSlot->bitmap;
	if (!ftBitmap) {
		CONSOLE_ERROR("Failed to get FreeType glyph bitmap");
		return false;
	}

	uint32_t glyphWidth = ftBitmap->width;
	uint32_t glyphHeight = ftBitmap->rows;
	bool isVisible = glyphWidth != 0 && glyphHeight != 0;

	outputGlyph->Codepoint = codepoint;
	outputGlyph->AdvanceX = advanceX;

	if (!isVisible)
		return true;

	ImFontAtlasRectId packId = ImFontAtlasPackAddRect(fontAtlas, glyphWidth, glyphHeight);
	if (packId == ImFontAtlasRectId_Invalid) {
		CONSOLE_ERROR("Failed to pack FreeType glyph into font atlas. (Out of texture memory)");
		return false;
	}

	ImTextureRect* rectangle = ImFontAtlasPackGetRect(fontAtlas, packId);

	fontAtlas->Builder->TempBuffer.resize(glyphWidth * glyphHeight * 4);
	uint32_t* tempBuffer = reinterpret_cast<uint32_t*>(fontAtlas->Builder->TempBuffer.Data);
	blitGlyph(ftBitmap, tempBuffer, glyphWidth);

	float referenceSize = bakedFont->OwnerFont->Sources[0]->SizePixels;
	float offsetsScale = (referenceSize != 0.f) ? (bakedFont->Size / referenceSize) : 1.f;

	float offsetX = ImFloor(sourceConfig->GlyphOffset.x * offsetsScale + 0.5f);
	float offsetY = ImFloor(sourceConfig->GlyphOffset.y * offsetsScale + 0.5f) + bakedFont->Ascent;

	float reciprocalDensity = 1.f / rasterizerDensity;

	float glyphOffsetX = static_cast<float>(ftFace->glyph->bitmap_left);
	float glyphOffsetY = static_cast<float>(-ftFace->glyph->bitmap_top);

	outputGlyph->X0 = glyphOffsetX * reciprocalDensity + offsetX;
	outputGlyph->Y0 = glyphOffsetY * reciprocalDensity + offsetY;
	outputGlyph->X1 = (glyphOffsetX + glyphWidth) * reciprocalDensity + offsetX;
	outputGlyph->Y1 = (glyphOffsetY + glyphHeight) * reciprocalDensity + offsetY;
	outputGlyph->Visible = true;
	outputGlyph->Colored = ftBitmap->pixel_mode == FT_PIXEL_MODE_BGRA;
	outputGlyph->PackId = packId;

	ImFontAtlasBakedSetFontGlyphBitmap(fontAtlas, bakedFont, sourceConfig, outputGlyph, rectangle, reinterpret_cast<uint8_t*>(tempBuffer), ImTextureFormat_RGBA32, glyphWidth * 4);
	return true;
}

static ImFontLoader loader;

IMGUI_API const ImFontLoader* RGE_ImGui_FreeType_GetFontLoader() {
	loader.Name = "RGE_ImGui_FreeType";
	loader.LoaderInit = loaderInit;
	loader.LoaderShutdown = loaderShutdown;
	loader.FontSrcInit = fontSourceInit;
	loader.FontSrcDestroy = fontSourceDestroy;
	loader.FontSrcContainsGlyph = fontSourceContainsGlyph;
	loader.FontBakedInit = bakedInit;
	loader.FontBakedDestroy = bakedDestroy;
	loader.FontBakedLoadGlyph = bakedLoadGlyph;
	loader.FontBakedSrcLoaderDataSize = sizeof(RGE_ImGui_FreeType_FontSourceBakedData);
	return &loader;
}