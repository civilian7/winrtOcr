# WinRT OCR

**English** | [한국어](README.md)

A native C++ DLL (`sc_ocr.dll`) that exposes the built-in Windows OCR engine (`Windows.Media.Ocr`) **without any .NET runtime**, plus a Delphi VCL demo application.

## Features

- **Zero runtime dependencies** — single DLL with statically linked CRT (~175KB). No .NET, no VC redistributable, no COM registration
- **x64 / x86** — `sc_ocr.dll` (64-bit), `sc_ocr32.dll` (32-bit)
- **Flat C API** — UTF-16 + `stdcall`, callable from Delphi, C, C#, or any language with FFI
- **Thread-safe** — OCR work is isolated on an internal MTA worker thread; callers do not need to initialize COM
- **Delphi wrapper included** — one-line calls via the `TSCOcr` class in `SCOcr.pas`
- The OCR engine ships with Windows itself — no training data or external libraries required

## Requirements

| Item | Details |
|---|---|
| Runtime | Windows 10 (1507, build 10240) or later |
| OCR languages | Windows language pack with OCR feature installed (Settings → Time & Language → Language) |
| DLL build | Visual Studio 2022 (C++/WinRT, header-only) |
| Demo app build | Delphi 13 (Win64) |

## Project Layout

```
├─ src\            Delphi sources (demo app + SCOcr.pas wrapper)
├─ cpp\            C++ DLL sources (sc_ocr.cpp, sc_ocr.def)
├─ bin\            Build outputs (exe, dll) — download from Releases
├─ dcu\            Delphi intermediate files
└─ build_cpp.bat   DLL build script (x64 + x86)
```

## Building

```bat
rem 1) C++ DLL (builds x64 + x86 into bin\)
build_cpp.bat

rem 2) Delphi demo app (from src\)
dcc64 -B -E..\bin -N0..\dcu WinRTOCR.dpr
```

## DLL API

All strings are UTF-16 (`wchar_t*`); the calling convention is `stdcall`.
Text-returning functions use a two-call pattern: pass `null` as the buffer to query the required length (excluding the terminator), then call again with a buffer to receive the text.

| Function | Description |
|---|---|
| `SC_OcrExtractText(imagePath, langCode, buffer, bufferSize)` | Extract text from an image file. Empty `langCode` uses the user profile languages |
| `SC_OcrGetAvailableLanguages(buffer, bufferSize)` | List of OCR-capable language tags (CRLF-separated) |
| `SC_OcrIsLanguageSupported(langCode)` | Whether the language is supported (1 = yes, 0 = no) |
| `SC_OcrGetLastError(buffer, bufferSize)` | Last error message for the calling thread |

**Error codes** (negative return values): `-1` general error · `-2` language pack not installed · `-3` engine creation failed · `-4` invalid argument

## Delphi Example

```pascal
uses
  SCOcr;

// Korean OCR (place sc_ocr.dll next to the exe)
var LText := TSCOcr.ExtractText('C:\images\sample.png', 'ko-KR');

// Recognize with the system default language
var LAuto := TSCOcr.ExtractText('C:\images\sample.png');

// Enumerate available languages
for var LTag in TSCOcr.GetAvailableLanguages do
begin
  Writeln(LTag);
end;
```

Errors are raised as `ESCOcrError` exceptions. Safe to call from worker threads.

## C Example

```c
wchar_t buf[65536];
int len = SC_OcrExtractText(L"C:\\images\\sample.png", L"en-US", buf, 65536);
if (len < 0)
{
    wchar_t err[1024];
    SC_OcrGetLastError(err, 1024);
}
```

## License

[MIT License](LICENSE)
