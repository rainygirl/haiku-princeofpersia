/*
 * Byte streams for the Prince of Persia engine: files (save games, settings,
 * replays) and memory buffers (option snapshots taken before a replay).
 */
#include <SDL2/SDL.h>

#include <sys/stat.h>

struct SDL_RWops {
	FILE* fp;
	Uint8* mem;
	Sint64 mem_size;
	Sint64 mem_pos;
	int writable;
};

extern "C" {

SDL_RWops* SDL_RWFromFile(const char* file, const char* mode) {
	if (file == NULL || mode == NULL) return NULL;
	FILE* fp = fopen(file, mode);
	if (fp == NULL) return NULL;
	SDL_RWops* rw = (SDL_RWops*)calloc(1, sizeof(SDL_RWops));
	rw->fp = fp;
	rw->writable = (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL || strchr(mode, '+') != NULL);
	return rw;
}

SDL_RWops* SDL_RWFromMem(void* mem, int size) {
	if (mem == NULL || size < 0) return NULL;
	SDL_RWops* rw = (SDL_RWops*)calloc(1, sizeof(SDL_RWops));
	rw->mem = (Uint8*)mem;
	rw->mem_size = size;
	rw->writable = 1;
	return rw;
}

SDL_RWops* SDL_RWFromConstMem(const void* mem, int size) {
	SDL_RWops* rw = SDL_RWFromMem((void*)mem, size);
	if (rw) rw->writable = 0;
	return rw;
}

size_t SDL_RWread(SDL_RWops* ctx, void* ptr, size_t size, size_t maxnum) {
	if (ctx == NULL || ptr == NULL || size == 0) return 0;
	if (ctx->fp) return fread(ptr, size, maxnum, ctx->fp);
	Sint64 avail = ctx->mem_size - ctx->mem_pos;
	if (avail <= 0) return 0;
	size_t num = (size_t)(avail / (Sint64)size);
	if (num > maxnum) num = maxnum;
	memcpy(ptr, ctx->mem + ctx->mem_pos, num * size);
	ctx->mem_pos += (Sint64)(num * size);
	return num;
}

size_t SDL_RWwrite(SDL_RWops* ctx, const void* ptr, size_t size, size_t num) {
	if (ctx == NULL || ptr == NULL || size == 0 || !ctx->writable) return 0;
	if (ctx->fp) return fwrite(ptr, size, num, ctx->fp);
	Sint64 avail = ctx->mem_size - ctx->mem_pos;
	if (avail <= 0) return 0;
	size_t n = (size_t)(avail / (Sint64)size);
	if (n > num) n = num;
	memcpy(ctx->mem + ctx->mem_pos, ptr, n * size);
	ctx->mem_pos += (Sint64)(n * size);
	return n;
}

Sint64 SDL_RWtell(SDL_RWops* ctx) {
	if (ctx == NULL) return -1;
	if (ctx->fp) return (Sint64)ftell(ctx->fp);
	return ctx->mem_pos;
}

int SDL_RWclose(SDL_RWops* ctx) {
	if (ctx == NULL) return -1;
	int result = 0;
	if (ctx->fp && fclose(ctx->fp) != 0) result = -1;
	free(ctx);
	return result;
}

// Helpers for the image loader.
Sint64 pop_rw_size(SDL_RWops* ctx) {
	if (ctx == NULL) return -1;
	if (ctx->fp) {
		struct stat st;
		if (fstat(fileno(ctx->fp), &st) != 0) return -1;
		return (Sint64)st.st_size;
	}
	return ctx->mem_size;
}

int pop_rw_seek_start(SDL_RWops* ctx) {
	if (ctx == NULL) return -1;
	if (ctx->fp) return fseek(ctx->fp, 0, SEEK_SET);
	ctx->mem_pos = 0;
	return 0;
}

} // extern "C"
