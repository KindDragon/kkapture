// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include "../kkapturedll/main.h"
#include "resource.h"

#pragma comment(lib,"vfw32.lib")
#pragma comment(lib,"msacm32.lib")

static const TCHAR RegistryKeyName[] = _T("Software\\Farbrausch\\kkapture");
static const int MAX_ARGS = _MAX_PATH*2;

// global vars for parameter passing (yes this sucks...)
static TCHAR ExeName[_MAX_PATH];
static TCHAR Arguments[MAX_ARGS];
static ParameterBlock Params;

static int GetCheckedRadioButton(HWND hWnd,int ctrlFirst,int ctrlLast)
{
  for(int i=ctrlFirst;i<=ctrlLast;i++)
    if(IsDlgButtonChecked(hWnd,i))
      return i - ctrlFirst;

  return -1;
}

static BOOL EnableDlgItem(HWND hWnd,int id,BOOL bEnable)
{
  HWND hCtrlWnd = GetDlgItem(hWnd,id);
  return EnableWindow(hCtrlWnd,bEnable);
}

static void SetVideoCodecInfo(HWND hWndDlg,HIC codec)
{
  TCHAR buffer[256];

  if(codec)
  {
    ICINFO info;
    ZeroMemory(&info,sizeof(info));
    ICGetInfo(codec,&info,sizeof(info));

    _sntprintf(buffer,sizeof(buffer)/sizeof(*buffer),_T("Video codec: %S"),info.szDescription);
    buffer[255] = 0;
  }
  else
    _tcscpy(buffer,_T("Video codec: (uncompressed)"));

  SetDlgItemText(hWndDlg,IDC_VIDEOCODEC,buffer);
}

static DWORD RegQueryDWord(HKEY hk,LPCTSTR name,DWORD defValue)
{
  DWORD value,typeCode,size=sizeof(DWORD);

  if(!hk || !RegQueryValueEx(hk,name,0,&typeCode,(LPBYTE) &value,&size) == ERROR_SUCCESS
    || typeCode != REG_DWORD && size != sizeof(DWORD))
    value = defValue;

  return value;
}

static void RegSetDWord(HKEY hk,LPCTSTR name,DWORD value)
{
  RegSetValueEx(hk,name,0,REG_DWORD,(LPBYTE) &value,sizeof(DWORD));
}

static void LoadSettingsFromRegistry()
{
  HKEY hk = 0;

  if(RegOpenKeyEx(HKEY_CURRENT_USER,RegistryKeyName,0,KEY_READ,&hk) != ERROR_SUCCESS)
    hk = 0;

  Params.FrameRate = RegQueryDWord(hk,_T("FrameRate"),6000);
  Params.Encoder = (EncoderType) RegQueryDWord(hk,_T("VideoEncoder"),AVIEncoder);
  Params.VideoCodec = RegQueryDWord(hk,_T("AVIVideoCodec"),mmioFOURCC('D','I','B',' '));
  Params.VideoQuality = RegQueryDWord(hk,_T("AVIVideoQuality"),ICQUALITY_DEFAULT);

  if(hk)
    RegCloseKey(hk);
}

static void SaveSettingsToRegistry()
{
  HKEY hk;

  if(RegCreateKeyEx(HKEY_CURRENT_USER,RegistryKeyName,0,0,0,KEY_ALL_ACCESS,0,&hk,0) == ERROR_SUCCESS)
  {
    RegSetDWord(hk,_T("FrameRate"),Params.FrameRate);
    RegSetDWord(hk,_T("VideoEncoder"),Params.Encoder);
    RegSetDWord(hk,_T("AVIVideoCodec"),Params.VideoCodec);
    RegSetDWord(hk,_T("AVIVideoQuality"),Params.VideoQuality);
    RegCloseKey(hk);
  }
}

INT_PTR CALLBACK MainDialogProc(HWND hWndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
  switch(uMsg)
  {
  case WM_INITDIALOG:
    {
      // center this window
      RECT rcWork,rcDlg;
      SystemParametersInfo(SPI_GETWORKAREA,0,&rcWork,0);
      GetWindowRect(hWndDlg,&rcDlg);
      SetWindowPos(hWndDlg,0,(rcWork.left+rcWork.right-rcDlg.right+rcDlg.left)/2,
        (rcWork.top+rcWork.bottom-rcDlg.bottom+rcDlg.top)/2,-1,-1,SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

      // load settings from registry (or get default settings)
      LoadSettingsFromRegistry();

      // set gui values
      TCHAR buffer[32];
      _stprintf(buffer,"%d.%02d",Params.FrameRate/100,Params.FrameRate%100);
      SetDlgItemText(hWndDlg,IDC_FRAMERATE,buffer);

      CheckRadioButton(hWndDlg,IDC_ENCODER1,IDC_ENCODER2,IDC_ENCODER1+Params.Encoder-1);
      EnableDlgItem(hWndDlg,IDC_VIDEOCODEC,Params.Encoder == AVIEncoder);
      EnableDlgItem(hWndDlg,IDC_VCPICK,Params.Encoder == AVIEncoder);

      HIC codec = ICOpen(ICTYPE_VIDEO,Params.VideoCodec,ICMODE_QUERY);
      SetVideoCodecInfo(hWndDlg,codec);
      ICClose(codec);
    }
    return TRUE;

  case WM_COMMAND:
    switch(LOWORD(wParam))
    {
    case IDCANCEL:
      EndDialog(hWndDlg,0);
      return TRUE;

    case IDOK:
      {
        TCHAR frameRateStr[64];

        Params.VersionTag = PARAMVERSION;

        // get values
        GetDlgItemText(hWndDlg,IDC_DEMO,ExeName,_MAX_PATH);
        GetDlgItemText(hWndDlg,IDC_ARGUMENTS,Arguments,MAX_ARGS);
        GetDlgItemText(hWndDlg,IDC_TARGET,Params.FileName,_MAX_PATH);
        GetDlgItemText(hWndDlg,IDC_FRAMERATE,frameRateStr,sizeof(frameRateStr)/sizeof(*frameRateStr));

        // validate everything and fill out parameter block
        HANDLE hFile = CreateFile(ExeName,GENERIC_READ,0,0,OPEN_EXISTING,0,0);
        if(hFile == INVALID_HANDLE_VALUE)
        {
          MessageBox(hWndDlg,_T("You need to specify a valid executable in the 'demo' field."),
            _T(".kkapture"),MB_ICONERROR|MB_OK);
          return TRUE;
        }
        else
          CloseHandle(hFile);

        double frameRate = (float)atof(frameRateStr);
        if(frameRate <= 0.0f || frameRate >= 1000.0f)
        {
          MessageBox(hWndDlg,_T("Please enter a valid frame rate."),
            _T(".kkapture"),MB_ICONERROR|MB_OK);
          return TRUE;
        }

        Params.FrameRate = frameRate * 100;
        Params.Encoder = (EncoderType) (BMPEncoder + GetCheckedRadioButton(hWndDlg,IDC_ENCODER1,IDC_ENCODER2));

        // save settings for next time
        SaveSettingsToRegistry();

        // that's it.
        EndDialog(hWndDlg,1);
      }
      return TRUE;

    case IDC_BDEMO:
      {
        OPENFILENAME ofn;
        TCHAR filename[_MAX_PATH];

        GetDlgItemText(hWndDlg,IDC_DEMO,filename,_MAX_PATH);

        ZeroMemory(&ofn,sizeof(ofn));
        ofn.lStructSize   = sizeof(ofn);
        ofn.hwndOwner     = hWndDlg;
        ofn.hInstance     = GetModuleHandle(0);
        ofn.lpstrFilter   = _T("Executable files (*.exe)\0*.exe\0");
        ofn.nFilterIndex  = 1;
        ofn.lpstrFile     = filename;
        ofn.nMaxFile      = sizeof(filename)/sizeof(*filename);
        ofn.Flags         = OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
        if(GetOpenFileName(&ofn))
        {
          SetDlgItemText(hWndDlg,IDC_DEMO,ofn.lpstrFile);

          // also set demo .avi file name if not yet set
          TCHAR drive[_MAX_DRIVE],dir[_MAX_DIR],fname[_MAX_FNAME],ext[_MAX_EXT],path[_MAX_PATH];
          int nChars = GetDlgItemText(hWndDlg,IDC_TARGET,fname,_MAX_FNAME);
          if(!nChars)
          {
            _tsplitpath(ofn.lpstrFile,drive,dir,fname,ext);
            _tmakepath(path,drive,dir,fname,_T(".avi"));
            SetDlgItemText(hWndDlg,IDC_TARGET,path);
          }
        }
      }
      return TRUE;

    case IDC_BTARGET:
      {
        OPENFILENAME ofn;
        TCHAR filename[_MAX_PATH];

        GetDlgItemText(hWndDlg,IDC_TARGET,filename,_MAX_PATH);

        ZeroMemory(&ofn,sizeof(ofn));
        ofn.lStructSize   = sizeof(ofn);
        ofn.hwndOwner     = hWndDlg;
        ofn.hInstance     = GetModuleHandle(0);
        ofn.lpstrFilter   = _T("AVI files (*.avi)\0*.avi\0");
        ofn.nFilterIndex  = 1;
        ofn.lpstrFile     = filename;
        ofn.nMaxFile      = sizeof(filename)/sizeof(*filename);
        ofn.Flags         = OFN_HIDEREADONLY|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
        if(GetSaveFileName(&ofn))
          SetDlgItemText(hWndDlg,IDC_TARGET,ofn.lpstrFile);
      }
      return TRUE;

    case IDC_ENCODER1:
    case IDC_ENCODER2:
      {
        BOOL useAviEncoder = LOWORD(wParam) == IDC_ENCODER2;

        EnableDlgItem(hWndDlg,IDC_VIDEOCODEC,useAviEncoder);
        EnableDlgItem(hWndDlg,IDC_VCPICK,useAviEncoder);
      }
      return TRUE;

    case IDC_VCPICK:
      {
        COMPVARS cv;
        ZeroMemory(&cv,sizeof(cv));

        cv.cbSize = sizeof(cv);
        cv.dwFlags = ICMF_COMPVARS_VALID;
        cv.fccType = ICTYPE_VIDEO;
        cv.fccHandler = Params.VideoCodec;
        cv.lQ = Params.VideoQuality;
        if(ICCompressorChoose(hWndDlg,0,0,0,&cv,0))
        {
          Params.VideoCodec = cv.fccHandler;
          Params.VideoQuality = cv.lQ;
          SetVideoCodecInfo(hWndDlg,cv.hic);
          ICCompressorFree(&cv);
        }
      }
      return TRUE;
    }
    break;
  }

  return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{
  if(DialogBox(hInstance,MAKEINTRESOURCE(IDD_MAINWIN),0,MainDialogProc))
  {
    // create file mapping object with parameter block
    HANDLE hParamMapping = CreateFileMapping(INVALID_HANDLE_VALUE,0,PAGE_READWRITE,
      0,sizeof(ParameterBlock),_T("__kkapture_parameter_block"));

    // copy parameters
    LPVOID ParamBlock = MapViewOfFile(hParamMapping,FILE_MAP_WRITE,0,0,sizeof(ParameterBlock));
    if(ParamBlock)
    {
      memcpy(ParamBlock,&Params,sizeof(ParameterBlock));
      UnmapViewOfFile(ParamBlock);
    }

    // prepare command line
    TCHAR commandLine[_MAX_PATH+MAX_ARGS+4];
    _tcscpy(commandLine,_T("\""));
    _tcscat(commandLine,ExeName);
    _tcscat(commandLine,_T("\" "));
    _tcscat(commandLine,Arguments);

    // determine kkapture dll path
    TCHAR mypath[_MAX_PATH],dllpath[_MAX_PATH],drive[_MAX_DRIVE],dir[_MAX_DIR];
    TCHAR fname[_MAX_FNAME],ext[_MAX_EXT];
    GetModuleFileName(0,mypath,_MAX_PATH);
    _tsplitpath(mypath,drive,dir,fname,ext);
    _tmakepath(dllpath,drive,dir,"kkapturedll","dll");

    // create process
	  STARTUPINFO si;
	  PROCESS_INFORMATION pi;

    ZeroMemory(&si,sizeof(si));
    ZeroMemory(&pi,sizeof(pi));
    si.cb = sizeof(si);

    if(DetourCreateProcessWithDll(ExeName,commandLine,0,0,TRUE,
      CREATE_DEFAULT_ERROR_MODE,0,0,&si,&pi,dllpath,0))
    {
      // wait for target process to finish
      WaitForSingleObject(pi.hProcess,INFINITE);
      CloseHandle(pi.hProcess);
    }
    else
      MessageBox(0,_T("Couldn't execute target process"),
        _T(".kkapture"),MB_ICONERROR|MB_OK);

    // cleanup
    CloseHandle(hParamMapping);
  }
}