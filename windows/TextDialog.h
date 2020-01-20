#pragma once


// TextDialog dialog

class TextDialog : public CDialogEx
{
	DECLARE_DYNAMIC(TextDialog)

public:
	TextDialog(CWnd* pParent = nullptr);   // standard constructor
	virtual ~TextDialog();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TEXTDIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString textString;
};
