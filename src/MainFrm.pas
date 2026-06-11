// WinRT OCR 데모를 위한 메인 화면 유닛 (sc_ocr.dll 네이티브 래퍼 버전)
unit MainFrm;

interface

uses
  Winapi.Windows, Winapi.Messages, System.SysUtils, System.Variants, System.Classes,
  Vcl.Graphics, Vcl.Controls, Vcl.Forms, Vcl.Dialogs, Vcl.StdCtrls, Vcl.ExtCtrls,
  Vcl.Imaging.jpeg, Vcl.Imaging.pngimage;

type
  TMainForm = class(TForm)
    ImagePreview: TImage;
    btnOpenImage: TButton;
    btnPasteImage: TButton;
    MemoOutput: TMemo;
    OpenDialog: TOpenDialog;
    cbLanguage: TComboBox;
    procedure btnOpenImageClick(Sender: TObject);
    procedure btnPasteImageClick(Sender: TObject);
  private
    function  GetSelectedLangCode: string;
    procedure ProcessOCR(const AFileName, ALangCode: string);
    procedure StartOcr(const AFileName, ALangCode: string);
  public
  end;

var
  MainForm: TMainForm;

implementation

{$R *.dfm}

{$REGION 'uses'}
uses
  System.Threading,
  System.IOUtils,
  Vcl.Clipbrd,
  SCOcr;
{$ENDREGION}

procedure TMainForm.btnOpenImageClick(Sender: TObject);
begin
  if OpenDialog.Execute then
  begin
    ImagePreview.Picture.LoadFromFile(OpenDialog.FileName);
    StartOcr(OpenDialog.FileName, GetSelectedLangCode);
  end;
end;

procedure TMainForm.btnPasteImageClick(Sender: TObject);
begin
  if not Clipboard.HasFormat(CF_BITMAP) then
  begin
    MemoOutput.Text := '클립보드에 이미지가 없습니다.';
    Exit;
  end;

  // 클립보드 비트맵을 임시 PNG 로 저장해 파일 기반 OCR 경로를 재사용
  var LTempFile := TPath.Combine(TPath.GetTempPath, 'winrtocr_clipboard.png');
  var LBitmap := TBitmap.Create;
  try
    LBitmap.Assign(Clipboard);
    ImagePreview.Picture.Assign(LBitmap);

    var LPng := TPngImage.Create;
    try
      LPng.Assign(LBitmap);
      LPng.SaveToFile(LTempFile);
    finally
      LPng.Free;
    end;
  finally
    LBitmap.Free;
  end;

  StartOcr(LTempFile, GetSelectedLangCode);
end;

function TMainForm.GetSelectedLangCode: string;
begin
  case cbLanguage.ItemIndex of
    1: Result := 'ko-KR';
    2: Result := 'en-US';
    3: Result := 'ja-JP';
    4: Result := 'zh-Hans';
    5: Result := 'zh-Hant';
    else Result := ''; // 0 또는 예외 상황 시 시스템 기본값
  end;
end;

procedure TMainForm.ProcessOCR(const AFileName, ALangCode: string);
var
  LRecognizedText: string;
  LErrorText: string;
begin
  try
    // sc_ocr.dll 경유로 텍스트 추출 (동기 호출, COM 초기화는 DLL 내부에서 처리)
    LRecognizedText := TSCOcr.ExtractText(AFileName, ALangCode);

    // 결과 텍스트를 메인 스레드에서 UI에 반영
    TThread.Queue(nil,
      procedure
      begin
        MemoOutput.Text := LRecognizedText;
      end);
  except
    on E: Exception do
    begin
      LErrorText := E.Message;
      TThread.Queue(nil,
        procedure
        begin
          MemoOutput.Text := '오류 발생: ' + LErrorText;
        end);
    end;
  end;
end;

procedure TMainForm.StartOcr(const AFileName, ALangCode: string);
begin
  MemoOutput.Clear;
  MemoOutput.Lines.Add('OCR 처리 중입니다...');

  // 메인 UI가 멈추지 않도록 백그라운드 스레드에서 OCR 진행
  TTask.Run(
    procedure
    begin
      ProcessOCR(AFileName, ALangCode);
    end);
end;

end.
