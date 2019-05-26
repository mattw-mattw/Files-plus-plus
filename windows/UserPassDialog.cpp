// UserPassDialog.cpp : implementation file
//

#include "stdafx.h"
#include "UserPassDialog.h"
#include "afxdialogex.h"
#include "resource.h"



// UserPassDialog dialog

IMPLEMENT_DYNAMIC(UserPassDialog, CDialogEx)

UserPassDialog::UserPassDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_USERPASSDIALOG, pParent)
	, name(_T(""))
	, password(_T(""))
{

}

UserPassDialog::~UserPassDialog()
{
}

void UserPassDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, name);
	DDX_Text(pDX, IDC_EDIT2, password);
}


BEGIN_MESSAGE_MAP(UserPassDialog, CDialogEx)
END_MESSAGE_MAP()


// UserPassDialog message handlers
