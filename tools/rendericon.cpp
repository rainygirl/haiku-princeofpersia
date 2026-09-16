// Renders an HVIF icon file (raw vector_icon data) to a PNG for checking. Haiku only.
// Build: setarch x86 g++ -o rendericon rendericon.cpp -lbe -ltranslation
#include <Application.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <File.h>
#include <IconUtils.h>
#include <TranslatorRoster.h>
#include <cstdio>
#include <vector>
int main(int argc, char** argv)
{
	if (argc < 4) { fprintf(stderr, "usage: rendericon icon.hvif out.png size\n"); return 1; }
	BApplication app("application/x-vnd.rainygirl-rendericon");   // BBitmap needs an app_server connection
	BFile in(argv[1], B_READ_ONLY); off_t n; in.GetSize(&n); std::vector<uint8> data(n); in.Read(data.data(), n);
	int size = atoi(argv[3]);
	BBitmap bmp(BRect(0, 0, size - 1, size - 1), B_RGBA32);
	status_t s = BIconUtils::GetVectorIcon(data.data(), n, &bmp);
	printf("GetVectorIcon: %s\n", strerror(s));
	if (s != B_OK) return 1;
	BFile out(argv[2], B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	BBitmapStream stream(&bmp);
	s = BTranslatorRoster::Default()->Translate(&stream, NULL, NULL, &out, B_PNG_FORMAT);
	BBitmap* keep; stream.DetachBitmap(&keep);
	printf("png: %s\n", strerror(s));
	return s == B_OK ? 0 : 1;
}
