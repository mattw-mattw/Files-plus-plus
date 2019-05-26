#pragma once


// UserPassDialog dialog

class UserPassDialog : public CDialogEx
{
	DECLARE_DYNAMIC(UserPassDialog)

public:
	UserPassDialog(CWnd* pParent = nullptr);   // standard constructor
	virtual ~UserPassDialog();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_USERPASSDIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CString name;
	CString password;
};
