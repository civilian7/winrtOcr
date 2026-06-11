object MainForm: TMainForm
  Left = 0
  Top = 0
  Caption = 'WinRT OCR Demo'
  ClientHeight = 450
  ClientWidth = 600
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Segoe UI'
  Font.Style = []
  Position = poScreenCenter
  DesignSize = (
    600
    450)
  TextHeight = 15
  object ImagePreview: TImage
    Left = 10
    Top = 40
    Width = 280
    Height = 400
    Anchors = [akLeft, akTop, akBottom]
    Proportional = True
    Stretch = True
  end
  object btnOpenImage: TButton
    Left = 10
    Top = 8
    Width = 100
    Height = 25
    Caption = #51060#48120#51648' '#50676#44592
    TabOrder = 0
    OnClick = btnOpenImageClick
  end
  object btnPasteImage: TButton
    Left = 270
    Top = 8
    Width = 130
    Height = 25
    Caption = #53364#47549#48372#46300' '#48537#50668#45347#44592
    TabOrder = 3
    OnClick = btnPasteImageClick
  end
  object MemoOutput: TMemo
    Left = 300
    Top = 40
    Width = 290
    Height = 400
    Anchors = [akLeft, akTop, akRight, akBottom]
    ScrollBars = ssBoth
    TabOrder = 1
  end
  object cbLanguage: TComboBox
    Left = 120
    Top = 8
    Width = 140
    Height = 23
    Style = csDropDownList
    ItemIndex = 0
    TabOrder = 2
    Text = #51088#46041'('#49884#49828#53596' '#44592#48376')'
    Items.Strings = (
      #51088#46041'('#49884#49828#53596' '#44592#48376')'
      #54620#44397#50612'(ko-KR)'
      #50689#50612'(en-US)'
      #51068#48376#50612'(ja-JP)'
      #51473#44397#50612' '#44036#52404'(zh-Hans)'
      #51473#44397#50612' '#48264#52404'(zh-Hant)'#39')')
  end
  object OpenDialog: TOpenDialog
    Filter = 'Image Files|*.png;*.jpg;*.jpeg;*.bmp|All Files|*.*'
    Left = 152
    Top = 8
  end
end
