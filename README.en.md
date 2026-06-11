# WinRT OCR

**English** | [한국어](README.md)

A native C++ DLL (`sc_ocr.dll`) that exposes the built-in Windows OCR engine (`Windows.Media.Ocr`) **without any .NET runtime**, plus a Delphi VCL demo application.

## Features

- **Zero runtime dependencies** — single DLL with statically linked CRT (~175KB). No .NET, no VC redistributable, no COM registration
- **x64 / x86** — `sc_ocr.dll` (64-bit), `sc_ocr32.dll` (32-bit)
- **Flat C API** — UTF-16 + `stdcall`, callable from Delphi, C, C#, or any language with FFI
- **Thread-safe** — OCR work is isolated on an internal MTA worker thread; callers do not need to initialize COM
- **Delphi wrapper included** — one-line calls via the `TSCOcr` class in `SCOcr.pas`
- **Headless CLI included** — `winocr.exe` for scripts and batch jobs
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

## Command-Line Tool (winocr)

A headless CLI for scripting. Place it next to `sc_ocr.dll`.

```bat
winocr scan.png                       rem print text to stdout (pipe-friendly)
winocr scan.png -l ko-KR -o out.txt   rem Korean OCR, save as UTF-8 file
winocr --list-langs                   rem list available OCR languages
winocr --help                         rem usage
```

| Option | Description |
|---|---|
| `-i, --input <file>` | Input image (also accepted as a positional argument) |
| `-o, --output <file>` | Save result to a UTF-8 file (default: stdout) |
| `-l, --lang <tag>` | OCR language (BCP-47). Defaults to user profile languages |
| `--list-langs` | List available OCR languages |
| `-v, --version` / `-h, --help` | Version / help |

Exit codes: `0` success · `1` OCR error · `2` usage error

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

## Windows Defender False Positives

Freshly built, unsigned executables may be **falsely flagged** as malware by Windows Defender's machine-learning heuristics (e.g. `Trojan:Win32/Wacatac.A!ml`) and quarantined or deleted automatically. The profile "small unsigned console exe + static linking + no reputation" statistically overlaps with malware loaders.

- `winocr.exe` **dynamically loads** `sc_ocr.dll` instead of statically linking the OCR core, which generally avoids the false positive. It may still recur as ML models are updated.
- If a file disappears during development, diagnose and fix it with:

```powershell
# Check detection history
Get-MpThreatDetection | Where-Object { $_.Resources -match 'winrtOcr' }

# Add the project folder as an exclusion (admin PowerShell)
Add-MpPreference -ExclusionPath 'D:\PROJECTS\winrtOcr'
```

For distribution, the real fix is signing with a **code-signing certificate** to build reputation.

## License

[MIT License](LICENSE)
