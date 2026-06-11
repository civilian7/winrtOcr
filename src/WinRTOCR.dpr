// WinRT OCR 데모 애플리케이션의 진입점 프로젝트 파일
program WinRTOCR;

uses
  Vcl.Forms,
  SCOcr in 'SCOcr.pas',
  MainFrm in 'MainFrm.pas' {MainForm};

{$R *.res}

begin
  Application.Initialize;
  Application.MainFormOnTaskbar := True;
  Application.CreateForm(TMainForm, MainForm);
  Application.Run;
end.
