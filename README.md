# WinRT OCR

[English](README.en.md) | **한국어**

Windows 내장 OCR 엔진(`Windows.Media.Ocr`)을 **.NET 런타임 없이** 사용할 수 있게 해주는 네이티브 C++ DLL(`sc_ocr.dll`)과 Delphi VCL 데모 애플리케이션입니다.

## 특징

- **런타임 의존성 제로** — CRT 정적 링크 단일 DLL (약 175KB). .NET, VC 재배포 패키지, COM 등록 모두 불필요
- **x64 / x86 지원** — `sc_ocr.dll` (64비트), `sc_ocr32.dll` (32비트)
- **평면 C API** — UTF-16 + `stdcall`, Delphi·C·C# 등 어떤 언어에서도 호출 가능
- **스레드 안전** — 내부 MTA 워커 스레드로 격리되어 호출 스레드의 COM 초기화가 필요 없음
- **Delphi 래퍼 포함** — `SCOcr.pas` 의 `TSCOcr` 클래스로 한 줄 호출
- OCR 엔진은 Windows 내장이므로 별도 학습 데이터·외부 라이브러리가 필요 없음

## 요구 사항

| 항목 | 내용 |
|---|---|
| 실행 | Windows 10 (1507, 빌드 10240) 이상 |
| OCR 언어 | Windows 언어팩의 OCR 기능 설치 필요 (설정 → 시간 및 언어 → 언어) |
| DLL 빌드 | Visual Studio 2022 (C++/WinRT, 헤더 온리) |
| 데모 앱 빌드 | Delphi 13 (Win64) |

## 폴더 구조

```
├─ src\            Delphi 소스 (데모 앱 + SCOcr.pas 래퍼)
├─ cpp\            C++ DLL 소스 (sc_ocr.cpp, sc_ocr.def)
├─ bin\            빌드 산출물 (exe, dll) — Release 에서 다운로드
├─ dcu\            Delphi 중간 산출물
└─ build_cpp.bat   DLL 빌드 스크립트 (x64 + x86)
```

## 빌드

```bat
rem 1) C++ DLL (x64 + x86 동시 빌드 → bin\)
build_cpp.bat

rem 2) Delphi 데모 앱 (src\ 에서)
dcc64 -B -E..\bin -N0..\dcu WinRTOCR.dpr
```

## DLL API

모든 문자열은 UTF-16(`wchar_t*`), 호출 규약은 `stdcall` 입니다.
텍스트 반환 함수는 2단계 호출 패턴을 사용합니다: 버퍼에 `null` 을 넘기면 필요한 문자 수(널 제외)를 반환하고, 버퍼를 넘기면 복사 후 전체 길이를 반환합니다.

| 함수 | 설명 |
|---|---|
| `SC_OcrExtractText(imagePath, langCode, buffer, bufferSize)` | 이미지 파일에서 텍스트 추출. `langCode` 가 빈 문자열이면 사용자 프로필 언어 사용 |
| `SC_OcrGetAvailableLanguages(buffer, bufferSize)` | OCR 가능한 언어 태그 목록 (CRLF 구분) |
| `SC_OcrIsLanguageSupported(langCode)` | 언어 지원 여부 (1 = 지원, 0 = 미지원) |
| `SC_OcrGetLastError(buffer, bufferSize)` | 호출 스레드의 마지막 오류 메시지 |

**오류 코드** (음수 반환값): `-1` 일반 오류 · `-2` 언어팩 미설치 · `-3` 엔진 생성 실패 · `-4` 잘못된 인자

## Delphi 사용 예

```pascal
uses
  SCOcr;

// 한국어 OCR (sc_ocr.dll 은 exe 와 같은 폴더에 배치)
var LText := TSCOcr.ExtractText('C:\images\sample.png', 'ko-KR');

// 시스템 기본 언어로 인식
var LAuto := TSCOcr.ExtractText('C:\images\sample.png');

// 사용 가능한 언어 확인
for var LTag in TSCOcr.GetAvailableLanguages do
begin
  Writeln(LTag);
end;
```

오류는 `ESCOcrError` 예외로 전달됩니다. 워커 스레드에서 호출해도 안전합니다.

## C 사용 예

```c
wchar_t buf[65536];
int len = SC_OcrExtractText(L"C:\\images\\sample.png", L"en-US", buf, 65536);
if (len < 0)
{
    wchar_t err[1024];
    SC_OcrGetLastError(err, 1024);
}
```

## 라이선스

[MIT License](LICENSE)
