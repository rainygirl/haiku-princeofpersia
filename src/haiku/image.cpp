/*
 * PNG loading and saving for the Prince of Persia engine on Haiku, using the
 * Translation Kit. The game's own graphics come from the DAT files and never
 * pass through here; this serves the optional lighting mask, mod graphics, and
 * the F12 screenshot feature.
 */
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <Bitmap.h>
#include <BitmapStream.h>
#include <DataIO.h>
#include <File.h>
#include <TranslationUtils.h>
#include <TranslatorRoster.h>

#include <string>

extern "C" {
// from rwops.cpp
Sint64 pop_rw_size(SDL_RWops* ctx);
int pop_rw_seek_start(SDL_RWops* ctx);
}

namespace {

std::string g_img_error;

SDL_Surface* surface_from_bitmap(BBitmap* bitmap) {
	if (bitmap == NULL || bitmap->InitCheck() != B_OK) return NULL;
	BBitmap* source = bitmap;
	BBitmap* converted = NULL;
	if (bitmap->ColorSpace() != B_RGBA32 && bitmap->ColorSpace() != B_RGB32) {
		converted = new BBitmap(bitmap->Bounds(), B_RGBA32);
		if (converted->InitCheck() != B_OK || converted->ImportBits(bitmap) != B_OK) {
			delete converted;
			return NULL;
		}
		source = converted;
	}
	int w = (int)source->Bounds().Width() + 1;
	int h = (int)source->Bounds().Height() + 1;
	// ARGB8888: bytes B G R A in memory, which is exactly B_RGBA32.
	SDL_Surface* s = SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	if (s != NULL) {
		const bool has_alpha = source->ColorSpace() == B_RGBA32;
		for (int y = 0; y < h; ++y) {
			const Uint32* src = (const Uint32*)((const Uint8*)source->Bits() + y * source->BytesPerRow());
			Uint32* dst = (Uint32*)((Uint8*)s->pixels + y * s->pitch);
			for (int x = 0; x < w; ++x) dst[x] = has_alpha ? src[x] : (src[x] | 0xFF000000u);
		}
	}
	delete converted;
	return s;
}

SDL_Surface* load_from_positionio(BPositionIO* io) {
	BTranslatorRoster* roster = BTranslatorRoster::Default();
	if (roster == NULL) { g_img_error = "no translator roster"; return NULL; }
	BBitmapStream stream;
	if (roster->Translate(io, NULL, NULL, &stream, B_TRANSLATOR_BITMAP) != B_OK) {
		g_img_error = "no translator could decode the image";
		return NULL;
	}
	BBitmap* bitmap = NULL;
	if (stream.DetachBitmap(&bitmap) != B_OK || bitmap == NULL) {
		g_img_error = "could not detach bitmap";
		return NULL;
	}
	SDL_Surface* s = surface_from_bitmap(bitmap);
	delete bitmap;
	if (s == NULL) g_img_error = "could not convert bitmap";
	return s;
}

} // namespace

extern "C" {

const char* IMG_GetError(void) { return g_img_error.c_str(); }

SDL_Surface* IMG_Load(const char* file) {
	if (file == NULL) return NULL;
	BFile f(file, B_READ_ONLY);
	if (f.InitCheck() != B_OK) { g_img_error = std::string("cannot open ") + file; return NULL; }
	return load_from_positionio(&f);
}

SDL_Surface* IMG_Load_RW(SDL_RWops* src, int freesrc) {
	if (src == NULL) return NULL;
	Sint64 size = pop_rw_size(src);
	SDL_Surface* result = NULL;
	if (size > 0) {
		Uint8* data = (Uint8*)malloc((size_t)size);
		pop_rw_seek_start(src);
		if (SDL_RWread(src, data, 1, (size_t)size) == (size_t)size) {
			BMemoryIO io(data, (size_t)size);
			result = load_from_positionio(&io);
		} else {
			g_img_error = "short read";
		}
		free(data);
	}
	if (freesrc) SDL_RWclose(src);
	return result;
}

int IMG_SavePNG(SDL_Surface* surface, const char* file) {
	if (surface == NULL || file == NULL) return -1;
	BBitmap* bitmap = new BBitmap(BRect(0, 0, surface->w - 1, surface->h - 1), B_RGB32);
	if (bitmap->InitCheck() != B_OK) { delete bitmap; g_img_error = "cannot allocate bitmap"; return -1; }
	// Go through a 32-bit copy so every source format is handled by the blitter.
	SDL_Surface* rgb = SDL_CreateRGBSurface(0, surface->w, surface->h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	if (rgb == NULL) { delete bitmap; return -1; }
	int saved_key = surface->colorkey_enabled;
	int saved_mode = surface->blend_mode;
	surface->colorkey_enabled = 0;
	surface->blend_mode = SDL_BLENDMODE_NONE;
	SDL_BlitSurface(surface, NULL, rgb, NULL);
	surface->colorkey_enabled = saved_key;
	surface->blend_mode = saved_mode;
	for (int y = 0; y < surface->h; ++y) {
		const Uint32* src = (const Uint32*)((const Uint8*)rgb->pixels + y * rgb->pitch);
		Uint32* dst = (Uint32*)((Uint8*)bitmap->Bits() + y * bitmap->BytesPerRow());
		for (int x = 0; x < surface->w; ++x) dst[x] = src[x] | 0xFF000000u;
	}
	SDL_FreeSurface(rgb);

	int result = -1;
	BFile out(file, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (out.InitCheck() == B_OK) {
		BBitmapStream stream(bitmap);
		BTranslatorRoster* roster = BTranslatorRoster::Default();
		if (roster != NULL && roster->Translate(&stream, NULL, NULL, &out, B_PNG_FORMAT) == B_OK) {
			result = 0;
		} else {
			g_img_error = "PNG translator failed";
		}
		BBitmap* detached = NULL;
		stream.DetachBitmap(&detached);
		bitmap = detached;
	} else {
		g_img_error = std::string("cannot create ") + file;
	}
	delete bitmap;
	return result;
}

} // extern "C"
