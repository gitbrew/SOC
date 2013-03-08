#define FSCTL_GET_VOLUME_INFORMATION	0x90064
#define	FSCTL_READ_MFT_RECORD			0x90068
#define FSCTL_GET_VOLUME_BITMAP			0x9006F
#define FSCTL_GET_RETRIEVAL_POINTERS	0x90073
#define FSCTL_MOVE_FILE					0x90074

typedef UINT NTSTATUS;

#define STATUS_SUCCESS			         ((NTSTATUS)0x00000000L)
#define STATUS_BUFFER_OVERFLOW           ((NTSTATUS)0x80000005L)
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#define STATUS_BUFFER_TOO_SMALL          ((NTSTATUS)0xC0000023L)
#define STATUS_ACCESS_DENIED	         ((NTSTATUS)0xC0000011L)
#define STATUS_ALREADY_COMMITTED         ((NTSTATUS)0xC0000021L)
#define STATUS_INVALID_DEVICE_REQUEST    ((NTSTATUS)0xC0000010L)

typedef struct {
	ULONGLONG			Vcn;
	ULONGLONG			Lcn;
} MAPPING_PAIR, *PMAPPING_PAIR;

typedef struct {
	ULONG				NumberOfPairs;
	ULONGLONG			StartVcn;
	MAPPING_PAIR		Pair[1];
} GET_RETRIEVAL_DESCRIPTOR, *PGET_RETRIEVAL_DESCRIPTOR;

typedef struct {
	ULONGLONG			StartLcn;
	ULONGLONG			ClustersToEndOfVol;
	BYTE				Map[1];
} BITMAP_DESCRIPTOR, *PBITMAP_DESCRIPTOR; 

typedef struct {
     HANDLE            FileHandle; 
     ULONG             Reserved;   
     ULONGLONG     StartVcn; 
     ULONGLONG     TargetLcn;
     ULONG             NumVcns; 
     ULONG             Reserved1;	
} MOVEFILE_DESCRIPTOR, *PMOVEFILE_DESCRIPTOR;

// No longer needed for XP and above
// typedef struct {
	// ULONGLONG    	SerialNumber;
	// ULONGLONG    	NumberOfSectors;
	// ULONGLONG    	TotalClusters;
	// ULONGLONG    	FreeClusters;
	// ULONGLONG    	Reserved;
	// ULONG    		BytesPerSector;
	// ULONG    		BytesPerCluster;
	// ULONG    		BytesPerMFTRecord;
	// ULONG    		ClustersPerMFTRecord;
	// ULONGLONG    	MFTLength;
	// ULONGLONG    	MFTStart;
	// ULONGLONG    	MFTMirrorStart;
	// ULONGLONG    	MFTZoneStart;
	// ULONGLONG    	MFTZoneEnd;
// } NTFS_VOLUME_DATA_BUFFER, *PNTFS_VOLUME_DATA_BUFFER;

typedef struct _IO_STATUS_BLOCK {
    NTSTATUS Status;
    ULONG Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef VOID (*PIO_APC_ROUTINE) (
				PVOID ApcContext,
				PIO_STATUS_BLOCK IoStatusBlock,
				ULONG Reserved
			);

NTSTATUS (__stdcall *NtFsControlFile)( 
					HANDLE FileHandle,
					HANDLE Event,
					PIO_APC_ROUTINE ApcRoutine,
					PVOID ApcContext,
					PIO_STATUS_BLOCK IoStatusBlock,	
					ULONG FsControlCode,
					PVOID InputBuffer,
					ULONG InputBufferLength,
					PVOID OutputBuffer,
					ULONG OutputBufferLength
			);


ULONG (__stdcall *RtlNtStatusToDosError) (
		IN NTSTATUS Status
		);