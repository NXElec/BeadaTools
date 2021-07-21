//#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#include <windows.h>
#include "pch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Define for pack type, per StatusLink protocol. */
#define TYPE_GET_PANEL_INFO 1
#define TYPE_PANELLINK_RESET 2
#define TYPE_SET_BACKLIGHT 3
#define TYPE_PUSH_STORAGE 4
#define TYPE_GET_TIME 5
#define TYPE_SET_TIME 6

const char protocol_str[] = "STATUS-LINK";

#pragma pack(push) //保存对齐状态
#pragma pack(1)   // 1 bytes对齐

typedef struct _STATUSLINK_TAG {
	unsigned char protocol_name[11];
	unsigned char version;
	unsigned char type;
	unsigned char reserved1;
	unsigned short sequence_number;
	unsigned short length;
	unsigned short checksum16;
} STATUSLINK_TAG;

typedef struct _STATUSLINK_INFO {
	unsigned short firmware_version;
	unsigned char panellink_version;
	unsigned char statuslink_version;
	unsigned char hardware_platform;
	unsigned char os_version;
	unsigned char sn[64];
	unsigned short screen_resolution_x;
	unsigned short screen_resolution_y;
	unsigned int storage_size;
	unsigned char max_brightness;
	unsigned char current_brightness;
}  STATUSLINK_INFO;

typedef struct _STATUSLINK_INFO_PACK {
	STATUSLINK_TAG header;
	STATUSLINK_INFO value;
}  STATUSLINK_INFO_PACK;

typedef struct _STATUSLINK_TEMP_PACK {
	STATUSLINK_TAG header;
	unsigned char value[256];
}  STATUSLINK_TEMP_PACK;

#pragma pack(pop)//恢复对齐状态

#define MIN_Buffer_Size 512

BOOL GetUSBDeviceSpeed(WINUSB_INTERFACE_HANDLE hDeviceHandle, UCHAR* pDeviceSpeed)
{
	if (!pDeviceSpeed || hDeviceHandle == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	BOOL bResult = TRUE;

	ULONG length = sizeof(UCHAR);

	bResult = WinUsb_QueryDeviceInformation(hDeviceHandle, DEVICE_SPEED, &length, pDeviceSpeed);
	if (!bResult)
	{
		printf("Error getting device speed: %d.\n", GetLastError());
		goto done;
	}

	if (*pDeviceSpeed == LowSpeed)
	{
		printf("Device speed: %d (Low speed).\n", *pDeviceSpeed);
		goto done;
	}
	if (*pDeviceSpeed == FullSpeed)
	{
		printf("Device speed: %d (Full speed).\n", *pDeviceSpeed);
		goto done;
	}
	if (*pDeviceSpeed == HighSpeed)
	{
		printf("Device speed: %d (High speed).\n", *pDeviceSpeed);
		goto done;
	}

done:
	return bResult;
}

struct PIPE_ID
{
	UCHAR  PipeInId;
	UCHAR  PipeOutId;
};

BOOL QueryDeviceEndpoints(WINUSB_INTERFACE_HANDLE hDeviceHandle, PIPE_ID* pipeid)
{
	if (hDeviceHandle == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	BOOL bResult = TRUE;

	USB_INTERFACE_DESCRIPTOR InterfaceDescriptor;
	ZeroMemory(&InterfaceDescriptor, sizeof(USB_INTERFACE_DESCRIPTOR));

	WINUSB_PIPE_INFORMATION  Pipe;
	ZeroMemory(&Pipe, sizeof(WINUSB_PIPE_INFORMATION));


	bResult = WinUsb_QueryInterfaceSettings(hDeviceHandle, 0, &InterfaceDescriptor);

	if (bResult)
	{
		for (int index = 0; index < InterfaceDescriptor.bNumEndpoints; index++)
		{
			bResult = WinUsb_QueryPipe(hDeviceHandle, 0, index, &Pipe);

			if (bResult)
			{
				if (Pipe.PipeType == UsbdPipeTypeControl)
				{
					printf("Endpoint index: %d Pipe type: %d Control Pipe ID: %d.\n", index, Pipe.PipeType, Pipe.PipeId);
				}
				if (Pipe.PipeType == UsbdPipeTypeIsochronous)
				{
					printf("Endpoint index: %d Pipe type: %d Isochronous Pipe ID: %d.\n", index, Pipe.PipeType, Pipe.PipeId);
				}
				if (Pipe.PipeType == UsbdPipeTypeBulk)
				{
					if (USB_ENDPOINT_DIRECTION_IN(Pipe.PipeId))
					{
						printf("Endpoint index: %d Pipe type: %d Bulk In Pipe ID: %x.\n", index, Pipe.PipeType, Pipe.PipeId);
						pipeid->PipeInId = Pipe.PipeId;
					}
					if (USB_ENDPOINT_DIRECTION_OUT(Pipe.PipeId))
					{
						printf("Endpoint index: %d Pipe type: %d Bulk Out Pipe ID: %x.\n", index, Pipe.PipeType, Pipe.PipeId);
						pipeid->PipeOutId = Pipe.PipeId;
					}

				}
				if (Pipe.PipeType == UsbdPipeTypeInterrupt)
				{
					printf("Endpoint index: %d Pipe type: %d Interrupt Pipe ID: %d.\n", index, Pipe.PipeType, Pipe.PipeId);
				}
			}
			else
			{
				continue;
			}
		}
	}

done:
	return bResult;
}

BOOL WriteToBulkEndpoint(WINUSB_INTERFACE_HANDLE hDeviceHandle, UCHAR* pID, ULONG* pcbWritten)
{
	if (hDeviceHandle == INVALID_HANDLE_VALUE || !pID || !pcbWritten)
	{
		return FALSE;
	}

	BOOL bResult = TRUE;

	UCHAR szBuffer[] = "Hello World";
	ULONG cbSize = strlen((const char*)szBuffer);
	ULONG cbSent = 0;

	bResult = WinUsb_WritePipe(hDeviceHandle, *pID, szBuffer, cbSize, &cbSent, 0);
	if (!bResult)
	{
		goto done;
	}

	printf("Wrote to pipe %d: %s \nActual data transferred: %d.\n", *pID, szBuffer, cbSent);
	*pcbWritten = cbSent;


done:
	return bResult;

}

BOOL ReadFromBulkEndpoint(WINUSB_INTERFACE_HANDLE hDeviceHandle, UCHAR* pID, ULONG cbSize)
{
	if (hDeviceHandle == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	BOOL bResult = TRUE;

	UCHAR* szBuffer = (UCHAR*)LocalAlloc(LPTR, sizeof(UCHAR)*cbSize);

	ULONG cbRead = 0;

	bResult = WinUsb_ReadPipe(hDeviceHandle, *pID, szBuffer, cbSize, &cbRead, 0);
	if (!bResult)
	{
		goto done;
	}

	printf("Read from pipe %d: %s \nActual data read: %d.\n", *pID, szBuffer, cbRead);


done:
	LocalFree(szBuffer);
	return bResult;

}

unsigned short checksum16(unsigned short *buf, int nword)
{
	unsigned long sum;

	for (sum = 0; nword > 0; nword--)
		sum += *buf++;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return ~sum;
}

#if 0
LONG __cdecl
_tmain(
	LONG     Argc,
	LPTSTR * Argv
)
#else
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
#endif

/*++

Routine description:

	Sample program that communicates with a USB device using WinUSB

--*/
{
	DEVICE_DATA           deviceData;
	HRESULT               hr;
	USB_DEVICE_DESCRIPTOR deviceDesc;
	BOOL                  bResult;
	BOOL                  noDevice;
	ULONG                 lengthReceived;
	PIPE_ID               pipeID;
	UCHAR                 speed;
	STATUSLINK_TAG *pTemp;
	STATUSLINK_INFO_PACK *pTemp1;
	int loop = 25;
	UCHAR Fmt[256];
	int isize;
	int iResult;
	char temps[256];
	wchar_t buffer[256];
	STATUSLINK_TEMP_PACK tempp;
	DWORD cbSent = 0;
	LONG     Argc;
	LPTSTR * Argv[3];
	time_t ltime;
	SYSTEMTIME tz;

		Argc = 1;
	while (1) {
		//
		// Find a device connected to the system that has WinUSB installed using our
		// INF
		//
		hr = OpenDevice(&deviceData, &noDevice);

		if (FAILED(hr)) {

			if (noDevice) {

				wsprintf(buffer, L"No BeadaPanel Device found!");
			}
			else {
				if (hr==0x80070005)
					wsprintf(buffer, L"Device already connected by AIDA64!\n");
				else
					wsprintf(buffer, L"Failed looking for device, HRESULT 0x%x\n", hr);
			}

			iResult = MessageBox(NULL, buffer, L"BeadaTools V1.2", MB_RETRYCANCEL | MB_ICONEXCLAMATION);
			if (iResult == IDRETRY) {
				continue;
			}

			return 0;
		}

		//
		// Get device descriptor
		//
		bResult = WinUsb_GetDescriptor(deviceData.WinusbHandle,
			USB_DEVICE_DESCRIPTOR_TYPE,
			0,
			0,
			(PBYTE)&deviceDesc,
			sizeof(deviceDesc),
			&lengthReceived);

		if (FALSE == bResult || lengthReceived != sizeof(deviceDesc)) {
			CloseDevice(&deviceData);

			wsprintf(buffer, L"Error among LastError %d or lengthReceived %d\n",
				FALSE == bResult ? GetLastError() : 0,
				lengthReceived);

			iResult = MessageBox(NULL, buffer, L"BeadaTools V1.2", MB_RETRYCANCEL | MB_ICONERROR);

			if (iResult == IDRETRY) {
				continue;
			}

			return 0;
		}

		//
		// Print a few parts of the device descriptor
		//
		wprintf(L"Device found: VID_%04X&PID_%04X; bcdDevice %04X\n",
			deviceDesc.idVendor,
			deviceDesc.idProduct,
			deviceDesc.bcdDevice);

		GetUSBDeviceSpeed(deviceData.WinusbHandle, &speed);
		QueryDeviceEndpoints(deviceData.WinusbHandle, &pipeID);

		if (deviceDesc.bcdDevice == 0x409) {
			CloseDevice(&deviceData);

			wsprintf(buffer, L"Device Information\r\n[Firmware Ver.         4.09]\r\n[Screen Resolution: 800x480]");
			iResult = MessageBox(NULL, buffer, L"BeadaTools V1.2", MB_RETRYCANCEL | MB_ICONINFORMATION);

			if (iResult == IDRETRY) {
				continue;
			}

			return 0;
		}
#if 0
		else if (Argc == 2) {
			isize = WideCharToMultiByte(CP_ACP, 0, Argv[1], -1, NULL, 0, NULL, NULL);
			if (isize <= 256) {
				WideCharToMultiByte(CP_ACP, 0, Argv[1], -1, (LPSTR)Fmt, isize, NULL, NULL);

				//	pFmt = (UCHAR *)Argv[2];
				printf("format string %s %d\n",
					Fmt, strlen((CONST char*)Fmt));
			}

			wsprintf(buffer, L"Write backlight value(%s) to device?", Argv[1]);

			if (MessageBox(NULL, buffer, L"BeadaTools V1.0", MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {

				pTemp = (STATUSLINK_TAG *)&tempp;
				pTemp->type = TYPE_SET_BACKLIGHT;
				pTemp->version = 1;
				memcpy(pTemp->protocol_name, protocol_str, strlen(protocol_str));
				pTemp->length = sizeof(STATUSLINK_TAG) + 1;
				pTemp->checksum16 = checksum16((unsigned short *)&tempp, (sizeof(STATUSLINK_TAG) - 2) / 2);
				tempp.value[0] = atoi((const char *)Fmt);

				bResult = WinUsb_WritePipe(deviceData.WinusbHandle, pipeID.PipeOutId, (UCHAR *)&tempp, sizeof(STATUSLINK_TAG)+1, &cbSent, 0);
				if (!bResult)
				{
					wprintf(L"WinUsb_WritePipe failure - header.\n");
					iResult = MessageBox(NULL, L"WinUsb_WritePipe failure - Stage 1", L"BeadaTools V1.0", MB_RETRYCANCEL | MB_ICONERROR);

					if (iResult == IDRETRY)
					{
						CloseDevice(&deviceData);
						continue;
					}
				}
				else {
					wprintf(L"WinUsb_WritePipe success - %d bytes header.\n", cbSent);
				}
			}

			CloseDevice(&deviceData);
			return 0;
		}
#endif
		else if (Argc == 3) {
#if 0
			isize = WideCharToMultiByte(CP_ACP, 0, Argv[2], -1, NULL, 0, NULL, NULL);
			if (isize <= 256) {
				WideCharToMultiByte(CP_ACP, 0, Argv[2], -1, (LPSTR)Fmt, isize, NULL, NULL);

				//	pFmt = (UCHAR *)Argv[2];
				printf("format string %s %d\n",
					Fmt, strlen((CONST char*)Fmt));
			}



			//		loop = _wtoi(Argv[2]);

			//	while (1) {
			HANDLE hFile = CreateFile(Argv[1],
				GENERIC_READ,
				0,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				0
			);

			if (hFile == INVALID_HANDLE_VALUE)
			{
				wprintf(L"File open failure. %s\n", Argv[1]);
				CloseDevice(&deviceData);
				return 0;
			}



			// Todo:
			// Should we implement a file mapping here to improve I/O performance?? 
			//
			DWORD dwFileSize = GetFileSize(hFile, NULL);
			UCHAR* szBuffer = (UCHAR*)LocalAlloc(LPTR, MIN_Buffer_Size*loop);

			{
				SetFilePointer(hFile,
					0,
					NULL,
					FILE_BEGIN);

				pTemp = (PANELLINK_STREAM_TAG*)szBuffer;
				pTemp->type = TYPE_START;
				pTemp->version = 1;
				memcpy(pTemp->protocol_name, protocol_str, strlen(protocol_str));
				memset(pTemp->fmtstr, 0, 256);
				memcpy(pTemp->fmtstr, Fmt, strlen((CONST char*)Fmt));
				pTemp->checksum16 = checksum16((unsigned short *)szBuffer, (sizeof(PANELLINK_STREAM_TAG) - 2) / 2);
				DWORD cbSent = 0;

				bResult = WinUsb_WritePipe(deviceData.WinusbHandle, pipeID.PipeOutId, (UCHAR *)szBuffer, sizeof(PANELLINK_STREAM_TAG), &cbSent, 0);
				if (!bResult)
				{
					wprintf(L"WinUsb_WritePipe failure - header.\n");
					LocalFree(szBuffer);
					CloseHandle(hFile);
					CloseDevice(&deviceData);
					return 0;
				}
				else {
					wprintf(L"WinUsb_WritePipe success - %d bytes header.\n", cbSent);
				}

				DWORD sections = dwFileSize / (MIN_Buffer_Size * loop);
				DWORD reminders = dwFileSize % (MIN_Buffer_Size * loop);
				if (reminders)
					sections += 1;

				unsigned long lpNumber = 0;
				for (int i = 0; i < sections; i++) {
					ReadFile(hFile,
						szBuffer,
						MIN_Buffer_Size * loop,//读取文件中多少内容
						&lpNumber,
						NULL
					);

					bResult = WinUsb_WritePipe(deviceData.WinusbHandle, pipeID.PipeOutId, szBuffer, lpNumber, &cbSent, 0);
					if (!bResult)
					{
						wprintf(L"WinUsb_WritePipe failure.\n");
						LocalFree(szBuffer);
						CloseHandle(hFile);
						CloseDevice(&deviceData);
						return 0;
					}


					//	printf("Wrote to pipe %d: \nActual data transferred: %d.\n", pipeID.PipeOutId, cbSent);
				}

				pTemp->type = TYPE_END;
				pTemp->version = 1;
				memcpy(pTemp->protocol_name, protocol_str, strlen(protocol_str));
				memset(pTemp->fmtstr, 0, 256);
				memcpy(pTemp->fmtstr, Fmt, strlen((CONST char *)Fmt));
				pTemp->checksum16 = checksum16((unsigned short *)szBuffer, (sizeof(PANELLINK_STREAM_TAG) - 2) / 2);
				cbSent = 0;

				bResult = WinUsb_WritePipe(deviceData.WinusbHandle, pipeID.PipeOutId, (UCHAR *)szBuffer, sizeof(PANELLINK_STREAM_TAG), &cbSent, 0);
				if (!bResult)
				{
					wprintf(L"WinUsb_WritePipe failure - header.\n");
					LocalFree(szBuffer);
					CloseHandle(hFile);
					CloseDevice(&deviceData);
					return 0;
				}
				else {
					wprintf(L"WinUsb_WritePipe success - %d bytes header.\n", cbSent);
				}

				wprintf(L"File content dump done.\n");

				pTemp->type = TYPE_RESET;
				pTemp->version = 1;
				memcpy(pTemp->protocol_name, protocol_str, strlen(protocol_str));
				memset(pTemp->fmtstr, 0, 256);
				pTemp->checksum16 = checksum16((unsigned short *)szBuffer, (sizeof(PANELLINK_STREAM_TAG) - 2) / 2);
				cbSent = 0;

				bResult = WinUsb_WritePipe(deviceData.WinusbHandle, pipeID.PipeOutId, (UCHAR *)szBuffer, sizeof(PANELLINK_STREAM_TAG), &cbSent, 0);
				if (!bResult)
				{
					wprintf(L"WinUsb_WritePipe failure - reset.\n");
					LocalFree(szBuffer);
					CloseHandle(hFile);
					CloseDevice(&deviceData);
					return 0;
				}
				else {
					wprintf(L"WinUsb_WritePipe success - %d bytes reset.\n", cbSent);
				}
			}

			wprintf(L"File content dump done.\n");

			LocalFree(szBuffer);
			CloseHandle(hFile);
			//	Sleep(5000);
#endif
		}
		else
		{

		// GET_PANEL_INFO command send out here
		pTemp = (STATUSLINK_TAG *)&tempp;
		pTemp->type = TYPE_GET_PANEL_INFO;
		pTemp->version = 1;
		memcpy(pTemp->protocol_name, protocol_str, strlen(protocol_str));
		pTemp->length = sizeof(STATUSLINK_TAG);
		pTemp->checksum16 = checksum16((unsigned short *)&tempp, (sizeof(STATUSLINK_TAG) - 2) / 2);

		bResult = WinUsb_WritePipe(deviceData.WinusbHandle, pipeID.PipeOutId, (UCHAR *)&tempp, sizeof(STATUSLINK_TAG), &cbSent, 0);
		if (!bResult)
		{
			wprintf(L"WinUsb_WritePipe failure - header.\n");
			iResult = MessageBox(NULL, L"WinUsb_WritePipe failure - Stage 2", L"BeadaTools V1.2", MB_RETRYCANCEL | MB_ICONERROR);

			if (iResult == IDRETRY)
			{
				CloseDevice(&deviceData);
				continue;
			}
		}
		else {
			wprintf(L"WinUsb_WritePipe success - %d bytes header.\n", cbSent);
		}

		bResult = WinUsb_ReadPipe(deviceData.WinusbHandle, pipeID.PipeInId, (UCHAR *)&tempp, sizeof(STATUSLINK_TEMP_PACK), &cbSent, 0);
		if (!bResult)
		{
			wprintf(L"WinUsb_ReadPipe failure - header.\n");
			iResult = MessageBox(NULL, L"WinUsb_ReadPipe failure - Stage 3", L"BeadaTools V1.2", MB_RETRYCANCEL | MB_ICONERROR);

			if (iResult == IDRETRY)
			{
				CloseDevice(&deviceData);
				continue;
			}
		}
		else {
			wprintf(L"WinUsb_ReadPipe success - %d bytes header.\n", cbSent);
	
		}

		pTemp1 = (STATUSLINK_INFO_PACK *)&tempp;
		wprintf(L"[Device Information]\r\n");
		wprintf(L"Firmware Ver.       %d\r\n", pTemp1->value.firmware_version);
		wprintf(L"StatusLink Ver.       %d\r\n", pTemp1->value.statuslink_version);
		wprintf(L"Screen Resolution : %d x %d\r\n", pTemp1->value.screen_resolution_x, pTemp1->value.screen_resolution_y);
		wprintf(L"On-Board Storage : %4.1f GB\r\n", (float)pTemp1->value.storage_size / 1000000);
		printf("S/N : %s\r\n", pTemp1->value.sn);
		wprintf(L"Brightness : %d:%d\r\n", pTemp1->value.current_brightness, pTemp1->value.max_brightness);

		sprintf(temps, "[Device Information]\r\nFirmware Ver.         %d\r\nScreen Resolution: %d x %d\r\nOn-Board Storage:%4.1f GB\r\n\r\nPress 'Continue' to connect to device.", pTemp1->value.firmware_version, pTemp1->value.screen_resolution_x, pTemp1->value.screen_resolution_y, (float)pTemp1->value.storage_size/1000000);

		isize = MultiByteToWideChar(CP_ACP, 0, temps, strlen(temps), NULL, 0); 
		MultiByteToWideChar(CP_ACP, 0, temps, strlen(temps), buffer, isize);
		buffer[isize] = 0;

		iResult = MessageBox(NULL, buffer, L"BeadaTools V1.2", MB_CANCELTRYCONTINUE | MB_ICONINFORMATION);

			if (iResult == IDTRYAGAIN) {
				CloseDevice(&deviceData);
				continue;
			}
			else if (iResult == IDCONTINUE) {
			if (pTemp1->value.firmware_version >= 502) {


				// GET_TIME command send out here
				pTemp = (STATUSLINK_TAG *)&tempp;
				pTemp->type = TYPE_GET_TIME;
				pTemp->version = 1;
				memcpy(pTemp->protocol_name, protocol_str, strlen(protocol_str));
				pTemp->length = sizeof(STATUSLINK_TAG);
				pTemp->checksum16 = checksum16((unsigned short *)&tempp, (sizeof(STATUSLINK_TAG) - 2) / 2);

				bResult = WinUsb_WritePipe(deviceData.WinusbHandle, pipeID.PipeOutId, (UCHAR *)&tempp, sizeof(STATUSLINK_TAG), &cbSent, 0);
				if (!bResult)
				{
					wprintf(L"WinUsb_WritePipe failure - header.\n");
					iResult = MessageBox(NULL, L"WinUsb_WritePipe failure - Stage 4", L"BeadaTools V1.2", MB_RETRYCANCEL | MB_ICONERROR);

					if (iResult == IDRETRY)
					{
						CloseDevice(&deviceData);
						continue;
					}
				}
				else {
					wprintf(L"WinUsb_WritePipe success - %d bytes header.\n", cbSent);
				}

				bResult = WinUsb_ReadPipe(deviceData.WinusbHandle, pipeID.PipeInId, (UCHAR *)&tempp, sizeof(STATUSLINK_TEMP_PACK), &cbSent, 0);
				if (!bResult)
				{
					wprintf(L"WinUsb_ReadPipe failure - header.\n");
					iResult = MessageBox(NULL, L"WinUsb_ReadPipe failure - Stage 5", L"BeadaTools V1.2", MB_RETRYCANCEL | MB_ICONERROR);

					if (iResult == IDRETRY)
					{
						CloseDevice(&deviceData);
						continue;
					}
				}
				else {
					wprintf(L"WinUsb_ReadPipe success - %d bytes header.\n", cbSent);

				}


				// set_time command send out here
				pTemp = (STATUSLINK_TAG *)&tempp;
				pTemp->type = TYPE_SET_TIME;
				pTemp->version = 1;
				memcpy(pTemp->protocol_name, protocol_str, strlen(protocol_str));
				pTemp->length = sizeof(STATUSLINK_TAG)+sizeof(SYSTEMTIME);
				pTemp->checksum16 = checksum16((unsigned short *)&tempp, (sizeof(STATUSLINK_TAG) - 2) / 2);
				GetLocalTime(&tz);
				*(SYSTEMTIME *)tempp.value = tz;

				bResult = WinUsb_WritePipe(deviceData.WinusbHandle, pipeID.PipeOutId, (UCHAR *)&tempp, sizeof(STATUSLINK_TAG)+sizeof(SYSTEMTIME), &cbSent, 0);
				if (!bResult)
				{
					wprintf(L"WinUsb_WritePipe failure - header.\n");
					iResult = MessageBox(NULL, L"WinUsb_WritePipe failure - Stage 6", L"BeadaTools V1.2", MB_RETRYCANCEL | MB_ICONERROR);

					if (iResult == IDRETRY)
					{
						CloseDevice(&deviceData);
						continue;
					}
				}
				else {
					wprintf(L"WinUsb_WritePipe success - %d bytes header.\n", cbSent);
				}
			}

				// push command send out here
				pTemp = (STATUSLINK_TAG *)&tempp;
				pTemp->type = TYPE_PUSH_STORAGE;
				pTemp->version = 1;
				memcpy(pTemp->protocol_name, protocol_str, strlen(protocol_str));
				pTemp->length = sizeof(STATUSLINK_TAG);
				pTemp->checksum16 = checksum16((unsigned short *)&tempp, (sizeof(STATUSLINK_TAG) - 2) / 2);

				bResult = WinUsb_WritePipe(deviceData.WinusbHandle, pipeID.PipeOutId, (UCHAR *)&tempp, sizeof(STATUSLINK_TAG), &cbSent, 0);
				if (!bResult)
				{
					wprintf(L"WinUsb_WritePipe failure - header.\n");
					iResult = MessageBox(NULL, L"WinUsb_WritePipe failure - Stage 7", L"BeadaTools V1.2", MB_RETRYCANCEL | MB_ICONERROR);

					if (iResult == IDRETRY)
					{
						CloseDevice(&deviceData);
						continue;
					}
				}
				else {
					wprintf(L"WinUsb_WritePipe success - %d bytes header.\n", cbSent);
				}
			}

			CloseDevice(&deviceData);
			return 0;

		}



	}

	return 0;
}
