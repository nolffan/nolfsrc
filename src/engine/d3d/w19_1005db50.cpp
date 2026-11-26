// *********************************************************************** //
//
//  w19_1005db50.cpp -- 0x1005db50 (55 bytes), slot +0x90 of the g_100895b0
//  view region (a render-module descriptor's shutdown hook).  /O2: the
//  shipped body pads to 16 with `mov %esi,%esi; lea 0x0(%edi,%eiz,1),%edi`.
//
//  It releases the two scratch buffers big5a960.cpp grows on demand --
//  the vertex cache 0x1008fe60 (capacity 0x1008fe9c) and the index
//  scratch 0x1008fe64 (capacity 0x1008fea0) -- through 0x10021cb0 (the
//  renderer's free, big5a960.cpp's spelling), then clears all four.  The
//  four zero stores are written in the shipped order (a0, 64, 9c, 60).
//
// *********************************************************************** //

typedef unsigned long  uint32;
typedef unsigned short uint16;

struct Vert5a960;

extern Vert5a960*   g_p1008fe60;                        // 0x1008fe60
extern uint16*      g_p1008fe64;                        // 0x1008fe64
extern uint32       g_n1008fe9c;                        // 0x1008fe9c
extern uint32       g_n1008fea0;                        // 0x1008fea0

extern "C" void sub_10021cb0(void* p);                  // 0x10021cb0

void sub_1005db50()
{
	sub_10021cb0(g_p1008fe60);
	sub_10021cb0(g_p1008fe64);

	// In the opposite order to the shipped stores (fea0, fe64, fe9c,
	// fe60): VC6 writes a chain of stores of one register from the last.
	g_p1008fe60 = 0;
	g_n1008fe9c = 0;
	g_p1008fe64 = 0;
	g_n1008fea0 = 0;
}
