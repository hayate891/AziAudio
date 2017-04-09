#include "Configuration.h"
#include "common.h"
#include "DirectSoundDriver.h"
#include <stdio.h>
#include "resource.h"
#include "SoundDriverInterface.h"
#include "SoundDriverFactory.h"


extern HINSTANCE hInstance; // DLL's HINSTANCE
extern SoundDriverInterface *snd;

// ************* Member Variables *************

bool Configuration::configAIEmulation;
bool Configuration::configSyncAudio;
bool Configuration::configForceSync;
unsigned long Configuration::configVolume;
char Configuration::configAudioLogFolder[500];
LPGUID Configuration::configDevice;
SoundDriverType Configuration::configDriver;

// Host SampleRate / BitRate - Seems to help Frank #188
unsigned long Configuration::configFrequency;
unsigned long Configuration::configBitRate;

// Used for NewAudio only
unsigned long Configuration::configBufferLevel; 
unsigned long Configuration::configBufferFPS;
unsigned long Configuration::configBackendFPS;

// Prevent various sleep states from taking place
bool Configuration::configDisallowSleepXA2;
bool Configuration::configDisallowSleepDS8;

// Todo: Setting to identify which audio backend is being used

// ************* File-scope private Variables *************

// Todo: Remove -- these need to be reconsidered
//static int SelectedDSound;
// DirectSound selection
#ifdef _WIN32
static GUID EnumDeviceGUID[20];
static char EnumDeviceName[20][100];
static int EnumDeviceCount;
static SoundDriverType EnumDriverType[10];
static int EnumDriverCount;
#endif

const char *ConfigFile = "Config/AziCfg.bin";

// Dialog Procedures
#if defined(_WIN32)
INT_PTR CALLBACK ConfigProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

BOOL CALLBACK DSEnumProc(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName, LPVOID lpContext);
void Configuration::LoadSettings()
{
	size_t file_size;
	unsigned char azicfg[4];
	FILE *file;
	file = fopen(ConfigFile, "rb");
	if (file == NULL)
	{
		SaveSettings(); // Saves the config file with defaults
	}
	else
	{
		for (file_size = 0; file_size < sizeof(azicfg); file_size++) {
			const int character = fgetc(file);
			if (character < 0 || character > 255)
				break; /* hit EOF or a disk read error */
			azicfg[file_size] = (unsigned char)(character);
		}
		if (fclose(file) != 0)
			fputs("Failed to close config file stream.\n", stderr);
	}

	Configuration::configSyncAudio = (azicfg[0] != 0x00) ? true : false;
	Configuration::configForceSync = (azicfg[1] != 0x00) ? true : false;
	Configuration::configAIEmulation = (azicfg[2] != 0x00) ? true : false;
	Configuration::configVolume = (azicfg[3] > 100) ? 100 : azicfg[3];
}
void Configuration::SaveSettings()
{
	FILE *file;
	file = fopen(ConfigFile, "wb");
	if (file != NULL)
	{
		fprintf(file, "%c", Configuration::configSyncAudio);
		fprintf(file, "%c", Configuration::configForceSync);
		fprintf(file, "%c", Configuration::configAIEmulation);
		fprintf(file, "%c", Configuration::configVolume);
		fclose(file);
	}
}

/*
	Loads the default values expected for Configuration.  This will also load the settings from a file if the configuration file exists
*/
void Configuration::LoadDefaults()
{
	EnumDeviceCount = 0;
	EnumDriverCount = 0;
	safe_strcpy(Configuration::configAudioLogFolder, 499, "D:\\");
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS) && !defined(_XBOX)
	strcpy_s(Configuration::configAudioLogFolder, 500, "D:\\");
#else
	strcpy(Configuration::configAudioLogFolder, "D:\\");
#endif
	configDevice = NULL;
	// TODO: Query the system and get defaults (windows only?)
	Configuration::configSyncAudio = true;
	Configuration::configForceSync = false;
	Configuration::configAIEmulation = true;
	Configuration::configVolume = 0; /* 0:  max volume; 100:  min volume */
	configFrequency = 44100; // Not implemented -- needs testing
	configBitRate   = 16;    // Not implemented -- needs testing
	configBufferLevel = 2;  // NewAudio only - How many frames to buffer
	configBufferFPS = 90;   // NewAudio only - How much data to frame per second
	configBackendFPS = 90;  // NewAudio only - How much data to frame per second
	configDisallowSleepXA2 = false;
	configDisallowSleepDS8 = false;
	if ((DirectSoundEnumerate(DSEnumProc, NULL)) != DS_OK)
	{
		EnumDeviceCount = 1;
		strcpy(EnumDeviceName[0], "Default");
		configDevice = NULL;
		printf("Unable to enumerate DirectSound devices\n");
	}
	EnumDriverCount = SoundDriverFactory::EnumDrivers(EnumDriverType, 10);
	configDriver = SoundDriverFactory::DefaultDriver();	
	LoadSettings();
}
#ifdef _WIN32
#pragma comment(lib, "comctl32.lib")
extern HINSTANCE hInstance;
void Configuration::ConfigDialog(HWND hParent)
{
	//DialogBox(hInstance, MAKEINTRESOURCE(IDD_CONFIG), hParent, ConfigProc);
	//return;
	PROPSHEETHEADER psh;
	PROPSHEETPAGE psp[2];
	
	memset(psp, 0, sizeof(psp));
	psp[0].dwSize = sizeof(PROPSHEETPAGE);
	psp[0].dwFlags = PSP_USEICONID | PSP_USETITLE;
	psp[0].hInstance = hInstance;
	psp[0].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_GENERAL);
	//psp[0].pszIcon = MAKEINTRESOURCE(IDI_FONT);
	psp[0].pfnDlgProc = ConfigProc;// FontDialogProc;
	psp[0].pszTitle = "Settings";// MAKEINTRESOURCE(IDS_FONT)
	psp[0].lParam = 0;
	psp[0].pfnCallback = NULL;

	psp[1].dwSize = sizeof(PROPSHEETPAGE);
	psp[1].dwFlags = PSP_USEICONID | PSP_USETITLE;
	psp[1].hInstance = hInstance;
	psp[1].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_ADVANCED);
	//psp[0].pszIcon = MAKEINTRESOURCE(IDI_FONT);
	psp[1].pfnDlgProc = ConfigProc;// FontDialogProc;
	psp[1].pszTitle = "Advanced";// MAKEINTRESOURCE(IDS_FONT)
	psp[1].lParam = 0;
	psp[1].pfnCallback = NULL;

	memset(&psh, 0, sizeof(PROPSHEETHEADER));
	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_PROPSHEETPAGE;
	psh.hwndParent = hParent;
	psh.hInstance = hInstance;
	psh.pszCaption = "Audio Options";
	psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
	psh.nStartPage = 0;
	psh.ppsp = (LPCPROPSHEETPAGE)&psp;
	psh.pfnCallback = NULL;

	PropertySheet(&psh);

}
void Configuration::AboutDialog(HWND hParent)
{
#define ABOUTMESSAGE \
	PLUGIN_VERSION\
	"\nby Azimer\n"\
	"\nHome: https://www.apollo64.com/\n"\
	"Source: https://github.com/Azimer/AziAudio/\n"\
	"\n"\
	"MusyX code credited to Bobby Smiles and Mupen64Plus\n"

	MessageBoxA(hParent, ABOUTMESSAGE, "About", MB_OK|MB_ICONINFORMATION);
}
#endif

#if 1
// TODO: I think this can safely be removed
#ifdef _WIN32
BOOL CALLBACK DSEnumProc(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName, LPVOID lpContext)
{
	UNREFERENCED_PARAMETER(lpszDrvName);
	UNREFERENCED_PARAMETER(lpContext);
	if (lpGUID == NULL)
	{
		safe_strcpy(EnumDeviceName[EnumDeviceCount], 99, "Default");
		memset(&EnumDeviceGUID[EnumDeviceCount], 0, sizeof(GUID));
	}
	else
	{
		safe_strcpy(EnumDeviceName[EnumDeviceCount], 99, lpszDesc);
		memcpy(&EnumDeviceGUID[EnumDeviceCount], lpGUID, sizeof(GUID));
	}
	EnumDeviceCount++;
	return TRUE;
}
#endif
#endif

#if defined(_WIN32) && !defined(_XBOX)
INT_PTR CALLBACK Configuration::ConfigProc(
	HWND hDlg,  // handle to dialog box
	UINT uMsg,     // message
	WPARAM wParam, // first message parameter
	LPARAM lParam  // second message parameter
	) {
	UNREFERENCED_PARAMETER(lParam);
	int x;
	switch (uMsg) {
	case WM_INITDIALOG:
		SendMessage(GetDlgItem(hDlg, IDC_DEVICE), CB_RESETCONTENT, 0, 0);
		for (x = 0; x < EnumDeviceCount; x++) 
		{
			SendMessage(GetDlgItem(hDlg, IDC_DEVICE), CB_ADDSTRING, 0, (long)EnumDeviceName[x]);
			if (configDevice != NULL)
				if (memcmp(&EnumDeviceGUID, configDevice, sizeof(GUID)) == 0)
					SendMessage(GetDlgItem(hDlg, IDC_DEVICE), CB_SETCURSEL, x, 0);
		}
		if (configDevice == NULL)
			SendMessage(GetDlgItem(hDlg, IDC_DEVICE), CB_SETCURSEL, 0, 0);

		SendMessage(GetDlgItem(hDlg, IDC_BACKEND), CB_RESETCONTENT, 0, 0);
		for (x = 0; x < EnumDriverCount; x++) 
		{
			SendMessage(GetDlgItem(hDlg, IDC_BACKEND), CB_ADDSTRING, 0, (long)SoundDriverFactory::GetDriverDescription(EnumDriverType[x]));
			if (EnumDriverType[x] == configDriver)
				SendMessage(GetDlgItem(hDlg, IDC_BACKEND), CB_SETCURSEL, x, 0);
		}

		SendMessage(GetDlgItem(hDlg, IDC_OLDSYNC), BM_SETCHECK, Configuration::configForceSync ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessage(GetDlgItem(hDlg, IDC_AUDIOSYNC), BM_SETCHECK, Configuration::configSyncAudio ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessage(GetDlgItem(hDlg, IDC_AI), BM_SETCHECK, Configuration::configAIEmulation ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessage(GetDlgItem(hDlg, IDC_VOLUME), TBM_SETPOS, TRUE, Configuration::configVolume);
		SendMessage(GetDlgItem(hDlg, IDC_VOLUME), TBM_SETTICFREQ, 20, 0);
		SendMessage(GetDlgItem(hDlg, IDC_VOLUME), TBM_SETRANGEMIN, FALSE, 0);
		SendMessage(GetDlgItem(hDlg, IDC_VOLUME), TBM_SETRANGEMAX, FALSE, 100);
		if (Configuration::configVolume == 100)
		{
			SendMessage(GetDlgItem(hDlg, IDC_MUTE), BM_SETCHECK, BST_CHECKED, 0);
		}
		else
		{
			SendMessage(GetDlgItem(hDlg, IDC_MUTE), BM_SETCHECK, BST_UNCHECKED, 0);
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			configForceSync = SendMessage(GetDlgItem(hDlg, IDC_OLDSYNC), BM_GETSTATE, 0, 0) == BST_CHECKED ? true : false;
			configSyncAudio = SendMessage(GetDlgItem(hDlg, IDC_AUDIOSYNC), BM_GETSTATE, 0, 0) == BST_CHECKED ? true : false;
			configAIEmulation = SendMessage(GetDlgItem(hDlg, IDC_AI), BM_GETSTATE, 0, 0) == BST_CHECKED ? true : false;
			x = (int)SendMessage(GetDlgItem(hDlg, IDC_DEVICE), CB_GETCURSEL, 0, 0);  // TODO: need to save and switch devices
			if (x == 0)
				configDevice = NULL;
			else
				memcpy(&configDevice, &EnumDeviceGUID[x], sizeof(GUID));
			Configuration::configVolume = SendMessage(GetDlgItem(hDlg, IDC_VOLUME), TBM_GETPOS, 0, 0);
			snd->SetVolume(Configuration::configVolume);

			SaveSettings();

			EndDialog(hDlg, 0);
			break;
		case IDCANCEL:
			EndDialog(hDlg, 0);
			break;
		case IDC_MUTE:
			if (IsDlgButtonChecked(hDlg, IDC_MUTE))
			{
				SendMessage(GetDlgItem(hDlg, IDC_VOLUME), TBM_SETPOS, TRUE, 100);
				snd->SetVolume(100);
			}
			else {
				SendMessage(GetDlgItem(hDlg, IDC_VOLUME), TBM_SETPOS, TRUE, configVolume);
				snd->SetVolume(configVolume);
			}
			break;
		}
		break;
	case WM_KEYDOWN:
		break;
	case WM_VSCROLL:
		short int userReq = LOWORD(wParam);
		if (userReq == TB_ENDTRACK || userReq == TB_THUMBTRACK)
		{
			int dwPosition = SendMessage(GetDlgItem(hDlg, IDC_VOLUME), TBM_GETPOS, 0, 0);
			if (dwPosition == 100)
			{
				SendMessage(GetDlgItem(hDlg, IDC_MUTE), BM_SETCHECK, BST_CHECKED, 0);
			}
			else
			{
				SendMessage(GetDlgItem(hDlg, IDC_MUTE), BM_SETCHECK, BST_UNCHECKED, 0);
			}
			configVolume = dwPosition;
			snd->SetVolume(dwPosition);
		}
		break;
	}

	return FALSE;

}
#endif
