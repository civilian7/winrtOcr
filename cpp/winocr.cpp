// winocr.exe — Windows 내장 OCR 을 사용하는 headless 명령줄 도구 (x64 전용)
//
// OCR 본체는 sc_ocr.dll 을 동적 로드해 사용한다 (exe 와 같은 폴더에 배치).
// 정적 링크(C++/WinRT 내장) 빌드는 Windows Defender ML 휴리스틱(Wacatac.A!ml)이
// 오탐으로 삭제하는 것이 확인되어 DLL 연동 구조를 사용한다.
//
// 사용 예:
//   winocr image.png                      텍스트를 표준 출력으로
//   winocr image.png -l ko-KR -o out.txt  한국어 인식 후 UTF-8 파일 저장
//   winocr --list-langs                   사용 가능한 OCR 언어 출력
//
// 종료 코드: 0 성공, 1 OCR 오류, 2 사용법 오류

#include <windows.h>
#include <string>
#include <vector>

namespace
{
    constexpr wchar_t APP_VERSION[] = L"1.1.0";
    constexpr wchar_t OCR_DLL_NAME[] = L"sc_ocr.dll";

    const wchar_t USAGE_TEXT[] =
        L"winocr — Windows 내장 OCR 명령줄 도구\r\n"
        L"\r\n"
        L"사용법:\r\n"
        L"  winocr <이미지 파일> [옵션]\r\n"
        L"\r\n"
        L"옵션:\r\n"
        L"  -i, --input <파일>    입력 이미지 파일 (위치 인자로도 지정 가능)\r\n"
        L"  -o, --output <파일>   결과를 UTF-8 파일로 저장 (생략 시 표준 출력)\r\n"
        L"  -l, --lang <태그>     OCR 언어 (BCP-47, 예: ko-KR, en-US)\r\n"
        L"                        생략 시 사용자 프로필 언어 사용\r\n"
        L"      --list-langs      사용 가능한 OCR 언어 목록 출력\r\n"
        L"  -v, --version         버전 표시\r\n"
        L"  -h, --help            이 도움말 표시\r\n"
        L"\r\n"
        L"예시:\r\n"
        L"  winocr scan.png\r\n"
        L"  winocr scan.png -l ko-KR -o result.txt\r\n"
        L"\r\n"
        L"종료 코드: 0 성공, 1 OCR 오류, 2 사용법 오류\r\n"
        L"필요 파일: sc_ocr.dll (winocr.exe 와 같은 폴더)\r\n";

    // sc_ocr.dll 함수 시그니처
    typedef int(__stdcall* TExtractText)(const wchar_t*, const wchar_t*, wchar_t*, int);
    typedef int(__stdcall* TGetLangs)(wchar_t*, int);
    typedef int(__stdcall* TGetLastError)(wchar_t*, int);

    TExtractText g_extractText = nullptr;
    TGetLangs g_getLangs = nullptr;
    TGetLastError g_getLastError = nullptr;

    // 콘솔이면 WriteConsoleW(유니코드 안전), 리다이렉트면 UTF-8 바이트로 출력
    void WriteStream(DWORD stdHandle, const std::wstring& text)
    {
        HANDLE handle = GetStdHandle(stdHandle);
        DWORD mode = 0;
        DWORD written = 0;

        if (GetConsoleMode(handle, &mode))
        {
            WriteConsoleW(handle, text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
            return;
        }

        const int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
            nullptr, 0, nullptr, nullptr);
        std::vector<char> utf8(static_cast<size_t>(len));
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
            utf8.data(), len, nullptr, nullptr);
        WriteFile(handle, utf8.data(), static_cast<DWORD>(len), &written, nullptr);
    }

    void WriteOut(const std::wstring& text)
    {
        WriteStream(STD_OUTPUT_HANDLE, text);
    }

    void WriteErr(const std::wstring& text)
    {
        WriteStream(STD_ERROR_HANDLE, text);
    }

    // exe 와 같은 폴더의 sc_ocr.dll 로드 (검색 경로 하이재킹 방지를 위해 전체 경로 사용)
    bool LoadOcrDll()
    {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring dllPath{ exePath };
        const size_t slash = dllPath.find_last_of(L'\\');
        dllPath = dllPath.substr(0, slash + 1) + OCR_DLL_NAME;

        HMODULE lib = LoadLibraryW(dllPath.c_str());
        if (!lib)
        {
            return false;
        }

        g_extractText = (TExtractText)GetProcAddress(lib, "SC_OcrExtractText");
        g_getLangs = (TGetLangs)GetProcAddress(lib, "SC_OcrGetAvailableLanguages");
        g_getLastError = (TGetLastError)GetProcAddress(lib, "SC_OcrGetLastError");
        return g_extractText && g_getLangs && g_getLastError;
    }

    // DLL 의 마지막 오류 메시지 조회
    std::wstring GetOcrError()
    {
        const int len = g_getLastError(nullptr, 0);
        if (len <= 0)
        {
            return L"알 수 없는 오류";
        }

        std::wstring msg(static_cast<size_t>(len), L'\0');
        g_getLastError(msg.data(), len + 1);
        return msg;
    }

    // 2단계 호출 패턴 공통 처리 (길이 조회 → 본문 수신). 실패 시 빈 문자열 + ok=false
    std::wstring CallTwoPhase(int(__stdcall* queryFn)(wchar_t*, int), bool& ok)
    {
        ok = false;
        const int len = queryFn(nullptr, 0);
        if (len < 0)
        {
            return L"";
        }

        ok = true;
        if (len == 0)
        {
            return L"";
        }

        std::wstring text(static_cast<size_t>(len), L'\0');
        queryFn(text.data(), len + 1);
        return text;
    }

    // 결과 텍스트를 UTF-8(BOM 포함) 파일로 저장
    bool SaveToFile(const std::wstring& filePath, const std::wstring& text)
    {
        HANDLE file = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        const int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
            nullptr, 0, nullptr, nullptr);
        std::vector<char> utf8(static_cast<size_t>(len));
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
            utf8.data(), len, nullptr, nullptr);

        const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
        DWORD written = 0;
        const bool ok = WriteFile(file, bom, sizeof(bom), &written, nullptr)
            && WriteFile(file, utf8.data(), static_cast<DWORD>(len), &written, nullptr);
        CloseHandle(file);
        return ok;
    }
}

int wmain(int argc, wchar_t* argv[])
{
    std::wstring inputPath;
    std::wstring outputPath;
    std::wstring langCode;
    bool listLangs = false;

    // 인자가 없으면 사용법 출력
    if (argc < 2)
    {
        WriteOut(USAGE_TEXT);
        return 2;
    }

    for (int i = 1; i < argc; ++i)
    {
        const std::wstring arg{ argv[i] };

        auto nextValue = [&](const wchar_t* optName) -> std::wstring
        {
            if (i + 1 >= argc)
            {
                WriteErr(std::wstring(L"오류: ") + optName + L" 옵션에 값이 필요합니다.\r\n");
                ExitProcess(2);
            }

            return argv[++i];
        };

        if (arg == L"-h" || arg == L"--help" || arg == L"/?")
        {
            WriteOut(USAGE_TEXT);
            return 0;
        }
        else if (arg == L"-v" || arg == L"--version")
        {
            WriteOut(std::wstring(L"winocr ") + APP_VERSION + L"\r\n");
            return 0;
        }
        else if (arg == L"--list-langs")
        {
            listLangs = true;
        }
        else if (arg == L"-i" || arg == L"--input")
        {
            inputPath = nextValue(L"--input");
        }
        else if (arg == L"-o" || arg == L"--output")
        {
            outputPath = nextValue(L"--output");
        }
        else if (arg == L"-l" || arg == L"--lang")
        {
            langCode = nextValue(L"--lang");
        }
        else if (!arg.empty() && arg[0] == L'-')
        {
            WriteErr(L"오류: 알 수 없는 옵션입니다: " + arg + L"\r\n'winocr --help' 로 사용법을 확인하세요.\r\n");
            return 2;
        }
        else if (inputPath.empty())
        {
            inputPath = arg; // 위치 인자 = 입력 파일
        }
        else
        {
            WriteErr(L"오류: 입력 파일이 두 번 지정되었습니다: " + arg + L"\r\n");
            return 2;
        }
    }

    if (!LoadOcrDll())
    {
        WriteErr(std::wstring(L"오류: ") + OCR_DLL_NAME + L" 을(를) 로드할 수 없습니다. winocr.exe 와 같은 폴더에 있는지 확인하세요.\r\n");
        return 1;
    }

    if (listLangs)
    {
        bool ok = false;
        const std::wstring langs = CallTwoPhase(g_getLangs, ok);
        if (!ok)
        {
            WriteErr(L"오류: " + GetOcrError() + L"\r\n");
            return 1;
        }

        WriteOut(langs);
        return 0;
    }

    if (inputPath.empty())
    {
        WriteErr(L"오류: 입력 이미지 파일을 지정하세요.\r\n'winocr --help' 로 사용법을 확인하세요.\r\n");
        return 2;
    }

    // OCR 실행 (2단계 호출)
    const int len = g_extractText(inputPath.c_str(), langCode.c_str(), nullptr, 0);
    if (len < 0)
    {
        WriteErr(L"오류: " + GetOcrError() + L"\r\n");
        return 1;
    }

    std::wstring text(static_cast<size_t>(len), L'\0');
    if (len > 0)
    {
        g_extractText(inputPath.c_str(), langCode.c_str(), text.data(), len + 1);
    }

    if (outputPath.empty())
    {
        WriteOut(text);
    }
    else
    {
        if (!SaveToFile(outputPath, text))
        {
            WriteErr(L"오류: 출력 파일을 쓸 수 없습니다: " + outputPath + L"\r\n");
            return 1;
        }
    }

    return 0;
}
