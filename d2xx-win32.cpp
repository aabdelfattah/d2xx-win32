#include <windows.h>
#include <stdio.h>
#include <time.h>
#include "ftd2xx.h"

#define READ_BUF_SIZE 1200
#define WRITE_BUF_SIZE 1200
DWORD BytesRead = 0;
DWORD BytesWritten = 0;
DWORD BytesMissed = 0;




typedef struct SerialParameters 
{
	HANDLE hCom;
};



int OpenFTDI(FT_HANDLE *ftHandle)
{
	FT_STATUS ftStatus;
	
	char p8DevSerial[256] ;
	
	
	
	FTDCB PortDCB;
	FTTIMEOUTS timeouts;

	strcpy(p8DevSerial,"AM024YYS");

	*ftHandle = FT_W32_CreateFile(	p8DevSerial,GENERIC_READ|GENERIC_WRITE,0,0,
									OPEN_EXISTING,
									FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED |
									FT_OPEN_BY_SERIAL_NUMBER,
									0);
	if (*ftHandle == INVALID_HANDLE_VALUE)
	{
		printf("FTDI: Error can't open device");
		return 0;
	}


		/*Configure Com port */
	// Initialize the DCBlength member. 
	PortDCB.DCBlength = sizeof (DCB); 

	// Get the default port setting information.
	if(!FT_W32_GetCommState (*ftHandle, &PortDCB))
	{
		printf("Could not Set Port Settings\n");
		return 0 ;
	}

	PortDCB.BaudRate = 3000000; 
	PortDCB.ByteSize = 8;                 // Number of bits/byte, 4-8 
	PortDCB.Parity = NOPARITY;            // 0-4=no,odd,even,mark,space 
	PortDCB.StopBits = ONESTOPBIT;
	PortDCB.fOutxCtsFlow = 0;
	PortDCB.fDtrControl = 0;
	PortDCB.fRtsControl = 0x02;			//disable rts/cts handshake flow control
	PortDCB.ByteSize = 0x08;


	// Configure the port according to the specifications of the DCB 
	// structure.
	if (!FT_W32_SetCommState(*ftHandle, &PortDCB))
	{
		printf("Could not Set Port Settings\n");
		return 0 ;
	}	

	if(!FT_W32_GetCommTimeouts(*ftHandle, &timeouts)) 
	{
		printf("Could not Set Port Settings\n");
		return 0 ;
	}

	ZeroMemory(&timeouts, sizeof(timeouts));
	timeouts.ReadTotalTimeoutConstant = 0x00;
	if(!FT_W32_SetCommTimeouts(*ftHandle, &timeouts))
	{
		printf("Could not Set Port Settings\n");
		return 0 ;
	}


	/*status = FT_OpenEx((PVOID)(LPCTSTR)p8DevSerial, FT_OPEN_BY_SERIAL_NUMBER,&ftHandle);
	if (status != FT_OK)
	{
		printf("FTDI: Error can't open device");
		return ftStatus;
	}*/
	/*
	ftStatus = FT_SetBaudRate(ftHandle, 115200); // Set baud rate to 115200
	if (ftStatus != FT_OK) 
	{
		printf("FTDI: Error can't set baud ");
		FT_Close(ftHandle);
		return ftStatus;
	}*/


	DWORD rsize;
    ftStatus = FT_GetQueueStatus(*ftHandle,&rsize);
    if (ftStatus != FT_OK) {
        printf("Unable to get Queue status\n");
    } else {
        printf("At Init there are %d characters in read queue\n",rsize);
    }

	ftStatus = FT_Purge(*ftHandle,FT_PURGE_RX | FT_PURGE_TX);
    if (ftStatus != FT_OK) {
        printf("Unable to clear USB read/write buffers\n");
        FT_Close(ftHandle);
        return ftStatus;
    }

	ftStatus = FT_ResetDevice(*ftHandle);
    if (ftStatus != FT_OK) {
        printf("Unable to reset USB FIFO\n");
        FT_Close(ftHandle);
        return ftStatus;
    }


	ftStatus = FT_GetQueueStatus(*ftHandle,&rsize);
    if (ftStatus != FT_OK) {
        printf("Unable to get Queue status\n");
    } else {
        printf("After purge/reset there are %d characters in read queue\n",rsize);
    }

	return ftStatus;



}


void CALCULATE_PKT_CHECKSUM(BYTE *pPkt,
							INT16 u16PktSize,
							INT32 *u32Sum)
{
	
	(*u32Sum) = 0;
	INT16* p16pkt = ((INT16 *) pPkt);
	// Main summing loop
	while(u16PktSize > 1)
	{
	  (*u32Sum) = (*u32Sum) + *p16pkt++;
	  u16PktSize = u16PktSize - 2;
	}

	// Add left-over byte, if any
	if (u16PktSize > 0)
		(*u32Sum) = (*u32Sum) + *((BYTE *) p16pkt);

	// Fold 32-bit sum to 16 bits
	while ((*u32Sum)>>16)
		(*u32Sum) = ((*u32Sum) & 0xFFFF) + ((*u32Sum) >> 16);


	


}

void HandleASuccessfulRead(BYTE* lpBuf, DWORD dwRead)
{
	BytesRead += dwRead;
}
void HandleASuccessfulWrite(DWORD dwWritten)
{
	BytesWritten += dwWritten;
}


BOOL WriteABuffer(HANDLE hCom, BYTE * lpBuf, DWORD dwToWrite)
{
   OVERLAPPED osWrite = {0};
   DWORD dwWritten;
   BOOL fRes;
   DWORD dwRes;

   // Create this writes OVERLAPPED structure hEvent.
   osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
   if (osWrite.hEvent == NULL)
      // Error creating overlapped event handle.
      return FALSE;

   // Issue write.
   if (!WriteFile(hCom, lpBuf, dwToWrite, &dwWritten, &osWrite)) {
      if (GetLastError() != ERROR_IO_PENDING) { 
         // WriteFile failed, but it isn't delayed. Report error and abort.
         fRes = FALSE;
      }
      else {
         // Write is pending.
         dwRes = WaitForSingleObject(osWrite.hEvent, INFINITE);
         switch(dwRes)
         {
            // OVERLAPPED structure's event has been signaled. 
            case WAIT_OBJECT_0:
                 if (!GetOverlappedResult(hCom, &osWrite, &dwWritten, FALSE))
                       fRes = FALSE;
				 else{
                  // Write operation completed successfully.
					HandleASuccessfulWrite(dwWritten);				
					 fRes = dwWritten;
				 }
                 break;
            
            default:
                 // An error has occurred in WaitForSingleObject.
                 // This usually indicates a problem with the
                // OVERLAPPED structure's event handle.
                 fRes = FALSE;
                 break;
         }
      }
   }
   else{
      // WriteFile completed immediately.
	   HandleASuccessfulWrite(dwWritten);
      fRes = dwWritten;
   }
   CloseHandle(osWrite.hEvent);
   return fRes;
}

BOOL ReadABuffer(HANDLE hCom, BYTE * lpBuf, DWORD dwToRead)
{
   OVERLAPPED osRead = {0};
   DWORD dwRead;
   BOOL fRes;
   DWORD dwRes;

   // Create this osRead OVERLAPPED structure hEvent.
   osRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
   if (osRead.hEvent == NULL)
      // Error creating overlapped event handle.
      return FALSE;

   // Issue read.
   if (!ReadFile(hCom, lpBuf, dwToRead, &dwRead, &osRead)) {
      if (GetLastError() != ERROR_IO_PENDING) { 
         // ReadFile failed, but it isn't delayed. Report error and abort.
         fRes = FALSE;
      }
      else {
         // Read is pending.
		  // Block here till request completed
         dwRes = WaitForSingleObject(osRead.hEvent, INFINITE);
         switch(dwRes)
         {
            // OVERLAPPED structure's event has been signaled. 
            case WAIT_OBJECT_0:
                 if (!GetOverlappedResult(hCom, &osRead, &dwRead, FALSE))
                       fRes = FALSE;
				 else{
                  
					 // Read operation completed successfully.
					 HandleASuccessfulRead(lpBuf, dwRead);
					 fRes = dwRead;
				 }
                 break;
            
            default:
                 // An error has occurred in WaitForSingleObject.
                 // This usually indicates a problem with the
                // OVERLAPPED structure's event handle.
                 fRes = FALSE;
                 break;
         }
      }
   }
   else{
		 // ReadFile completed immediately.
		HandleASuccessfulRead(lpBuf, dwRead);
		fRes = dwRead;
   }
   CloseHandle(osRead.hEvent);
   return fRes;
}


BOOL FTReadABuffer(FT_HANDLE ftHandle, BYTE * lpBuf, DWORD dwToRead)
{
	//FT_HANDLE ftHandle; // setup by FT_W32_CreateFile for overlapped i/o
	DWORD dwRead;
	BOOL fRes;
	OVERLAPPED osRead = { 0 };
	osRead.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (!FT_W32_ReadFile(ftHandle, lpBuf, dwToRead, &dwRead, &osRead)) 
	{
		if (FT_W32_GetLastError(ftHandle) == ERROR_IO_PENDING) 
		{
			// read is delayed so do some other stuff until ...
			if (!FT_W32_GetOverlappedResult(ftHandle, &osRead, &dwRead, TRUE))
			{
				// error
				fRes =  -1;
			}
			else 
			{
				if (dwToRead == dwRead)
				{
					// FT_W32_ReadFile OK
					HandleASuccessfulRead(lpBuf, dwRead);
					fRes = dwRead;
				}
				else
				{
					// FT_W32_ReadFile timeout
					fRes = -1;
				}
			}
		}
	}
	else 
	{
		// FT_W32_ReadFile OK
		HandleASuccessfulRead(lpBuf, dwRead);
		fRes = dwRead;
	}

	CloseHandle (osRead.hEvent);
	return fRes;

}


BOOL FTWriteABuffer(FT_HANDLE ftHandle, BYTE * lpBuf, DWORD dwToWrite)
{
	//FT_HANDLE ftHandle; // setup by FT_W32_CreateFile for overlapped i/o
	DWORD dwWritten;
	BOOL fRes;
	OVERLAPPED osWrite = { 0 };
	osWrite.hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);

	if (!FT_W32_WriteFile(ftHandle, lpBuf, dwToWrite, &dwWritten, &osWrite)) 
	{
		if (FT_W32_GetLastError(ftHandle) == ERROR_IO_PENDING) 
		{
			// read is delayed so do some other stuff until ...
			if (!FT_W32_GetOverlappedResult(ftHandle, &osWrite, &dwWritten, TRUE))
			{
				// error
				fRes =  -1;
			}
			else 
			{
				if (dwToWrite == dwWritten)
				{
					// FT_W32_WriteFile OK
					HandleASuccessfulWrite(dwWritten);
					fRes =  dwWritten;
				}
				else
				{
					// FT_W32_WriteFile timeout
					fRes =  -1;
				}
			}
		}
	}
	else 
	{
		// FT_W32_WriteFile OK
		HandleASuccessfulWrite(dwWritten);
		fRes =  dwWritten;
	}
	CloseHandle (osWrite.hEvent);
	return fRes;

}


DWORD WINAPI WriterThread(LPVOID lpParam)
{

	SerialParameters *s =  (SerialParameters *)lpParam;
	INT32 ret = 0 , sum = 0;
	BYTE lpBuf[WRITE_BUF_SIZE];
	for (int i = 0 ; i < sizeof(lpBuf) ; i++)
		lpBuf[i]=i;
	CALCULATE_PKT_CHECKSUM(lpBuf,sizeof(lpBuf)-2,&sum);
	lpBuf[WRITE_BUF_SIZE-2] = (BYTE) ((sum) & 0xFF) ;
	lpBuf[WRITE_BUF_SIZE-1] =  (BYTE)((sum>>8) & 0xFF);
	while(1)
	{

		while (ret < sizeof(lpBuf))
		{
			//ret += WriteABuffer(s->hCom,lpBuf+ret, (sizeof(lpBuf)-ret) );
			ret += FTWriteABuffer((FT_HANDLE)s->hCom,lpBuf+ret, (sizeof(lpBuf)-ret) );
		}
		ret = 0;

	}

}

DWORD WINAPI ReaderThread(LPVOID lpParam)
{
	SerialParameters *s =  (SerialParameters *)lpParam;
	INT32 ret = 0, sum = 0; INT16 u16CalculatedSum =0 , u16RxSum=0;
	BYTE lpBuf[READ_BUF_SIZE];
	memset(lpBuf,0,sizeof(lpBuf));

	while(1)
	{
		while (ret < sizeof(lpBuf))
		{
			//ret += ReadABuffer(s->hCom,lpBuf+ret , (sizeof(lpBuf)-ret) );
			ret += FTReadABuffer((FT_HANDLE)s->hCom,lpBuf+ret , (sizeof(lpBuf)-ret) );

		}
		ret = 0 ;

		CALCULATE_PKT_CHECKSUM(lpBuf,sizeof(lpBuf)-2,&sum);
		u16CalculatedSum = sum & 0x0000FFFF ;
		u16RxSum = (   lpBuf[READ_BUF_SIZE-2]    ) 
			| (  lpBuf[READ_BUF_SIZE-1] << 8   ) ;

		if (u16CalculatedSum != u16RxSum)
		{
			printf("miss .. \n");
			BytesMissed += READ_BUF_SIZE;
			break;
		}

	}
	return 1;

}






int main()
{
	DWORD dwWThreadId,dwRThreadId, dwThrdParam = 1;
	HANDLE hWThread;
	HANDLE hRThread;
	SerialParameters s;
	HANDLE hSerial;
	FT_HANDLE ftHandle;
	DCB dcbSerialParams = {0};
	COMMTIMEOUTS timeouts = {0};
	time_t t;
	struct tm tm;
	float fTxThroughput,fRxThroughput;
	unsigned long tTimeElapsed=0;
		
	// Open the serial port 
	printf("Opening serial port...");
	if (OpenFTDI(&ftHandle))
	{
			printf("Error\n");
			return 1;
	}else
		printf("Ok \n");

	s.hCom = (HANDLE)ftHandle;



	

	hWThread = CreateThread(
							NULL, // default security attributes
							0, // use default stack size
							WriterThread, // thread function
							&s, // argument to thread function
							0, // use default creation flags
							&dwWThreadId); // returns the thread identifier
						 
	printf("The Write thread ID: %d.\n", dwWThreadId);
	 
	// Check the return value for success. If something wrong...
	if (hWThread == NULL)
		printf("CreateThread() failed, error: %d.\n", GetLastError());

	hRThread = CreateThread(
							NULL, // default security attributes
							0, // use default stack size
							ReaderThread, // thread function
							&s, // argument to thread function
							0, // use default creation flags
							&dwRThreadId); // returns the thread identifier
	 
	printf("The reader thread ID: %d.\n", dwRThreadId);
	 
	// Check the return value for success. If something wrong...
	if (hRThread == NULL)
		printf("CreateThread() failed, error: %d.\n", GetLastError());



	while(1)
	{
		Sleep(5*1000);
		tTimeElapsed +=5;
		t = time(NULL);
		tm = *localtime(&t);
		printf(
			"Time___%d_%d_%d_%d_%d_%d \r\n   Tx = %d, Rx  = %d , Missed = %d\n", 
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec,BytesRead,BytesWritten,BytesMissed );


		fTxThroughput = (float)(BytesWritten * 8 ) / (tTimeElapsed * 1024  * 1024);
		fRxThroughput = (float)(BytesRead * 8 ) / (tTimeElapsed * 1024  * 1024);
		printf(
			"Tx Throughput = %0.9f Mbps ,Rx Throughput = %0.9f Mbps\n"
			,fTxThroughput,fRxThroughput
			);



	}
	
	if (CloseHandle(hWThread) != 0)
		printf("Handle to writer thread closed successfully.\n");
	if (CloseHandle(hRThread) != 0)
		printf("Handle to reader thread closed successfully.\n");

	// Close serial port
	printf("Closing serial port...");
	if (CloseHandle(hSerial) == 0)
	{
		printf("Error\n");
		return 1;
	}
	printf("OK\n");

	// exit normally
	return 0;
}
