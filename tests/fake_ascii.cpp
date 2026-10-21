struct AsciiString { const char* p; AsciiString(const char* s); };
AsciiString::AsciiString(const char* s) : p(s) {}
