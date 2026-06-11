// sc_ocr.dll — Windows.Media.Ocr(WinRT) 를 평면 C API 로 노출하는 네이티브 래퍼
// Delphi 등 네이티브 코드에서 .NET 런타임 없이 OS 내장 OCR 을 사용하기 위한 DLL
//
// 규약:
//  - 모든 문자열은 UTF-16(wchar_t*), 호출 규약은 stdcall
//  - 텍스트 반환 함수는 2단계 호출 패턴: ABuffer=nullptr 로 호출하면 필요한 문자 수(널 제외)를 반환,
//    버퍼를 넘기면 복사 후 필요한 문자 수를 반환 (버퍼가 작으면 잘라서 복사)
//  - 음수 반환값은 오류 코드. 상세 메시지는 SC_OcrGetLastError 로 조회 (호출 스레드별 보관)

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>
#include <shcore.h>
#include <combaseapi.h>
#include <mutex>
#include <string>
#include <thread>

using namespace winrt;
using namespace winrt::Windows::Globalization;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Storage::Streams;

namespace
{
    // 오류 코드
    constexpr int SC_OCR_OK = 0;
    constexpr int SC_OCR_ERR_GENERAL = -1;            // 일반 오류 (SC_OcrGetLastError 참조)
    constexpr int SC_OCR_ERR_LANG_NOT_SUPPORTED = -2; // 언어 팩 미설치 / 미지원
    constexpr int SC_OCR_ERR_NO_ENGINE = -3;          // OCR 엔진 생성 실패
    constexpr int SC_OCR_ERR_INVALID_ARG = -4;        // 잘못된 인자

    // 호출 스레드별 마지막 오류 메시지
    thread_local std::wstring g_lastError;

    // 코드와 메시지를 함께 던지는 내부 예외
    struct OcrError
    {
        int Code;
        std::wstring Message;
    };

    // 결과 문자열을 호출자 버퍼에 복사. 반환값은 필요한 문자 수(널 제외)
    int CopyToBuffer(const std::wstring& text, wchar_t* buffer, int bufferSize)
    {
        const int required = static_cast<int>(text.size());
        if (buffer == nullptr || bufferSize <= 0)
        {
            return required;
        }

        const int copyLen = (required < bufferSize) ? required : bufferSize - 1;
        wmemcpy(buffer, text.c_str(), copyLen);
        buffer[copyLen] = L'\0';
        return required;
    }

    // OCR 본체 — MTA 스레드에서 실행된다는 전제 (블로킹 .get() 사용)
    std::wstring ExtractTextCore(const std::wstring& imagePath, const std::wstring& langCode)
    {
        // 1) 파일을 IRandomAccessStream 으로 오픈 (StorageFile 브로커 경유보다 빠르고 제약 없음)
        IRandomAccessStream stream{ nullptr };
        const HRESULT hr = CreateRandomAccessStreamOnFile(
            imagePath.c_str(), STGM_READ, guid_of<IRandomAccessStream>(), put_abi(stream));
        if (FAILED(hr))
        {
            throw OcrError{ SC_OCR_ERR_GENERAL, L"이미지 파일을 열 수 없습니다: " + imagePath };
        }

        // 2) 비트맵 디코딩
        const auto decoder = BitmapDecoder::CreateAsync(stream).get();
        const SoftwareBitmap bitmap = decoder.GetSoftwareBitmapAsync().get();

        // 3) OCR 엔진 생성
        OcrEngine engine{ nullptr };
        if (langCode.empty())
        {
            engine = OcrEngine::TryCreateFromUserProfileLanguages();
        }
        else
        {
            const Language lang{ langCode };
            if (!OcrEngine::IsLanguageSupported(lang))
            {
                throw OcrError{ SC_OCR_ERR_LANG_NOT_SUPPORTED,
                    L"'" + langCode + L"' 언어 팩이 Windows 에 설치되어 있지 않거나 OCR 을 지원하지 않습니다." };
            }

            engine = OcrEngine::TryCreateFromLanguage(lang);
        }

        if (!engine)
        {
            throw OcrError{ SC_OCR_ERR_NO_ENGINE, L"OCR 엔진을 생성할 수 없습니다. 사용 가능한 OCR 언어가 없습니다." };
        }

        // 4) 인식 후 라인을 CRLF 로 결합
        const auto result = engine.RecognizeAsync(bitmap).get();
        std::wstring text;
        for (const auto& line : result.Lines())
        {
            text += line.Text();
            text += L"\r\n";
        }

        return text;
    }

    // MTA 를 프로세스 수명 동안 고정한다.
    // 호출마다 MTA 가 생성/파괴되면 마지막 COM 스레드 종료 시 아파트먼트 런다운이 일어나고,
    // 이때 C++/WinRT 팩토리 캐시 정리 콜백이 런다운 스레드에서 실행되며 크래시한다 (AV 0xC0000005).
    // CoIncrementMTAUsage 로 MTA 를 유지하면 런다운 자체가 발생하지 않는다 (쿠키는 의도적으로 해제하지 않음).
    void EnsureMtaPinned()
    {
        static std::once_flag pinned;
        std::call_once(pinned, []
        {
            CO_MTA_USAGE_COOKIE cookie{};
            CoIncrementMTAUsage(&cookie);
        });
    }

    // fn 을 전용 MTA 스레드에서 실행 — 호출 스레드의 COM 아파트먼트 상태와 무관하게 안전.
    // (STA 스레드에서 IAsyncOperation::get() 을 호출하면 교착/어설션이 발생하므로 격리)
    template <typename TFn>
    int RunOnMtaThread(TFn&& fn)
    {
        EnsureMtaPinned();

        int resultCode = SC_OCR_ERR_GENERAL;
        std::wstring resultError;

        std::thread worker([&]
        {
            try
            {
                init_apartment(apartment_type::multi_threaded);
                resultCode = fn();
            }
            catch (const OcrError& e)
            {
                resultCode = e.Code;
                resultError = e.Message;
            }
            catch (const hresult_error& e)
            {
                resultCode = SC_OCR_ERR_GENERAL;
                resultError = e.message().c_str();
            }
            catch (const std::exception&)
            {
                resultCode = SC_OCR_ERR_GENERAL;
                resultError = L"알 수 없는 내부 오류가 발생했습니다.";
            }
        });
        worker.join();

        // 오류 메시지는 호출 스레드의 TLS 에 보관 (SC_OcrGetLastError 로 조회)
        g_lastError = resultError;
        return resultCode;
    }
}

extern "C"
{
    // 이미지 파일에서 텍스트를 추출한다.
    //  AImagePath  : 이미지 파일 전체 경로 (필수)
    //  ALangCode   : BCP-47 언어 태그 (예: "ko-KR"). nullptr/빈 문자열이면 사용자 프로필 언어 사용
    //  ABuffer     : 결과를 받을 버퍼 (nullptr 이면 필요한 길이만 반환)
    //  ABufferSize : 버퍼 크기 (문자 수, 널 포함)
    // 반환: >=0 결과 텍스트의 문자 수(널 제외), <0 오류 코드
    int __stdcall SC_OcrExtractText(
        const wchar_t* AImagePath, const wchar_t* ALangCode, wchar_t* ABuffer, int ABufferSize)
    {
        if (AImagePath == nullptr || *AImagePath == L'\0')
        {
            g_lastError = L"이미지 경로가 비어 있습니다.";
            return SC_OCR_ERR_INVALID_ARG;
        }

        const std::wstring imagePath{ AImagePath };
        const std::wstring langCode{ (ALangCode != nullptr) ? ALangCode : L"" };
        std::wstring text;

        const int code = RunOnMtaThread([&]
        {
            text = ExtractTextCore(imagePath, langCode);
            return SC_OCR_OK;
        });

        if (code < 0)
        {
            return code;
        }

        return CopyToBuffer(text, ABuffer, ABufferSize);
    }

    // 지정한 언어가 OCR 가능한지 확인한다. 반환: 1 지원, 0 미지원, <0 오류
    int __stdcall SC_OcrIsLanguageSupported(const wchar_t* ALangCode)
    {
        if (ALangCode == nullptr || *ALangCode == L'\0')
        {
            g_lastError = L"언어 코드가 비어 있습니다.";
            return SC_OCR_ERR_INVALID_ARG;
        }

        const std::wstring langCode{ ALangCode };
        int supported = 0;

        const int code = RunOnMtaThread([&]
        {
            const Language lang{ langCode };
            supported = OcrEngine::IsLanguageSupported(lang) ? 1 : 0;
            return SC_OCR_OK;
        });

        if (code < 0)
        {
            return code;
        }

        return supported;
    }

    // 설치된 OCR 가능 언어 태그 목록을 CRLF 로 구분해 반환한다. 2단계 호출 패턴 동일
    int __stdcall SC_OcrGetAvailableLanguages(wchar_t* ABuffer, int ABufferSize)
    {
        std::wstring tags;

        const int code = RunOnMtaThread([&]
        {
            for (const auto& lang : OcrEngine::AvailableRecognizerLanguages())
            {
                tags += lang.LanguageTag();
                tags += L"\r\n";
            }

            return SC_OCR_OK;
        });

        if (code < 0)
        {
            return code;
        }

        return CopyToBuffer(tags, ABuffer, ABufferSize);
    }

    // 호출 스레드에서 마지막으로 발생한 오류 메시지를 반환한다. 2단계 호출 패턴 동일
    int __stdcall SC_OcrGetLastError(wchar_t* ABuffer, int ABufferSize)
    {
        return CopyToBuffer(g_lastError, ABuffer, ABufferSize);
    }
}
