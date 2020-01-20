// TextDialog.cpp : implementation file
//

#include "stdafx.h"
#include "TextDialog.h"
#include "afxdialogex.h"
#include "resource.h"


// TextDialog dialog

IMPLEMENT_DYNAMIC(TextDialog, CDialogEx)

TextDialog::TextDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_TEXTDIALOG, pParent)
    , textString(_T(""))
{

}

TextDialog::~TextDialog()
{
}

void TextDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_RICHEDIT21, textString);
}


BEGIN_MESSAGE_MAP(TextDialog, CDialogEx)
END_MESSAGE_MAP()


// TextDialog message handlers
