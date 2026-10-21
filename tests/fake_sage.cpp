// Minimal stand-in for a SAGE exe: FieldParse/BlockParse tables, a module registry, vtables.
typedef void (*ParseProc)(void*, void*, void*, const void*);
struct FieldParse { const char* token; ParseProc parse; const void* userData; int offset; };
struct BlockParse { const char* token; void (*parse)(void*); };
struct AsciiString { const char* p; AsciiString(const char* s); };
extern "C" int g_sink;
int g_sink;
__declspec(noinline) void parseReal(void*, void*, void* s, const void*) { g_sink += 1; }
__declspec(noinline) void parseInt(void*, void*, void* s, const void*) { g_sink += 2; }
__declspec(noinline) void parseAscii(void*, void*, void* s, const void*) { g_sink += 3; }
__declspec(noinline) void parseObj(void*) { g_sink += 4; }
__declspec(noinline) void parseWeap(void*) { g_sink += 5; }
struct ActiveBodyModuleData { float a, b, c; };
const FieldParse activeBodyTable[] = {
  {"MaxHealth", parseReal, 0, 4}, {"InitialHealth", parseReal, 0, 8},
  {"SubdualDamageCap", parseReal, 0, 12}, {"SubdualDamageHealRate", parseInt, 0, 16},
  {"SubdualDamageHealAmount", parseReal, 0, 20}, {"HordeHealthBonus", parseInt, 0, 24}, {0,0,0,0}};
const FieldParse fakeGlobal[] = {
  {"UnicodeFontName", parseAscii, 0, 0}, {"LocalFontFile", parseAscii, 0, 4},
  {"MilitaryCaptionTitleFont", parseAscii, 0, 8}, {"DrawableCaptionFont", parseAscii, 0, 12}, {0,0,0,0}};
const BlockParse typeTable[] = { {"Armor", parseObj}, {"Object", parseObj}, {"Weapon", parseWeap},
  {"FireLogicSystem", parseWeap}, {0,0} };
struct Module { virtual ~Module(){} virtual int f1(){return 1;} virtual int f2(){return 2;} };
struct ActiveBody : Module { int x; __declspec(noinline) ActiveBody(){x=1;} int f1(){return 11;} int f2(){return g_sink;} };
struct HordeContain : Module { int y; __declspec(noinline) HordeContain(){y=2;} int f1(){return 21;} int f2(){return g_sink*2;} };
__declspec(noinline) Module* ab_new(){ return new ActiveBody; }
__declspec(noinline) void* ab_data(){ return new ActiveBodyModuleData; }
__declspec(noinline) Module* hc_new(){ return new HordeContain; }
__declspec(noinline) void* hc_data(){ return new int[3]; }
typedef Module*(*NewProc)(); typedef void*(*NewData)();
struct ModuleFactory {
  __declspec(noinline) void addModuleInternal(NewProc p, NewData d, int type, const AsciiString& n, int i) { g_sink += (int)(long)p + type + i + (int)(long)n.p; }
  __declspec(noinline) void init();
};
void ModuleFactory::init() {
  addModuleInternal(ab_new, ab_data, 1, AsciiString("ActiveBody"), 0);
  addModuleInternal(hc_new, hc_data, 2, AsciiString("HordeContain"), 0);
}
void* operator new(unsigned n){ static char heap[4096]; static unsigned u; void* r = heap+u; u+= (n+15)&~15u; return r; }
void* operator new[](unsigned n){ return operator new(n); }
void operator delete(void*, unsigned){} void operator delete(void*){}
extern "C" int mainCRTStartup(){ ModuleFactory f; f.init(); return g_sink + (int)(long)activeBodyTable + (int)(long)fakeGlobal + (int)(long)typeTable; }
