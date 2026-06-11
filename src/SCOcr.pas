// sc_ocr.dll (Windows.Media.Ocr 네이티브 래퍼) 의 Delphi 바인딩 유닛
unit SCOcr;

interface

{$REGION 'uses'}
uses
  System.SysUtils,
  Winapi.Windows;
{$ENDREGION}

type
  /// <summary>sc_ocr.dll 호출 중 발생하는 오류를 나타내는 예외 클래스입니다.</summary>
  ESCOcrError = class(Exception);

  /// <summary>
  /// Windows 내장 OCR(WinRT Windows.Media.Ocr) 을 sc_ocr.dll 경유로 사용하는 정적 래퍼 클래스입니다.
  /// DLL 은 실행 파일과 같은 폴더의 sc_ocr.dll 을 최초 사용 시점에 지연 로드합니다.
  /// 모든 메서드는 스레드 안전하며 워커 스레드에서 호출해도 됩니다 (COM 초기화 불필요 — DLL 내부에서 처리).
  /// </summary>
  TSCOcr = class
  strict private
    class var FExtractText: function(AImagePath, ALangCode, ABuffer: PWideChar; ABufferSize: Integer): Integer; stdcall;
    class var FGetAvailableLanguages: function(ABuffer: PWideChar; ABufferSize: Integer): Integer; stdcall;
    class var FGetLastError: function(ABuffer: PWideChar; ABufferSize: Integer): Integer; stdcall;
    class var FIsLanguageSupported: function(ALangCode: PWideChar): Integer; stdcall;
    class var FLibHandle: HMODULE;
    class destructor Destroy;
  private
    class function GetLastErrorText: string;
    class procedure EnsureLoaded;
    class procedure RaiseLastError(ACode: Integer);
  public
    /// <summary>이미지 파일에서 텍스트를 추출합니다. Windows 내장 OCR 엔진을 사용하며 호출 스레드를 블로킹합니다.</summary>
    /// <param name="AImagePath">이미지 파일의 전체 경로 (PNG/JPG/BMP 등 WIC 가 디코딩 가능한 포맷)</param>
    /// <param name="ALangCode">BCP-47 언어 태그 (예: 'ko-KR', 'en-US'). 빈 문자열이면 사용자 프로필 언어 사용</param>
    /// <returns>인식된 텍스트 (라인은 CRLF 로 구분). 인식된 내용이 없으면 빈 문자열</returns>
    /// <exception cref="ESCOcrError">파일 열기 실패, 언어 팩 미설치, 엔진 생성 실패 시 발생</exception>
    class function ExtractText(const AImagePath: string; const ALangCode: string = ''): string;

    /// <summary>OS 에 설치되어 OCR 이 가능한 언어 태그 목록을 반환합니다.</summary>
    /// <returns>BCP-47 언어 태그 배열 (예: ['ko', 'en-US'])</returns>
    /// <exception cref="ESCOcrError">DLL 로드 실패 또는 내부 오류 시 발생</exception>
    class function GetAvailableLanguages: TArray<string>;

    /// <summary>지정한 언어가 이 시스템에서 OCR 가능한지 확인합니다.</summary>
    /// <param name="ALangCode">BCP-47 언어 태그 (예: 'ko-KR')</param>
    /// <returns>OCR 가능하면 True</returns>
    /// <exception cref="ESCOcrError">DLL 로드 실패 또는 내부 오류 시 발생</exception>
    class function IsLanguageSupported(const ALangCode: string): Boolean;
  end;

implementation

const
  // 플랫폼별 DLL 선택: Win64 는 sc_ocr.dll, Win32 는 sc_ocr32.dll
  SC_OCR_DLL = {$IFDEF WIN64}'sc_ocr.dll'{$ELSE}'sc_ocr32.dll'{$ENDIF};

{$REGION 'TSCOcr'}

class destructor TSCOcr.Destroy;
begin
  if FLibHandle <> 0 then
  begin
    FreeLibrary(FLibHandle);
    FLibHandle := 0;
  end;
end;

class procedure TSCOcr.EnsureLoaded;

  function Bind(const AName: string): Pointer;
  begin
    Result := GetProcAddress(FLibHandle, PChar(AName));
    if not Assigned(Result) then
    begin
      raise ESCOcrError.CreateFmt('%s 에서 %s 함수를 찾을 수 없습니다.', [SC_OCR_DLL, AName]);
    end;
  end;

begin
  if FLibHandle <> 0 then
  begin
    Exit;
  end;

  // 실행 파일과 같은 폴더에서 로드 (검색 경로 하이재킹 방지)
  var LPath := ExtractFilePath(ParamStr(0)) + SC_OCR_DLL;
  FLibHandle := LoadLibrary(PChar(LPath));
  if FLibHandle = 0 then
  begin
    raise ESCOcrError.CreateFmt('%s 을(를) 로드할 수 없습니다. 실행 파일과 같은 폴더에 있는지 확인하세요. (%s)',
      [SC_OCR_DLL, SysErrorMessage(Winapi.Windows.GetLastError)]);
  end;

  FExtractText := Bind('SC_OcrExtractText');
  FGetAvailableLanguages := Bind('SC_OcrGetAvailableLanguages');
  FGetLastError := Bind('SC_OcrGetLastError');
  FIsLanguageSupported := Bind('SC_OcrIsLanguageSupported');
end;

class function TSCOcr.ExtractText(const AImagePath, ALangCode: string): string;
begin
  EnsureLoaded;

  // 1차 호출: 필요한 버퍼 길이 조회
  var LLen := FExtractText(PWideChar(AImagePath), PWideChar(ALangCode), nil, 0);
  if LLen < 0 then
  begin
    RaiseLastError(LLen);
  end;

  if LLen = 0 then
  begin
    Exit('');
  end;

  // 2차 호출: 실제 텍스트 수신
  SetLength(Result, LLen);
  var LCopied := FExtractText(PWideChar(AImagePath), PWideChar(ALangCode), PWideChar(Result), LLen + 1);
  if LCopied < 0 then
  begin
    RaiseLastError(LCopied);
  end;

  // 두 호출 사이에 결과가 달라질 수 있으므로 실제 길이로 보정
  if LCopied < LLen then
  begin
    SetLength(Result, LCopied);
  end;
end;

class function TSCOcr.GetAvailableLanguages: TArray<string>;
begin
  EnsureLoaded;

  var LLen := FGetAvailableLanguages(nil, 0);
  if LLen < 0 then
  begin
    RaiseLastError(LLen);
  end;

  if LLen = 0 then
  begin
    Exit(nil);
  end;

  var LText: string;
  SetLength(LText, LLen);
  var LCopied := FGetAvailableLanguages(PWideChar(LText), LLen + 1);
  if LCopied < 0 then
  begin
    RaiseLastError(LCopied);
  end;

  if LCopied < LLen then
  begin
    SetLength(LText, LCopied);
  end;

  Result := LText.Split([#13#10], TStringSplitOptions.ExcludeEmpty);
end;

class function TSCOcr.GetLastErrorText: string;
begin
  var LLen := FGetLastError(nil, 0);
  if LLen <= 0 then
  begin
    Exit('');
  end;

  SetLength(Result, LLen);
  FGetLastError(PWideChar(Result), LLen + 1);
end;

class function TSCOcr.IsLanguageSupported(const ALangCode: string): Boolean;
begin
  EnsureLoaded;

  var LCode := FIsLanguageSupported(PWideChar(ALangCode));
  if LCode < 0 then
  begin
    RaiseLastError(LCode);
  end;

  Result := LCode = 1;
end;

class procedure TSCOcr.RaiseLastError(ACode: Integer);
begin
  var LMessage := GetLastErrorText;
  if LMessage = '' then
  begin
    LMessage := Format('OCR 처리 중 알 수 없는 오류가 발생했습니다. (코드: %d)', [ACode]);
  end;

  raise ESCOcrError.Create(LMessage);
end;

{$ENDREGION}

end.
