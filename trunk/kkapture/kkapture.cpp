/* kkapture: intrusive demo video capturing.
 * Copyright (c) 2005-2006 Fabian "ryg/farbrausch" Giesen.
 *
 * This program is free software; you can redistribute and/or modify it under
 * the terms of the Artistic License, Version 2.0beta5, or (at your opinion)
 * any later version; all distributions of this program should contain this
 * license in a file named "LICENSE.txt".
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT UNLESS REQUIRED BY
 * LAW OR AGREED TO IN WRITING WILL ANY COPYRIGHT HOLDER OR CONTRIBUTOR
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
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

// some dialog helpers
static BOOL EnableDlgItem(HWND hWnd,int id,BOOL bEnable)
{
  HWND hCtrlWnd = GetDlgItem(hWnd,id);
  return EnableWindow(hCtrlWnd,bEnable);
}

static BOOL IsWow64()
{
  typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
  LPFN_ISWOW64PROCESS IsWow64Process = (LPFN_ISWOW64PROCESS) 
    GetProcAddress(GetModuleHandle(_T("kernel32")),_T("IsWow64Process"));

  if(IsWow64Process)
  {
    BOOL result;
    if(!IsWow64Process(GetCurrentProcess(),&result))
      result = FALSE;

    return result;
  }
  else
    return FALSE;
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
  Params.Encoder = (EncoderType) RegQueryDWord(hk,_T("VideoEncoder"),AVIEncoderDShow);
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

static INT_PTR CALLBACK MainDialogProc(HWND hWndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
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

      SendDlgItemMessage(hWndDlg,IDC_ENCODER,CB_ADDSTRING,0,(LPARAM) ".BMP/.WAV writer");
      SendDlgItemMessage(hWndDlg,IDC_ENCODER,CB_ADDSTRING,0,(LPARAM) ".AVI (VfW, segmented)");
      SendDlgItemMessage(hWndDlg,IDC_ENCODER,CB_ADDSTRING,0,(LPARAM) ".AVI (DirectShow, OpenDML)");
      SendDlgItemMessage(hWndDlg,IDC_ENCODER,CB_SETCURSEL,Params.Encoder - 1,0);

      EnableDlgItem(hWndDlg,IDC_VIDEOCODEC,Params.Encoder != BMPEncoder);
      EnableDlgItem(hWndDlg,IDC_VCPICK,Params.Encoder != BMPEncoder);

      if(IsWow64())
      {
        CheckDlgButton(hWndDlg,IDC_NEWINTERCEPT,BST_CHECKED);
        EnableDlgItem(hWndDlg,IDC_NEWINTERCEPT,FALSE);
      }

      HIC codec = ICOpen(ICTYPE_VIDEO,Params.VideoCodec,ICMODE_QUERY);
      SetVideoCodecInfo(hWndDlg,codec);
      ICClose(codec);

      // gui stuff not read from registry
      CheckDlgButton(hWndDlg,IDC_VCAPTURE,BST_CHECKED);
      CheckDlgButton(hWndDlg,IDC_ACAPTURE,BST_CHECKED);
      CheckDlgButton(hWndDlg,IDC_SLEEPLAST,BST_CHECKED);
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
        Params.Encoder = (EncoderType) (1 + SendDlgItemMessage(hWndDlg,IDC_ENCODER,CB_GETCURSEL,0,0));

        Params.CaptureVideo = IsDlgButtonChecked(hWndDlg,IDC_VCAPTURE) == BST_CHECKED;
        Params.CaptureAudio = IsDlgButtonChecked(hWndDlg,IDC_ACAPTURE) == BST_CHECKED;
        Params.SoundMaxSkip = IsDlgButtonChecked(hWndDlg,IDC_SKIPSILENCE) == BST_CHECKED ? 10 : 0;
        Params.MakeSleepsLastOneFrame = IsDlgButtonChecked(hWndDlg,IDC_SLEEPLAST) == BST_CHECKED;
        Params.SleepTimeout = 2500; // yeah, this should be configurable

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

    case IDC_ENCODER:
      if(HIWORD(wParam) == CBN_SELCHANGE)
      {
        int selection = SendDlgItemMessage(hWndDlg,IDC_ENCODER,CB_GETCURSEL,0,0);
        BOOL allowCodecSelect = selection != 0;

        EnableDlgItem(hWndDlg,IDC_VIDEOCODEC,allowCodecSelect);
        EnableDlgItem(hWndDlg,IDC_VCPICK,allowCodecSelect);
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

// ----

static DWORD GetEntryPoint(TCHAR *fileName)
{
  IMAGE_DOS_HEADER doshdr;
  IMAGE_NT_HEADERS32 nthdr;
  DWORD read;

  HANDLE hFile = CreateFile(fileName,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
  if(hFile == INVALID_HANDLE_VALUE)
    return 0;

  if(!ReadFile(hFile,&doshdr,sizeof(doshdr),&read,0) || read != sizeof(doshdr))
  {
    CloseHandle(hFile);
    return 0;
  }

  if(doshdr.e_magic != IMAGE_DOS_SIGNATURE)
  {
    CloseHandle(hFile);
    return 0;
  }

  if(SetFilePointer(hFile,doshdr.e_lfanew,0,FILE_BEGIN) == INVALID_SET_FILE_POINTER)
  {
    CloseHandle(hFile);
    return 0;
  }

  if(!ReadFile(hFile,&nthdr,sizeof(nthdr),&read,0) || read != sizeof(nthdr))
  {
    CloseHandle(hFile);
    return 0;
  }

  CloseHandle(hFile);

  if(nthdr.Signature != IMAGE_NT_SIGNATURE
    || nthdr.FileHeader.Machine != IMAGE_FILE_MACHINE_I386
    || nthdr.FileHeader.SizeOfOptionalHeader != IMAGE_SIZEOF_NT_OPTIONAL32_HEADER
    || nthdr.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC
    || !nthdr.OptionalHeader.AddressOfEntryPoint)
    return 0;

  return nthdr.OptionalHeader.ImageBase + nthdr.OptionalHeader.AddressOfEntryPoint;
}

static bool PrepareInstrumentation(HANDLE hProcess,BYTE *workArea,TCHAR *dllName,DWORD entryPointAddr)
{
  BYTE origCode[24];
  struct bufferType
  {
    BYTE code[2048];
    BYTE data[2048];
  } buffer;
  BYTE jumpCode[5];

  DWORD offsWorkArea = (DWORD) workArea;
  BYTE *code = buffer.code;
  BYTE *loadLibrary = (BYTE *) GetProcAddress(GetModuleHandle(_T("kernel32.dll")),"LoadLibraryA");
  BYTE *entryPoint = (BYTE *) entryPointAddr;

  // Read original startup code
  if(!ReadProcessMemory(hProcess,entryPoint,origCode,sizeof(origCode),0))
    return false;

  // Generate Initialization hook
  code = DetourGenPushad(code);
  _tcscpy((TCHAR *) buffer.data,dllName);
  code = DetourGenPush(code,offsWorkArea + offsetof(bufferType,data));
  code = DetourGenCall(code,loadLibrary,workArea + (code - buffer.code));
  code = DetourGenPopad(code);

  // Copy over first few bytes from startup code
  BYTE *sourcePtr = origCode;

  while((sourcePtr - origCode) < sizeof(jumpCode))
    sourcePtr = DetourCopyInstruction(code + (sourcePtr - origCode),sourcePtr,0);

  code += sourcePtr - origCode;

  // Jump to rest
  code = DetourGenJmp(code,entryPoint + (sourcePtr - origCode),workArea + (code - buffer.code));

  // And prepare jump to init hook from original entry point
  DetourGenJmp(jumpCode,workArea,entryPoint);

  // Finally, write everything into process memory
  if(!WriteProcessMemory(hProcess,workArea,&buffer,sizeof(buffer),0)
    || !WriteProcessMemory(hProcess,entryPoint,jumpCode,sizeof(jumpCode),0)
    || !FlushInstructionCache(hProcess,entryPoint,sizeof(jumpCode)))
    return false;

  return true;
}

// ----

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
    TCHAR mypath[_MAX_PATH],dllpath[_MAX_PATH],exepath[_MAX_PATH];
    TCHAR drive[_MAX_DRIVE],dir[_MAX_DIR],fname[_MAX_FNAME],ext[_MAX_EXT];
    GetModuleFileName(0,mypath,_MAX_PATH);
    _tsplitpath(mypath,drive,dir,fname,ext);
    _tmakepath(dllpath,drive,dir,"kkapturedll","dll");

    // create process
	  STARTUPINFO si;
	  PROCESS_INFORMATION pi;

    ZeroMemory(&si,sizeof(si));
    ZeroMemory(&pi,sizeof(pi));
    si.cb = sizeof(si);

    _tsplitpath(ExeName,drive,dir,fname,ext);
    _tmakepath(exepath,drive,dir,_T(""),_T(""));
    SetCurrentDirectory(exepath);

    /*if(DetourCreateProcessWithDll(ExeName,commandLine,0,0,TRUE,
      CREATE_DEFAULT_ERROR_MODE,0,0,&si,&pi,0,0,0))*/
    DWORD entryPoint = GetEntryPoint(ExeName);
    if(!entryPoint)
      MessageBox(0,_T("Not a supported executable format."),
        _T(".kkapture"),MB_ICONERROR|MB_OK);
    else if(CreateProcess(ExeName,commandLine,0,0,TRUE,
      CREATE_DEFAULT_ERROR_MODE|CREATE_SUSPENDED,0,0,&si,&pi))
    {
      // get some memory in the target processes' space for us to work with
      void *workMem = VirtualAllocEx(pi.hProcess,0,4096,MEM_COMMIT,
        PAGE_EXECUTE_READWRITE);

      // do all the mean initialization faking code here
      if(PrepareInstrumentation(pi.hProcess,(BYTE *) workMem,dllpath,entryPoint))
      {
        // we're done with our evil machinations, so rock on
        ResumeThread(pi.hThread);

        // wait for target process to finish
        WaitForSingleObject(pi.hProcess,INFINITE);
        CloseHandle(pi.hProcess);
      }
      else
        MessageBox(0,_T("Startup instrumentation failed"),
        _T(".kkapture"),MB_ICONERROR|MB_OK);
    }
    else
      MessageBox(0,_T("Couldn't execute target process"),
        _T(".kkapture"),MB_ICONERROR|MB_OK);

    // cleanup
    CloseHandle(hParamMapping);
  }
}