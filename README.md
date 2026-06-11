# WinRT OCR

[English](README.en.md) | **한국어**

Windows 내장 OCR 엔진(`Windows.Media.Ocr`)을 **.NET 런타임 없이** 사용할 수 있게 해주는 네이티브 C++ DLL(`sc_ocr.dll`)과 Delphi VCL 데모 애플리케이션입니다.

## 특징

- **런타임 의존성 제로** — CRT 정적 링크 단일 DLL (약 175KB). .NET, VC 재배포 패키지, COM 등록 모두 불필요
- **x64 / x86 지원** — `sc_ocr.dll` (64비트), `sc_ocr32.dll` (32비트)
- **평면 C API** — UTF-16 + `stdcall`, Delphi·C·C# 등 어떤 언어에서도 호출 가능
- **스레드 안전** — 내부 MTA 워커 스레드로 격리되어 호출 스레드의 COM 초기화가 필요 없음
- **Delphi 래퍼 포함** — `SCOcr.pas` 의 `TSCOcr` 클래스로 한 줄 호출
- **headless CLI 포함** — `winocr.exe` 로 스크립트/배치에서 바로 OCR
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

## 명령줄 도구 (winocr)

GUI 없이 스크립트에서 사용할 수 있는 headless CLI 입니다. `sc_ocr.dll` 과 같은 폴더에 두고 실행합니다.

```bat
winocr scan.png                       rem 텍스트를 표준 출력으로 (파이프 가능)
winocr scan.png -l ko-KR -o out.txt   rem 한국어 인식 후 UTF-8 파일 저장
winocr --list-langs                   rem 사용 가능한 OCR 언어 목록
winocr --help                         rem 사용법
```

| 옵션 | 설명 |
|---|---|
| `-i, --input <파일>` | 입력 이미지 (위치 인자로도 지정 가능) |
| `-o, --output <파일>` | 결과를 UTF-8 파일로 저장 (생략 시 표준 출력) |
| `-l, --lang <태그>` | OCR 언어 (BCP-47). 생략 시 사용자 프로필 언어 |
| `--list-langs` | 사용 가능한 OCR 언어 목록 |
| `-v, --version` / `-h, --help` | 버전 / 도움말 |

종료 코드: `0` 성공 · `1` OCR 오류 · `2` 사용법 오류

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

## Windows Defender 오탐 안내

갓 빌드된 무서명 실행 파일은 Windows Defender의 머신러닝 휴리스틱이 멀웨어로 **오탐**(`Trojan:Win32/Wacatac.A!ml` 등)하여 자동 격리·삭제할 수 있습니다. "작은 무서명 콘솔 exe + 정적 링크 + 평판 없음"이라는 특징이 멀웨어 로더와 통계적으로 겹치기 때문입니다.

- `winocr.exe` 는 OCR 본체를 정적 링크하지 않고 `sc_ocr.dll` 을 **동적 로드**하는 구조라 일반적으로 오탐을 피합니다. 그래도 ML 모델 업데이트에 따라 재발할 수 있습니다.
- 개발 중 파일이 사라지면 다음으로 확인·해결할 수 있습니다:

```powershell
# 탐지 기록 확인
Get-MpThreatDetection | Where-Object { $_.Resources -match 'winrtOcr' }

# 프로젝트 폴더를 제외 경로로 등록 (관리자 PowerShell)
Add-MpPreference -ExclusionPath 'D:\PROJECTS\winrtOcr'
```

배포 시 근본 해결책은 **코드 서명 인증서**로 서명해 평판을 확보하는 것입니다.

## 라이선스

[MIT License](LICENSE)
