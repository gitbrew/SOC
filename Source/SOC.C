#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <time.h>
#include "defrag.h"

#define LLINVALID		((ULONGLONG) -1)
#define	FILEMAPSIZE		(16384+2)

BOOLEAN		Silent = FALSE;
BOOLEAN		Recurse = FALSE;
BOOLEAN		ZapFreeSpace = FALSE;
BOOLEAN		CleanCompressedFiles = FALSE;
DWORD		NumPasses = 1;
DWORD		FilesFound = 0;


BOOL (__stdcall *pGetDiskFreeSpaceEx)(
	  LPCTSTR lpDirectoryName,                 
	  PULARGE_INTEGER lpFreeBytesAvailableToCaller, 
													
	  PULARGE_INTEGER lpTotalNumberOfBytes,    
	  PULARGE_INTEGER lpTotalNumberOfFreeBytes 
);
 
void PrintNtError( NTSTATUS status )
{
	TCHAR *errMsg;

	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, RtlNtStatusToDosError( status ), 
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
			(LPTSTR) &errMsg, 0, NULL );
	_tprintf(_T("%s\n"), errMsg );
	LocalFree( errMsg );
}

void PrintWin32Error( DWORD ErrorCode )
{
	LPVOID lpMsgBuf;

	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
					NULL, ErrorCode, 
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					(LPTSTR) &lpMsgBuf, 0, NULL );
	_tprintf(_T("%s\n"), lpMsgBuf );
	LocalFree( lpMsgBuf );
}

VOID OverwriteFileName( PTCHAR FileName, PTCHAR LastFileName )
{
	TCHAR		newName[MAX_PATH];
	PTCHAR		lastSlash;
	DWORD		i, j, index;

	_tcscpy( LastFileName, FileName );
	lastSlash = _tcsrchr( LastFileName, _T('\\'));
	index = (lastSlash - LastFileName)/sizeof(TCHAR);

	_tcscpy( newName, FileName );
	for( i = 0; i < 26; i++ ) {

		for( j = index+1 ; j < _tcsclen( FileName ); j++ ) {

			if( FileName[j] != _T('.')) newName[j] = (TCHAR) i + _T('A');
		}

		if( !MoveFile( LastFileName, newName )) {

			return;
		}

		_tcscpy( LastFileName, newName );
	}
}

BOOLEAN SecureOverwrite( HANDLE FileHandle, DWORD Length )
{
#define CLEANBUFSIZE 65536
	static PBYTE	cleanBuffer[3];
	static BOOLEAN	buffersAlloced = FALSE;

	DWORD		i, j, passes;
	DWORD		bytesWritten, bytesToWrite, totalWritten;
	LONG		seekLength;
	BOOLEAN		status;

	if( !buffersAlloced ) {

		srand( (unsigned)time( NULL ) );
	
		for( i = 0; i < 3; i++ ) {

			cleanBuffer[i] = VirtualAlloc( NULL, CLEANBUFSIZE, MEM_COMMIT, PAGE_READWRITE );
			if( !cleanBuffer[i] ) {

				for( j = 0; j < i; j++ ) {

					VirtualFree( cleanBuffer[j], 0, MEM_RELEASE );
				}
				return FALSE;
			}

			switch( i ) {

			case 0:
				break;
			case 1:
				memset( cleanBuffer[i], 0xFF, CLEANBUFSIZE );
				break;
			case 2:
				for( j = 0; j < CLEANBUFSIZE; j++ ) cleanBuffer[i][j] = (BYTE) rand();
				break;
			}
		}	
		buffersAlloced = TRUE;
	}

	seekLength = (LONG) Length;
	for( passes = 0; passes < NumPasses; passes++ ) {

		if( passes != 0 ) {

			SetFilePointer( FileHandle, -seekLength, NULL, FILE_CURRENT );
		}

		for( i = 0; i < 3; i++ ) {

			if( i != 0 ) {

				SetFilePointer( FileHandle, -seekLength, NULL, FILE_CURRENT );
			}

			bytesToWrite = Length;
			totalWritten = 0;
			while( totalWritten < Length ) {

				bytesToWrite = Length - totalWritten;
				if( bytesToWrite > CLEANBUFSIZE ) bytesToWrite = CLEANBUFSIZE;

				status = WriteFile( FileHandle, cleanBuffer[i], bytesToWrite, &bytesWritten, NULL );
				if( !status ) return FALSE;

				totalWritten += bytesWritten;
			}
		}
	}
	return TRUE;
}

VOID SecureDelete( PTCHAR FileName, DWORD FileLengthHi,
					DWORD FileLengthLo ) 
{
	HANDLE	hFile;
	ULONGLONG bytesToWrite, bytesWritten;
	ULARGE_INTEGER fileLength;
	TCHAR   lastFileName[MAX_PATH];

	hFile = CreateFile( FileName, GENERIC_WRITE, 
						FILE_SHARE_READ|FILE_SHARE_WRITE,
						NULL, CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL );
	if( hFile == INVALID_HANDLE_VALUE ) {

		_tprintf( _T("\nError opening %s for delete: "), FileName );
		PrintWin32Error( GetLastError());
		return;
	}

	if( FileLengthLo || FileLengthHi ) {

		FileLengthLo--;
		if( FileLengthLo == (DWORD) -1 && FileLengthHi ) FileLengthHi--;
		SetFilePointer( hFile, FileLengthLo, &FileLengthHi, FILE_BEGIN );

		if( !SecureOverwrite( hFile, 1 )) {

			_tprintf( _T("\nError overwriting %s: "), FileName );
			PrintWin32Error( GetLastError() );
			CloseHandle( hFile );
			return;
		}

		SetFilePointer( hFile, 0, NULL, FILE_BEGIN );
		fileLength.LowPart = FileLengthLo;
		fileLength.HighPart = FileLengthHi;
		bytesWritten = 0;
		while( bytesWritten < fileLength.QuadPart ) {

			bytesToWrite = min( fileLength.QuadPart - bytesWritten, 65536 );
			if( !SecureOverwrite( hFile, (DWORD) bytesToWrite )) {

				_tprintf( _T("\nError overwriting %s: "), FileName );
				PrintWin32Error( GetLastError() );
				CloseHandle( hFile );
				return;
			}
			bytesWritten += bytesToWrite;
		}
	}

	CloseHandle( hFile );
	
	OverwriteFileName( FileName, lastFileName );

	if( !DeleteFile( lastFileName ) ) {

		_tprintf( _T("\nError deleting %s: "), FileName );
		PrintWin32Error( GetLastError() );

		if( !MoveFile( lastFileName, FileName )) {

			_tprintf( _T("\nError renaming file back to original name. File is left as %s\n"),
				lastFileName );
		}
		return;
	}

	if( !Silent ) _tprintf( _T("deleted.\n"));
}

BOOLEAN ScanFile( HANDLE VolumeHandle,
				  DWORD ClusterSize,
				  HANDLE FileHandle, 
				  PBOOLEAN ReallyCompressed,
				  PBOOLEAN ZappedFile )
{
	DWORD						status;
	int							i;
	IO_STATUS_BLOCK				ioStatus;
	ULONGLONG					startVcn, prevVcn;
	LARGE_INTEGER				clusterOffset;
	ULONGLONG					endOfPrevRun;
	PGET_RETRIEVAL_DESCRIPTOR	fileMappings;
	ULONGLONG					fileMap[ FILEMAPSIZE ];
	int							lines = 0;

	*ReallyCompressed = FALSE;
	*ZappedFile = FALSE;

	startVcn = 0;
	endOfPrevRun = LLINVALID;
	fileMappings = (PGET_RETRIEVAL_DESCRIPTOR) fileMap;
	while( !(status = NtFsControlFile( FileHandle, NULL, NULL, 0, &ioStatus,
						FSCTL_GET_RETRIEVAL_POINTERS,
						&startVcn, sizeof( startVcn ),
						fileMappings, FILEMAPSIZE * sizeof(ULONGLONG) ) ) ||
			 status == STATUS_BUFFER_OVERFLOW ||
			 status == STATUS_PENDING ) {

		if( status == STATUS_PENDING ) {
			
			WaitForSingleObject( FileHandle, INFINITE ); 

			if( ioStatus.Status != STATUS_SUCCESS && 
				ioStatus.Status != STATUS_BUFFER_OVERFLOW ) {

				return ioStatus.Status == STATUS_SUCCESS;
			}
		}

		startVcn = fileMappings->StartVcn;
		prevVcn  = fileMappings->StartVcn;
		for( i = 0; i < (ULONGLONG) fileMappings->NumberOfPairs; i++ ) {	 

			if( fileMappings->Pair[i].Lcn != LLINVALID ) {

				*ReallyCompressed = TRUE;

				if( VolumeHandle != INVALID_HANDLE_VALUE ) {
			

					clusterOffset.QuadPart = fileMappings->Pair[i].Lcn * ClusterSize;
					SetFilePointer( VolumeHandle, clusterOffset.LowPart,
									&clusterOffset.HighPart, FILE_BEGIN );
					if( !SecureOverwrite( VolumeHandle,
									ClusterSize * (DWORD) (fileMappings->Pair[i].Vcn - startVcn) )) {

						return TRUE;
					}							

				} else {

					return TRUE;	
				}
			}
			startVcn = fileMappings->Pair[i].Vcn;
		}

		if( !status ) break;
	}

	if( status && status != STATUS_INVALID_PARAMETER && !Silent ) {

		printf("Scanning file: ");
		PrintNtError( status );
	}

	if( status == STATUS_SUCCESS ) *ZappedFile = TRUE;
	return status == STATUS_SUCCESS;
}

BOOLEAN SecureDeleteCompressed( PTCHAR FileName ) 
{
	HANDLE			hFile;
	BOOLEAN			reallyCompressed = FALSE;
	BOOLEAN			zappedFile = FALSE;
	TCHAR			lastFileName[MAX_PATH];
	static TCHAR	volumeName[] = _T("\\\\.\\A:");
	static TCHAR	volumeRoot[] = _T("A:\\");
	static HANDLE	hVolume = INVALID_HANDLE_VALUE;
	static DWORD	clusterSize;
	DWORD			sectorsPerCluster, bytesPerSector, freeClusters, totalClusters;

	if( hVolume == INVALID_HANDLE_VALUE ) {

		volumeName[4] = FileName[0];
		hVolume = CreateFile( volumeName, GENERIC_READ|GENERIC_WRITE,
							FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
							0, 0 );

		volumeRoot[0] = FileName[0];
		GetDiskFreeSpace( volumeRoot, &sectorsPerCluster, &bytesPerSector,
						&freeClusters, &totalClusters );

		clusterSize = bytesPerSector * sectorsPerCluster;
	}

	hFile = CreateFile( FileName, GENERIC_READ, 
						0,NULL, OPEN_EXISTING, 0, NULL );
	if( hFile == INVALID_HANDLE_VALUE ) {

		_tprintf( _T("\nError opening %s for compressed file scan: "), FileName );
		PrintWin32Error( GetLastError());
		return TRUE;
	}
	
	if( !ScanFile( hVolume, clusterSize, hFile, 
			&reallyCompressed, &zappedFile )) {

		CloseHandle( hFile );
		return TRUE;
	}

	CloseHandle( hFile );

	if( reallyCompressed ) {

		CloseHandle( hFile );

		OverwriteFileName( FileName, lastFileName );

		if( !DeleteFile( lastFileName )) {

			_tprintf( _T("\nError deleting %s: "), FileName );
			PrintWin32Error( GetLastError() );
			if( !MoveFile( lastFileName, FileName )) {

				_tprintf( _T("\nError renaming file back to original name. File is left as %s\n"),
					lastFileName );
			}
			return TRUE;
		}

		if( !zappedFile ) CleanCompressedFiles = TRUE;

		if( !Silent ) _tprintf( _T("deleted.\n"));
	}

	return reallyCompressed;
}

VOID ProcessFile( PWIN32_FIND_DATA FindData, TCHAR *FileName )
{
	if( FindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) return;

	FilesFound++;

	if( !Silent ) {
		
		_tprintf( _T("%s..."), FileName );
		fflush( stdout );
	}

	if( FindData->dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED ||
		FindData->dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED  ||
		FindData->dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE ) {

		if( SecureDeleteCompressed( FileName )) return;
	} 

	SecureDelete( FileName, FindData->nFileSizeHigh,
							FindData->nFileSizeLow );
}

void ProcessDirectory( TCHAR *PathName, TCHAR *SearchPattern )
{
	TCHAR			subName[MAX_PATH], fileSearchName[MAX_PATH], searchName[MAX_PATH];
	HANDLE			dirHandle, patternHandle;
	static BOOLEAN	firstCall = TRUE;
	static BOOLEAN  deleteDirectories = FALSE;
	WIN32_FIND_DATA foundFile;

	if( firstCall ) {

		if( _tcsrchr( PathName, '*' ) ) {
	
            if( _tcsrchr( PathName, '\\' ) ) {

                _stprintf( SearchPattern, _tcsrchr( PathName, '\\' )+1 );
                _tcscpy( searchName, PathName );
                _tcscpy( _tcsrchr( searchName, '\\')+1, _T("*.*") );
				if( !_tcscmp( SearchPattern, _T("*.*")) ||
					!_tcscmp( SearchPattern, _T("*"))) {

					deleteDirectories = TRUE;
				}

            } else {
                
                _stprintf( SearchPattern, PathName );
                _tcscpy( searchName, PathName );
            }
            _stprintf( fileSearchName, _T("%s"), PathName );

		} else {

			_stprintf( SearchPattern, _T("*.*") );
			_stprintf( searchName, _T("%s"), PathName );
            _stprintf( fileSearchName, _T("%s"), PathName );
			deleteDirectories = TRUE;
		}

	} else {

		_stprintf( searchName, _T("%s\\*.*"), PathName );
		_stprintf( fileSearchName, _T("%s\\%s"), PathName, SearchPattern );
	}

	if( (patternHandle = FindFirstFile( fileSearchName, &foundFile )) != 
		INVALID_HANDLE_VALUE  ) {

		do {

			if( _tcscmp( foundFile.cFileName, _T(".") ) &&
				_tcscmp( foundFile.cFileName, _T("..") )) {

				_tcscpy( subName, searchName );
				if( _tcsrchr( subName, '\\' ) ) 
					_tcscpy( _tcsrchr( subName, '\\')+1, foundFile.cFileName );
				else
					_tcscpy( subName, foundFile.cFileName );

				ProcessFile( &foundFile, subName );
			}
		} while( FindNextFile( patternHandle, &foundFile ));
		FindClose( patternHandle );
	}

	if( Recurse ) {

        if( firstCall && !_tcsrchr( searchName, L'\\') ) {

            if( _tcsrchr( searchName, L'*' )) {

                if( (dirHandle = FindFirstFile( _T("*.*"), &foundFile )) == 
                    INVALID_HANDLE_VALUE  ) {

			return;
                }
            } else {

                if( (dirHandle = FindFirstFile( searchName, &foundFile )) == 
                    INVALID_HANDLE_VALUE  ) {

                    return;
                }
            }
        } else {

            if( (dirHandle = FindFirstFile( searchName, &foundFile )) == 
                INVALID_HANDLE_VALUE  ) {

                return;
            }
        }
        firstCall = FALSE;

		do {

			if( (foundFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
				_tcscmp( foundFile.cFileName, _T(".") ) &&
				_tcscmp( foundFile.cFileName, _T("..") )) {

				_tcscpy( subName, searchName );
				if( _tcsrchr( subName, '\\' ) ) 
					_tcscpy( _tcsrchr( subName, '\\')+1, foundFile.cFileName );
				else
					_tcscpy( subName, foundFile.cFileName );

				ProcessDirectory( subName, SearchPattern );

				if( deleteDirectories ) {

					if( !RemoveDirectory( subName )) {

						_tprintf( _T("\nError deleting %s: "), subName );
						PrintWin32Error( GetLastError() );
					}
				}
			}

		} while( FindNextFile( dirHandle, &foundFile ));
		FindClose( dirHandle );
	}
}

BOOLEAN CleanFreeSpace( PTCHAR DrivePath )
{
	TCHAR		tempFileName[MAX_PATH];
	ULARGE_INTEGER bytesAvail, totalBytes, freeBytes;
	DWORD		sectorsPerCluster, bytesPerSector, totalClusters, freeClusters;
	ULONGLONG	tempSize = 0;
	TCHAR		progress[] = _T("|/-\\|/-\\");
	HANDLE		hTempFile;
	BOOLEAN		createdFile;
	DWORD		percent, cleanSize, mftFilesCreated;
	DWORD		prevSize, prevPercent = 0;
	
	if( DrivePath[1] != ':' ) {

		_tprintf( _T("Cannot clean free space for UNC drive\n\n"));
		return FALSE;
	}

	DrivePath[3] = 0;

	if( CleanCompressedFiles ) {

		_tprintf(_T("Cleaning free space to securely delete compressed files: 0%%"));
	} else {

		_tprintf(_T("Cleaning free space on %s: 0%%"), DrivePath );
	}
	fflush( stdout );

	if( !GetDiskFreeSpace( DrivePath, &sectorsPerCluster, &bytesPerSector,
		&freeClusters, &totalClusters )) {

		_tprintf( _T("Could not determine disk cluster size: "));
		PrintWin32Error( GetLastError());
		return FALSE;
	}

#if UNICODE
	if( !(pGetDiskFreeSpaceEx = (PVOID) GetProcAddress( GetModuleHandle( _T("kernel32.dll") ),
											"GetDiskFreeSpaceExW" ))) {
#else
	if( !(pGetDiskFreeSpaceEx = (PVOID) GetProcAddress( GetModuleHandle( _T("kernel32.dll") ),
											"GetDiskFreeSpaceExA" ))) {
#endif

		bytesAvail.QuadPart = sectorsPerCluster * freeClusters * bytesPerSector;
        freeBytes.QuadPart = bytesAvail.QuadPart;

	} else {
	
		if( !pGetDiskFreeSpaceEx( DrivePath, &bytesAvail, &totalBytes, &freeBytes )) {

			_tprintf( _T("Could not determine amount of free space: "));
			PrintWin32Error( GetLastError());
			return FALSE;
		}
	}

	if( bytesAvail.QuadPart != freeBytes.QuadPart ) {

		_tprintf(_T("Your disk quota prevents you from cleaning free space on this drive.\n\n"));
		return FALSE;
	}

	_stprintf( tempFileName, _T("%sSDELTEMP"), DrivePath );
	hTempFile = CreateFile( tempFileName, GENERIC_WRITE, 
					0, NULL, CREATE_NEW, 
					FILE_FLAG_NO_BUFFERING|FILE_FLAG_SEQUENTIAL_SCAN|
					FILE_FLAG_DELETE_ON_CLOSE|FILE_ATTRIBUTE_HIDDEN, NULL );

	if( hTempFile == INVALID_HANDLE_VALUE ) {

		_tprintf( _T("Could not create free-space cleanup file: "));
		PrintWin32Error( GetLastError());
		return FALSE;
	}

	cleanSize = sectorsPerCluster * bytesPerSector * 128;

	while( cleanSize > bytesPerSector * sectorsPerCluster ) {

		if( SecureOverwrite( hTempFile, cleanSize )) {

			tempSize += cleanSize;

			percent = (DWORD) ((tempSize * 100)/ freeBytes.QuadPart );

			if( percent != prevPercent ) {
				if( CleanCompressedFiles ) {

					_tprintf(_T("\rCleaning free space to securely delete compressed files: %d%%"),
						percent );
				} else {

					_tprintf(_T("\rCleaning free space on %s: %d%%"), DrivePath, percent );
				}
				prevPercent = percent;
			}
		} else {

			cleanSize -= bytesPerSector * sectorsPerCluster;
		}
	}

	_stprintf( tempFileName, _T("%sSDELTEMP1"), DrivePath );
	hTempFile = CreateFile( tempFileName, GENERIC_WRITE, 
					0, NULL, CREATE_NEW, 
					FILE_FLAG_SEQUENTIAL_SCAN|FILE_FLAG_DELETE_ON_CLOSE|
					FILE_ATTRIBUTE_HIDDEN|FILE_FLAG_WRITE_THROUGH, NULL );

	if( hTempFile != INVALID_HANDLE_VALUE ) {

		while( cleanSize ) {

			if( !SecureOverwrite( hTempFile, cleanSize )) {

				cleanSize--;
			}
		}
	}

	if( ZapFreeSpace ) {

		mftFilesCreated = 0;
		prevSize = 4096; 
		while( 1 ) {

			_stprintf( tempFileName, _T("%sSDELMFT%06d"), DrivePath, mftFilesCreated++ );
			hTempFile = CreateFile( tempFileName, GENERIC_WRITE, 
							0, NULL, CREATE_NEW, 
							FILE_FLAG_SEQUENTIAL_SCAN|FILE_FLAG_DELETE_ON_CLOSE|
							FILE_ATTRIBUTE_HIDDEN, NULL );
			if( hTempFile == INVALID_HANDLE_VALUE ) {

				break;
			}

			cleanSize = prevSize;
			createdFile = FALSE;
			while( cleanSize ) {

				if( !SecureOverwrite( hTempFile, cleanSize )) {

					cleanSize--;

				} else {

					prevSize = cleanSize;
					createdFile = TRUE;
				}
			}	
			
			if( !createdFile ) break;

			if( mftFilesCreated == 1 ) {
				_tprintf( _T("\r                                                           "));
				
			} 

			_tprintf( _T("\rCleaning MFT...%c"),
				progress[ mftFilesCreated % 8 ]);

		}
	}

	_tprintf(_T("\rFree space cleaned on %s                                       \n"),
		DrivePath );
	return TRUE;
}

VOID LocateNativeEntryPoints()
{
	if( GetVersion() >= 0x80000000) return;

	if( !(NtFsControlFile = (void *) GetProcAddress( GetModuleHandle(_T("ntdll.dll")),
			"NtFsControlFile" )) ) {

		_tprintf(_T("\nCould not find NtFsControlFile entry point in NTDLL.DLL\n"));
		exit(1);
	}
	if( !(RtlNtStatusToDosError = (void *) GetProcAddress( GetModuleHandle(_T("ntdll.dll")),
							"RtlNtStatusToDosError" )) ) {

		_tprintf(_T("\nCould not find RtlNtStatusToDosError entry point in NTDLL.DLL\n"));
		exit(1);
	}
}

int Usage( TCHAR *ProgramName ) 
{
    _tprintf(_T("usage: %s [-p passes] [-s] [-q] <file or directory>\n"), ProgramName );
	_tprintf(_T("       %s [-p passes] -z [drive letter]\n"), ProgramName );
	_tprintf(_T("   -p passes  Specifies number of overwrite passes (default is 1)\n"));
    _tprintf(_T("   -s         Recurse subdirectories\n"));
    _tprintf(_T("   -q         Don't print errors (Quiet)\n"));
	_tprintf(_T("   -z         Clean free space\n\n"));
    return -1;
}

int _tmain( int argc, TCHAR *argv[] ) 
{
    TCHAR       searchPattern[MAX_PATH];
	TCHAR		searchPath[MAX_PATH];
	PTCHAR		filePart;
	BOOL		foundFileArg = FALSE;
	int			i;

    //
    // Print banner and perform parameter check
    //
    _tprintf(_T("\n - SOC v2\n") );
    _tprintf(_T("Copyright (C) 2013 Kyle Barnthouse (durandal)\n"));
    _tprintf(_T("http://gitbrew.org A Gitbrew Release\n\n"));

    if( argc < 2 ) {

        return Usage( argv[0] );
    }

    for( i = 1; i < argc; i++ ) {

	    if( !_tcsicmp( argv[i], _T("/s") ) ||
			!_tcsicmp( argv[i], _T("-s") )) {

			Recurse = TRUE;

		} else if( !_tcsicmp( argv[i], _T("/q") ) ||
				   !_tcsicmp( argv[i], _T("-q") )) {

			Silent = TRUE;

		} else if( !_tcsicmp( argv[i], _T("/z") ) ||
				   !_tcsicmp( argv[i], _T("-z") )) {

			ZapFreeSpace = TRUE;

		} else if( !_tcsicmp( argv[i], _T("/p") ) ||
				   !_tcsicmp( argv[i], _T("-p") )) {

			NumPasses = atoi( argv[i+1] );
			if( !NumPasses ) return Usage( argv[0] );
			i++;

		} else if( !_tcsicmp( argv[i], _T("/?") ) ||
				   !_tcsicmp( argv[i], _T("-?") )) {
				   
			return Usage( argv[0] );

		} else {

			if( foundFileArg ) return Usage( argv[0] );
			foundFileArg = TRUE;
		}
    }

	if( !ZapFreeSpace && !foundFileArg ) {

		return Usage( argv[0] );
	}

	LocateNativeEntryPoints();

	if( foundFileArg ) {
		
		GetFullPathName( argv[argc-1], MAX_PATH, searchPath, &filePart );
	}
	printf("SOC is set for %d pass%s.\n", NumPasses,
		NumPasses > 1 ? "es" : "");
	
	if( !ZapFreeSpace ) {

		ProcessDirectory( searchPath, searchPattern );

		if( !FilesFound ) _tprintf(_T("No files found that match %s.\n"), argv[argc-1] );

	} else if( !foundFileArg ) {

		GetCurrentDirectory( MAX_PATH, searchPath );
	} 

	if( CleanCompressedFiles || ZapFreeSpace ) {

		CleanFreeSpace( searchPath );
	}

	_tprintf(_T("\n"));
    return 0;
}

