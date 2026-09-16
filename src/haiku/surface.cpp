/*
 * Software surfaces for the Prince of Persia engine on Haiku.
 *
 * The engine draws everything into 320x200 surfaces: 8-bit paletted sprites
 * decoded from the DAT files, a 24-bit RGB screen buffer, and 32-bit RGBA
 * overlays. This file implements those surfaces, palettes, fills, blits
 * (color key, alpha blend, add, modulate) and format conversions in plain C++
 * with no external library.
 */
#include <SDL2/SDL.h>

namespace {

struct Rgba { int r, g, b, a; };

int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

Uint8 shift_of(Uint32 mask) {
	if (mask == 0) return 0;
	Uint8 shift = 0;
	while (!(mask & 1)) { mask >>= 1; ++shift; }
	return shift;
}

Uint32 format_from_masks(int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
	if (depth == 8) return SDL_PIXELFORMAT_INDEX8;
	if (depth == 24) return SDL_PIXELFORMAT_RGB24;
	if (depth == 32) {
		if (Rmask == 0x00FF0000 && Amask == 0xFF000000) return SDL_PIXELFORMAT_ARGB8888;
		if (Rmask == 0x000000FF && Amask == 0xFF000000) return SDL_PIXELFORMAT_ABGR8888;
		if (Rmask == 0x00FF0000 && Amask == 0) return SDL_PIXELFORMAT_RGB888;
		return SDL_PIXELFORMAT_UNKNOWN;
	}
	return SDL_PIXELFORMAT_UNKNOWN;
}

SDL_Palette* palette_create(int ncolors) {
	SDL_Palette* p = (SDL_Palette*)calloc(1, sizeof(SDL_Palette));
	p->ncolors = ncolors;
	p->colors = (SDL_Color*)calloc(ncolors, sizeof(SDL_Color));
	for (int i = 0; i < ncolors; ++i) {
		p->colors[i].r = p->colors[i].g = p->colors[i].b = 255;
		p->colors[i].a = 255;
	}
	p->refcount = 1;
	return p;
}

void palette_release(SDL_Palette* p) {
	if (p == NULL) return;
	if (--p->refcount > 0) return;
	free(p->colors);
	free(p);
}

inline Uint32 read_raw(const SDL_Surface* s, const Uint8* px) {
	switch (s->format->BytesPerPixel) {
		case 1: return *px;
		case 3: return px[0] | (px[1] << 8) | (px[2] << 16);
		default: return *(const Uint32*)px;
	}
}

inline void write_raw(const SDL_Surface* s, Uint8* px, Uint32 v) {
	switch (s->format->BytesPerPixel) {
		case 1: *px = (Uint8)v; break;
		case 3: px[0] = (Uint8)v; px[1] = (Uint8)(v >> 8); px[2] = (Uint8)(v >> 16); break;
		default: *(Uint32*)px = v; break;
	}
}

inline Rgba raw_to_rgba(const SDL_Surface* s, Uint32 v) {
	const SDL_PixelFormat* f = s->format;
	Rgba c;
	if (f->format == SDL_PIXELFORMAT_INDEX8) {
		const SDL_Palette* p = f->palette;
		if (p != NULL && (int)v < p->ncolors) {
			c.r = p->colors[v].r; c.g = p->colors[v].g; c.b = p->colors[v].b; c.a = p->colors[v].a;
		} else {
			c.r = c.g = c.b = 0; c.a = 255;
		}
		return c;
	}
	c.r = (v & f->Rmask) >> f->Rshift;
	c.g = (v & f->Gmask) >> f->Gshift;
	c.b = (v & f->Bmask) >> f->Bshift;
	c.a = f->Amask ? (int)((v & f->Amask) >> f->Ashift) : 255;
	return c;
}

int nearest_palette_index(const SDL_Palette* p, int r, int g, int b) {
	if (p == NULL) return 0;
	int best = 0;
	long best_dist = -1;
	for (int i = 0; i < p->ncolors; ++i) {
		long dr = p->colors[i].r - r, dg = p->colors[i].g - g, db = p->colors[i].b - b;
		long dist = dr * dr + dg * dg + db * db;
		if (best_dist < 0 || dist < best_dist) { best_dist = dist; best = i; }
	}
	return best;
}

inline Uint32 rgba_to_raw(const SDL_Surface* s, const Rgba& c) {
	const SDL_PixelFormat* f = s->format;
	if (f->format == SDL_PIXELFORMAT_INDEX8) {
		return nearest_palette_index(f->palette, c.r, c.g, c.b);
	}
	return ((Uint32)c.r << f->Rshift) | ((Uint32)c.g << f->Gshift) | ((Uint32)c.b << f->Bshift)
		| (f->Amask ? ((Uint32)c.a << f->Ashift) : 0);
}

inline Uint8* pixel_ptr(const SDL_Surface* s, int x, int y) {
	return (Uint8*)s->pixels + y * s->pitch + x * s->format->BytesPerPixel;
}

bool intersect(const SDL_Rect& a, const SDL_Rect& b, SDL_Rect* out) {
	int x0 = a.x > b.x ? a.x : b.x;
	int y0 = a.y > b.y ? a.y : b.y;
	int x1 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
	int y1 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
	if (x1 <= x0 || y1 <= y0) { out->x = out->y = out->w = out->h = 0; return false; }
	out->x = x0; out->y = y0; out->w = x1 - x0; out->h = y1 - y0;
	return true;
}

/* Blend one source pixel (already in rgba, alpha modulated) onto a destination pixel. */
inline void blend_pixel(SDL_Surface* dst, Uint8* dp, const Rgba& sc, int mode) {
	Rgba dc = raw_to_rgba(dst, read_raw(dst, dp));
	Rgba out;
	switch (mode) {
		case SDL_BLENDMODE_BLEND: {
			int a = sc.a;
			out.r = (sc.r * a + dc.r * (255 - a)) / 255;
			out.g = (sc.g * a + dc.g * (255 - a)) / 255;
			out.b = (sc.b * a + dc.b * (255 - a)) / 255;
			out.a = a + dc.a * (255 - a) / 255;
			break;
		}
		case SDL_BLENDMODE_ADD: {
			int a = sc.a;
			out.r = clamp255(dc.r + sc.r * a / 255);
			out.g = clamp255(dc.g + sc.g * a / 255);
			out.b = clamp255(dc.b + sc.b * a / 255);
			out.a = dc.a;
			break;
		}
		case SDL_BLENDMODE_MOD:
			out.r = sc.r * dc.r / 255;
			out.g = sc.g * dc.g / 255;
			out.b = sc.b * dc.b / 255;
			out.a = dc.a;
			break;
		default:
			out = sc;
			break;
	}
	write_raw(dst, dp, rgba_to_raw(dst, out));
}

} // namespace

extern "C" {

const char* SDL_GetPixelFormatName(Uint32 format) {
	switch (format) {
		case SDL_PIXELFORMAT_INDEX8: return "SDL_PIXELFORMAT_INDEX8";
		case SDL_PIXELFORMAT_RGB24: return "SDL_PIXELFORMAT_RGB24";
		case SDL_PIXELFORMAT_ARGB8888: return "SDL_PIXELFORMAT_ARGB8888";
		case SDL_PIXELFORMAT_ABGR8888: return "SDL_PIXELFORMAT_ABGR8888";
		case SDL_PIXELFORMAT_RGB888: return "SDL_PIXELFORMAT_RGB888";
		default: return "SDL_PIXELFORMAT_UNKNOWN";
	}
}

SDL_Surface* SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
	(void)flags;
	if (w < 0 || h < 0 || !(depth == 8 || depth == 24 || depth == 32)) return NULL;
	SDL_Surface* s = (SDL_Surface*)calloc(1, sizeof(SDL_Surface));
	SDL_PixelFormat* f = (SDL_PixelFormat*)calloc(1, sizeof(SDL_PixelFormat));
	if (s == NULL || f == NULL) { free(s); free(f); return NULL; }
	f->BitsPerPixel = depth;
	f->BytesPerPixel = depth / 8;
	if (depth == 8) {
		f->palette = palette_create(256);
	} else {
		if (depth == 24 && Rmask == 0) { Rmask = 0x0000FF; Gmask = 0x00FF00; Bmask = 0xFF0000; }
		if (depth == 32 && Rmask == 0) { Rmask = 0x00FF0000; Gmask = 0x0000FF00; Bmask = 0x000000FF; Amask = 0xFF000000; }
		f->Rmask = Rmask; f->Gmask = Gmask; f->Bmask = Bmask; f->Amask = (depth == 32) ? Amask : 0;
		f->Rshift = shift_of(Rmask); f->Gshift = shift_of(Gmask); f->Bshift = shift_of(Bmask); f->Ashift = shift_of(f->Amask);
	}
	f->format = format_from_masks(depth, f->Rmask, f->Gmask, f->Bmask, f->Amask);
	s->format = f;
	s->w = w;
	s->h = h;
	s->pitch = (w * f->BytesPerPixel + 3) & ~3;
	s->pixels = calloc(1, (size_t)s->pitch * (h > 0 ? h : 1) + 4);
	s->clip_rect.x = 0; s->clip_rect.y = 0; s->clip_rect.w = w; s->clip_rect.h = h;
	s->refcount = 1;
	s->blend_mode = f->Amask ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE;
	s->alpha_mod = 255;
	return s;
}

void SDL_FreeSurface(SDL_Surface* s) {
	if (s == NULL) return;
	if (--s->refcount > 0) return;
	if (s->format) {
		palette_release(s->format->palette);
		free(s->format);
	}
	free(s->pixels);
	free(s);
}

int SDL_LockSurface(SDL_Surface* s) { if (s) s->locked++; return 0; }
void SDL_UnlockSurface(SDL_Surface* s) { if (s && s->locked > 0) s->locked--; }

int SDL_SetColorKey(SDL_Surface* s, int flag, Uint32 key) {
	if (s == NULL) return -1;
	s->colorkey_enabled = flag ? 1 : 0;
	s->colorkey = key;
	return 0;
}

int SDL_SetPaletteColors(SDL_Palette* palette, const SDL_Color* colors, int firstcolor, int ncolors) {
	if (palette == NULL || colors == NULL) return -1;
	int status = 0;
	if (firstcolor < 0) return -1;
	if (firstcolor + ncolors > palette->ncolors) {
		ncolors = palette->ncolors - firstcolor;
		status = -1;
	}
	if (ncolors > 0) memcpy(palette->colors + firstcolor, colors, ncolors * sizeof(SDL_Color));
	palette->version++;
	return status;
}

int SDL_SetSurfacePalette(SDL_Surface* s, SDL_Palette* palette) {
	if (s == NULL || s->format->format != SDL_PIXELFORMAT_INDEX8) return -1;
	if (palette == s->format->palette) return 0;
	if (palette) palette->refcount++;
	palette_release(s->format->palette);
	s->format->palette = palette;
	return 0;
}

int SDL_SetSurfaceBlendMode(SDL_Surface* s, SDL_BlendMode mode) {
	if (s == NULL) return -1;
	s->blend_mode = mode;
	return 0;
}

int SDL_SetSurfaceAlphaMod(SDL_Surface* s, Uint8 alpha) {
	if (s == NULL) return -1;
	s->alpha_mod = alpha;
	return 0;
}

SDL_bool SDL_SetClipRect(SDL_Surface* s, const SDL_Rect* rect) {
	if (s == NULL) return SDL_FALSE;
	SDL_Rect full = {0, 0, s->w, s->h};
	if (rect == NULL) { s->clip_rect = full; return SDL_TRUE; }
	return intersect(full, *rect, &s->clip_rect) ? SDL_TRUE : SDL_FALSE;
}

Uint32 SDL_MapRGBA(const SDL_PixelFormat* f, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
	if (f == NULL) return 0;
	if (f->format == SDL_PIXELFORMAT_INDEX8) return nearest_palette_index(f->palette, r, g, b);
	return ((Uint32)r << f->Rshift) | ((Uint32)g << f->Gshift) | ((Uint32)b << f->Bshift)
		| (f->Amask ? ((Uint32)a << f->Ashift) : 0);
}

Uint32 SDL_MapRGB(const SDL_PixelFormat* f, Uint8 r, Uint8 g, Uint8 b) {
	return SDL_MapRGBA(f, r, g, b, 255);
}

int SDL_FillRect(SDL_Surface* dst, const SDL_Rect* rect, Uint32 color) {
	if (dst == NULL) return -1;
	SDL_Rect r;
	if (rect == NULL) {
		r = dst->clip_rect;
	} else if (!intersect(*rect, dst->clip_rect, &r)) {
		return 0;
	}
	int bpp = dst->format->BytesPerPixel;
	for (int y = r.y; y < r.y + r.h; ++y) {
		Uint8* p = pixel_ptr(dst, r.x, y);
		if (bpp == 1) {
			memset(p, (Uint8)color, r.w);
		} else if (bpp == 4) {
			Uint32* p32 = (Uint32*)p;
			for (int x = 0; x < r.w; ++x) p32[x] = color;
		} else {
			Uint8 c0 = (Uint8)color, c1 = (Uint8)(color >> 8), c2 = (Uint8)(color >> 16);
			for (int x = 0; x < r.w; ++x) { p[0] = c0; p[1] = c1; p[2] = c2; p += 3; }
		}
	}
	return 0;
}

int SDL_BlitSurface(SDL_Surface* src, const SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect) {
	if (src == NULL || dst == NULL) return -1;
	SDL_Rect sr = {0, 0, src->w, src->h};
	if (srcrect != NULL) {
		SDL_Rect full = {0, 0, src->w, src->h};
		if (!intersect(*srcrect, full, &sr)) { if (dstrect) { dstrect->w = dstrect->h = 0; } return 0; }
	}
	int dx = dstrect ? dstrect->x : 0;
	int dy = dstrect ? dstrect->y : 0;
	// Adjust for the part of the source rect that was clipped away.
	if (srcrect != NULL) { dx += sr.x - srcrect->x; dy += sr.y - srcrect->y; }

	SDL_Rect want = {dx, dy, sr.w, sr.h};
	SDL_Rect dr;
	if (!intersect(want, dst->clip_rect, &dr)) {
		if (dstrect) { *dstrect = want; dstrect->w = dstrect->h = 0; }
		return 0;
	}
	sr.x += dr.x - dx;
	sr.y += dr.y - dy;
	sr.w = dr.w;
	sr.h = dr.h;
	if (dstrect) *dstrect = dr;

	const bool keyed = src->colorkey_enabled != 0;
	const Uint32 key = src->colorkey;
	const int mode = src->blend_mode;
	const int alpha_mod = src->alpha_mod;
	const Uint32 sfmt = src->format->format;
	const Uint32 dfmt = dst->format->format;

	// Fast path 1: same format, no key, no blending -> row copies.
	// (Blending an opaque, unmodulated source is a plain copy as well.)
	const bool blend_is_copy = mode == SDL_BLENDMODE_NONE
		|| (mode == SDL_BLENDMODE_BLEND && src->format->Amask == 0 && alpha_mod == 255);
	if (sfmt == dfmt && !keyed && blend_is_copy && sfmt != SDL_PIXELFORMAT_UNKNOWN) {
		size_t row_bytes = (size_t)sr.w * src->format->BytesPerPixel;
		for (int y = 0; y < sr.h; ++y) {
			memmove(pixel_ptr(dst, dr.x, dr.y + y), pixel_ptr(src, sr.x, sr.y + y), row_bytes);
		}
		return 0;
	}

	// Fast path 2: paletted sprite onto the 24-bit screen (the hot path for the game).
	if (sfmt == SDL_PIXELFORMAT_INDEX8 && dfmt == SDL_PIXELFORMAT_RGB24 && mode == SDL_BLENDMODE_NONE) {
		const SDL_Palette* pal = src->format->palette;
		const SDL_PixelFormat* df = dst->format;
		Uint8 lut[256][3];
		int n = pal ? pal->ncolors : 0;
		for (int i = 0; i < 256; ++i) {
			int r = i < n ? pal->colors[i].r : 0, g = i < n ? pal->colors[i].g : 0, b = i < n ? pal->colors[i].b : 0;
			Uint32 v = ((Uint32)r << df->Rshift) | ((Uint32)g << df->Gshift) | ((Uint32)b << df->Bshift);
			lut[i][0] = (Uint8)v; lut[i][1] = (Uint8)(v >> 8); lut[i][2] = (Uint8)(v >> 16);
		}
		for (int y = 0; y < sr.h; ++y) {
			const Uint8* sp = pixel_ptr(src, sr.x, sr.y + y);
			Uint8* dp = pixel_ptr(dst, dr.x, dr.y + y);
			for (int x = 0; x < sr.w; ++x, ++sp, dp += 3) {
				Uint8 idx = *sp;
				if (keyed && idx == key) continue;
				dp[0] = lut[idx][0]; dp[1] = lut[idx][1]; dp[2] = lut[idx][2];
			}
		}
		return 0;
	}

	// Paletted onto paletted: copy indices (palettes are shared or identical in practice).
	if (sfmt == SDL_PIXELFORMAT_INDEX8 && dfmt == SDL_PIXELFORMAT_INDEX8) {
		for (int y = 0; y < sr.h; ++y) {
			const Uint8* sp = pixel_ptr(src, sr.x, sr.y + y);
			Uint8* dp = pixel_ptr(dst, dr.x, dr.y + y);
			for (int x = 0; x < sr.w; ++x) {
				if (keyed && sp[x] == key) continue;
				dp[x] = sp[x];
			}
		}
		return 0;
	}

	// General path.
	const int sbpp = src->format->BytesPerPixel;
	const int dbpp = dst->format->BytesPerPixel;
	for (int y = 0; y < sr.h; ++y) {
		const Uint8* sp = pixel_ptr(src, sr.x, sr.y + y);
		Uint8* dp = pixel_ptr(dst, dr.x, dr.y + y);
		for (int x = 0; x < sr.w; ++x, sp += sbpp, dp += dbpp) {
			Uint32 raw = read_raw(src, sp);
			if (keyed && raw == key) continue;
			Rgba c = raw_to_rgba(src, raw);
			if (sfmt == SDL_PIXELFORMAT_INDEX8) c.a = 255; // palette alpha is not a source of transparency
			if (mode == SDL_BLENDMODE_NONE) {
				write_raw(dst, dp, rgba_to_raw(dst, c));
			} else {
				c.a = c.a * alpha_mod / 255;
				blend_pixel(dst, dp, c, mode);
			}
		}
	}
	return 0;
}

int SDL_BlitScaled(SDL_Surface* src, const SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect) {
	if (src == NULL || dst == NULL) return -1;
	SDL_Rect sr = {0, 0, src->w, src->h};
	if (srcrect) sr = *srcrect;
	SDL_Rect dr = {0, 0, dst->w, dst->h};
	if (dstrect) dr = *dstrect;
	if (sr.w <= 0 || sr.h <= 0 || dr.w <= 0 || dr.h <= 0) return 0;
	const int sbpp = src->format->BytesPerPixel;
	const int dbpp = dst->format->BytesPerPixel;
	const bool same = src->format->format == dst->format->format;
	for (int y = 0; y < dr.h; ++y) {
		int ty = dr.y + y;
		if (ty < dst->clip_rect.y || ty >= dst->clip_rect.y + dst->clip_rect.h) continue;
		int sy = sr.y + (int)((long long)y * sr.h / dr.h);
		if (sy < 0 || sy >= src->h) continue;
		for (int x = 0; x < dr.w; ++x) {
			int tx = dr.x + x;
			if (tx < dst->clip_rect.x || tx >= dst->clip_rect.x + dst->clip_rect.w) continue;
			int sx = sr.x + (int)((long long)x * sr.w / dr.w);
			if (sx < 0 || sx >= src->w) continue;
			const Uint8* sp = pixel_ptr(src, sx, sy);
			Uint8* dp = pixel_ptr(dst, tx, ty);
			if (same) {
				memcpy(dp, sp, sbpp);
			} else {
				write_raw(dst, dp, rgba_to_raw(dst, raw_to_rgba(src, read_raw(src, sp))));
			}
			(void)dbpp;
		}
	}
	return 0;
}

SDL_Surface* SDL_ConvertSurface(SDL_Surface* src, const SDL_PixelFormat* fmt, Uint32 flags) {
	(void)flags;
	if (src == NULL || fmt == NULL) return NULL;
	SDL_Surface* out = SDL_CreateRGBSurface(0, src->w, src->h, fmt->BitsPerPixel, fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
	if (out == NULL) return NULL;
	const bool src_indexed = src->format->format == SDL_PIXELFORMAT_INDEX8;
	const bool dst_indexed = out->format->format == SDL_PIXELFORMAT_INDEX8;
	if (dst_indexed) {
		// Use the palette of the requested format, or of the source when converting index to index.
		SDL_Palette* pal = fmt->palette ? fmt->palette : (src_indexed ? src->format->palette : NULL);
		if (pal) SDL_SetSurfacePalette(out, pal);
	}
	const bool keyed = src->colorkey_enabled != 0;
	const Uint32 key = src->colorkey;
	const bool dst_has_alpha = out->format->Amask != 0;
	const int sbpp = src->format->BytesPerPixel;
	const int dbpp = out->format->BytesPerPixel;
	for (int y = 0; y < src->h; ++y) {
		const Uint8* sp = pixel_ptr(src, 0, y);
		Uint8* dp = pixel_ptr(out, 0, y);
		if (src_indexed && dst_indexed) {
			memcpy(dp, sp, src->w);
			continue;
		}
		for (int x = 0; x < src->w; ++x, sp += sbpp, dp += dbpp) {
			Uint32 raw = read_raw(src, sp);
			Rgba c = raw_to_rgba(src, raw);
			if (src_indexed) c.a = 255;
			if (keyed && raw == key) {
				if (dst_has_alpha) c.a = 0;
			}
			write_raw(out, dp, rgba_to_raw(out, c));
		}
	}
	if (keyed) {
		if (src->format->format == out->format->format) {
			SDL_SetColorKey(out, 1, key);
		} else if (!dst_has_alpha) {
			Rgba c = raw_to_rgba(src, key);
			SDL_SetColorKey(out, 1, rgba_to_raw(out, c));
		}
	}
	out->blend_mode = src->blend_mode;
	if (dst_has_alpha && !src->format->Amask) out->blend_mode = SDL_BLENDMODE_BLEND;
	out->alpha_mod = src->alpha_mod;
	return out;
}

SDL_Surface* SDL_ConvertSurfaceFormat(SDL_Surface* src, Uint32 pixel_format, Uint32 flags) {
	SDL_PixelFormat f;
	memset(&f, 0, sizeof(f));
	switch (pixel_format) {
		case SDL_PIXELFORMAT_ARGB8888:
			f.BitsPerPixel = 32; f.Rmask = 0x00FF0000; f.Gmask = 0x0000FF00; f.Bmask = 0x000000FF; f.Amask = 0xFF000000; break;
		case SDL_PIXELFORMAT_ABGR8888:
			f.BitsPerPixel = 32; f.Rmask = 0x000000FF; f.Gmask = 0x0000FF00; f.Bmask = 0x00FF0000; f.Amask = 0xFF000000; break;
		case SDL_PIXELFORMAT_RGB24:
			f.BitsPerPixel = 24; f.Rmask = 0x0000FF; f.Gmask = 0x00FF00; f.Bmask = 0xFF0000; break;
		case SDL_PIXELFORMAT_INDEX8:
			f.BitsPerPixel = 8; break;
		default:
			return NULL;
	}
	f.format = pixel_format;
	return SDL_ConvertSurface(src, &f, flags);
}

} // extern "C"
