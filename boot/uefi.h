#ifndef TINY_UEFI_H
#define TINY_UEFI_H

#include <stdint.h>
#include <stddef.h>

#define EFIAPI __attribute__((ms_abi))

typedef void *EFI_HANDLE;
typedef uint64_t EFI_STATUS;
typedef uint64_t UINTN;
typedef uint64_t EFI_PHYSICAL_ADDRESS; /* <--- FIXED */
typedef uint64_t EFI_VIRTUAL_ADDRESS;

#define EFI_SUCCESS 0
#define EFI_ERROR(status) (((int64_t)(status)) < 0)

typedef struct { uint8_t Addr[32]; } EFI_MAC_ADDRESS;
typedef struct { uint8_t Addr[4]; } EFI_IPv4_ADDRESS;
typedef struct { uint8_t Addr[16]; } EFI_IPv6_ADDRESS;
typedef struct { EFI_MAC_ADDRESS M; EFI_IPv4_ADDRESS I4; EFI_IPv6_ADDRESS I6; } EFI_IP_ADDRESS;
typedef struct { uint32_t D1; uint16_t D2; uint16_t D3; uint8_t D4[8]; } EFI_GUID;
typedef struct { uint64_t Sig; uint32_t Rev; uint32_t HdrSz; uint32_t CRC; uint32_t Res; } EFI_TABLE_HEADER;

typedef struct _STO { 
    void *Reset; 
    EFI_STATUS (EFIAPI *OutputString)(struct _STO *This, uint16_t *String); 
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

/* Memory Definitions */
typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress, /* Value 2 = Force specific address */
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

typedef enum { EfiLoaderData=2 } EFI_MEMORY_TYPE;

typedef struct { 
    uint32_t Type; 
    EFI_PHYSICAL_ADDRESS PhysicalStart; 
    EFI_VIRTUAL_ADDRESS VirtualStart; 
    uint64_t NumberOfPages; 
    uint64_t Attribute; 
} EFI_MEMORY_DESCRIPTOR;

typedef struct {
    EFI_TABLE_HEADER Hdr; void *RT; void *RTP;
    EFI_STATUS (EFIAPI *AllocatePages)(EFI_ALLOCATE_TYPE, EFI_MEMORY_TYPE, UINTN, EFI_PHYSICAL_ADDRESS*);
    EFI_STATUS (EFIAPI *FreePages)(EFI_PHYSICAL_ADDRESS, UINTN);
    EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN*, EFI_MEMORY_DESCRIPTOR*, UINTN*, UINTN*, uint32_t*);
    EFI_STATUS (EFIAPI *AllocatePool)(EFI_MEMORY_TYPE, UINTN, void**);
    EFI_STATUS (EFIAPI *FreePool)(void*);
    void *CE; void *ST; void *WFE; void *SE; void *CLE; void *CHE; void *IPI; void *RPI; void *UPI;
    EFI_STATUS (EFIAPI *HandleProtocol)(EFI_HANDLE, EFI_GUID*, void**);
    void *Res; void *RPN;
    EFI_STATUS (EFIAPI *LocateHandle)(int, EFI_GUID*, void*, UINTN*, EFI_HANDLE*);
    void *LDP; void *ICT; void *LI; void *SI; void *Exit; void *UI;
    EFI_STATUS (EFIAPI *ExitBootServices)(EFI_HANDLE, UINTN);
    void *GNMC; void *Stall; void *SWT; void *CC; void *DC; void *OP; void *CP; void *OPI; void *PPH; void *LHB;
    EFI_STATUS (EFIAPI *LocateProtocol)(EFI_GUID*, void*, void**);
} EFI_BOOT_SERVICES;

typedef struct { 
    EFI_TABLE_HEADER Hdr; uint16_t *FV; uint32_t FR; EFI_HANDLE CIH; void *CI; EFI_HANDLE COH; 
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut; 
    void *SEH; void *SE; void *RS; EFI_BOOT_SERVICES *BootServices; UINTN NTE; void *CT; 
} EFI_SYSTEM_TABLE;

typedef struct { uint32_t Ver; uint32_t HR; uint32_t VR; uint32_t PF; uint32_t PIM[4]; uint32_t PPSL; } EFI_GOP_MODE_INFO;
typedef struct { uint32_t MM; uint32_t M; EFI_GOP_MODE_INFO *Info; UINTN IS; uint64_t FBB; UINTN FBS; } EFI_GOP_MODE;
typedef struct _GOP { 
    EFI_STATUS (EFIAPI *QM)(struct _GOP*, uint32_t, UINTN*, EFI_GOP_MODE_INFO**); 
    EFI_STATUS (EFIAPI *SM)(struct _GOP*, uint32_t); 
    void *Blt; EFI_GOP_MODE *Mode; 
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct _FP { 
    uint64_t R; 
    EFI_STATUS (EFIAPI *Open)(struct _FP*, struct _FP**, uint16_t*, uint64_t, uint64_t); 
    EFI_STATUS (EFIAPI *Close)(struct _FP*); 
    EFI_STATUS (EFIAPI *Delete)(struct _FP*); 
    EFI_STATUS (EFIAPI *Read)(struct _FP*, UINTN*, void*); 
} EFI_FILE_PROTOCOL;

typedef struct _SFSP { 
    uint64_t R; 
    EFI_STATUS (EFIAPI *OpenVolume)(struct _SFSP*, EFI_FILE_PROTOCOL**); 
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

#define EFI_GOP_GUID {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}}
#define EFI_SFSP_GUID {0x0964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}
#endif
