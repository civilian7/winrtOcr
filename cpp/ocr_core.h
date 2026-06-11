// ocr_core.h — Windows.Media.Ocr(WinRT) 호출 코어 (sc_ocr.dll 과 winocr.exe 가 공유)
// 전제: 호출 스레드가 MTA 로 초기화되어 있어야 함 (블로킹 .get() 사용)
#pragma once

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>
#include <shcore.h>
#include <string>

namespace ocr_core
{
    // 오류 코드
    constexpr int SC_OCR_OK = 0;
    constexpr int SC_OCR_ERR_GENERAL = -1;            // 일반 오류
    constexpr int SC_OCR_ERR_LANG_NOT_SUPPORTED = -2; // 언어 팩 미설치 / 미지원
    constexpr int SC_OCR_ERR_NO_ENGINE = -3;          // OCR 엔진 생성 실패
    constexpr int SC_OCR_ERR_INVALID_ARG = -4;        // 잘못된 인자

    // 코드와 메시지를 함께 던지는 내부 예외
    struct OcrError
    {
        int Code;
        std::wstring Message;
    };

    // 이미지 파일에서 텍스트 추출. 라인은 CRLF 로 결합되어 반환된다.
    inline std::wstring ExtractText(const std::wstring& imagePath, const std::wstring& langCode)
    {
        using namespace winrt;
        using namespace winrt::Windows::Globalization;
        using namespace winrt::Windows::Graphics::Imaging;
        using namespace winrt::Windows::Media::Ocr;
        using namespace winrt::Windows::Storage::Streams;

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

    // 설치된 OCR 가능 언어 태그 목록 (CRLF 구분)
    inline std::wstring GetAvailableLanguages()
    {
        std::wstring tags;
        for (const auto& lang : winrt::Windows::Media::Ocr::OcrEngine::AvailableRecognizerLanguages())
        {
            tags += lang.LanguageTag();
            tags += L"\r\n";
        }

        return tags;
    }

    // 지정한 언어의 OCR 지원 여부
    inline bool IsLanguageSupported(const std::wstring& langCode)
    {
        const winrt::Windows::Globalization::Language lang{ langCode };
        return winrt::Windows::Media::Ocr::OcrEngine::IsLanguageSupported(lang);
    }
}
