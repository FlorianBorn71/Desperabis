/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "cpu.h"
#include "lazyflags.h"
#include "callback.h"
#include "pic.h"
#include "fpu.h"
#include "paging.h"
#include "mmx.h"

bool CPU_RDMSR();
bool CPU_WRMSR();
bool CPU_SYSENTER();
bool CPU_SYSEXIT();

#define PRE_EXCEPTION { }

#define CPU_CORE CPU_ARCHTYPE_386

#define DoString DoString_Normal

extern bool ignore_opcode_63;

#if C_DEBUG
#include "debug.h"
#endif

#if (!C_CORE_INLINE)
#define LoadMb(off) mem_readb(off)
#define LoadMw(off) mem_readw(off)
#define LoadMd(off) mem_readd(off)
#define LoadMq(off) ((uint64_t)((uint64_t)mem_readd(off+4)<<32 | (uint64_t)mem_readd(off)))
#define SaveMb(off,val)	mem_writeb(off,val)
#define SaveMw(off,val)	mem_writew(off,val)
#define SaveMd(off,val)	mem_writed(off,val)
#define SaveMq(off,val) {mem_writed(off,val&0xffffffff);mem_writed(off+4,(val>>32)&0xffffffff);}
#else 
#include "paging.h"
#define LoadMb(off) mem_readb_inline(off)
#define LoadMw(off) mem_readw_inline(off)
#define LoadMd(off) mem_readd_inline(off)
#define LoadMq(off) ((uint64_t)((uint64_t)mem_readd_inline(off+4)<<32 | (uint64_t)mem_readd_inline(off)))
#define SaveMb(off,val)	mem_writeb_inline(off,val)
#define SaveMw(off,val)	mem_writew_inline(off,val)
#define SaveMd(off,val)	mem_writed_inline(off,val)
#define SaveMq(off,val) {mem_writed_inline(off,val&0xffffffff);mem_writed_inline(off+4,(val>>32)&0xffffffff);}
#endif

Bitu cycle_count;

#if C_FPU
#define CPU_FPU	1u						//Enable FPU escape instructions
#endif

#define CPU_PIC_CHECK 1u
#define CPU_TRAP_CHECK 1u

#define CPU_TRAP_DECODER	CPU_Core_Normal_Trap_Run

#define OPCODE_NONE			0x000u
#define OPCODE_0F			0x100u
#define OPCODE_SIZE			0x200u

#define PREFIX_ADDR			0x1u
#define PREFIX_REP			0x2u

#define TEST_PREFIX_ADDR	(core.prefixes & PREFIX_ADDR)
#define TEST_PREFIX_REP		(core.prefixes & PREFIX_REP)

#define DO_PREFIX_SEG(_SEG)					\
	BaseDS=SegBase(_SEG);					\
	BaseSS=SegBase(_SEG);					\
	core.base_val_ds=_SEG;					\
	goto restart_opcode;

#define DO_PREFIX_ADDR()								\
	core.prefixes=(core.prefixes & ~PREFIX_ADDR) |		\
	(cpu.code.big ^ PREFIX_ADDR);						\
	core.ea_table=&EATable[(core.prefixes&1u) * 256u];	\
	goto restart_opcode;

#define DO_PREFIX_REP(_ZERO)				\
	core.prefixes|=PREFIX_REP;				\
	core.rep_zero=_ZERO;					\
	goto restart_opcode;

#define REMEMBER_PREFIX(_x) last_prefix = (_x)

static uint8_t last_prefix;

typedef PhysPt (*GetEAHandler)(void);

static const uint32_t AddrMaskTable[2]={0x0000ffffu,0xffffffffu};

static struct {
	Bitu opcode_index;
	PhysPt cseip;
	PhysPt base_ds,base_ss;
	SegNames base_val_ds;
	bool rep_zero;
	Bitu prefixes;
	GetEAHandler * ea_table;
} core;

/* FIXME: Someone at Microsoft tell how subtracting PhysPt - PhysPt = __int64, or PhysPt + PhysPt = __int64 */
#define GETIP		((PhysPt)(core.cseip-SegBase(cs)))
#define SAVEIP		reg_eip=GETIP;
#define LOADIP		core.cseip=((PhysPt)(SegBase(cs)+reg_eip));

#define SegBase(c)	SegPhys(c)
#define BaseDS		core.base_ds
#define BaseSS		core.base_ss

static INLINE void FetchDiscardb() {
	core.cseip+=1;
}

static INLINE uint8_t FetchPeekb() {
	uint8_t temp=LoadMb(core.cseip);
	return temp;
}

static INLINE uint8_t Fetchb() {
	uint8_t temp=LoadMb(core.cseip);
	core.cseip+=1;
	return temp;
}

static INLINE uint16_t Fetchw() {
	uint16_t temp=LoadMw(core.cseip);
	core.cseip+=2;
	return temp;
}
static INLINE uint32_t Fetchd() {
	uint32_t temp=LoadMd(core.cseip);
	core.cseip+=4;
	return temp;
}

#define Push_16 CPU_Push16
#define Push_32 CPU_Push32
#define Pop_16 CPU_Pop16
#define Pop_32 CPU_Pop32

#include "instructions.h"
#include "core_normal/support.h"
#include "core_normal/string.h"


int FindSequence(const uint8_t* data, int dataLen, const uint8_t* seq, int seqLen)
{
    for(int i = 0; i < dataLen - seqLen; i++, data++)
    {
        if(memcmp(data, seq, seqLen) == 0)
        {
            return i;
        }
    }
    return -1;
}

struct RawGameObject
{
    union
    {
        uint8_t m_rawBytes[54];
        int16_t m_rawShorts[27];
        uint16_t m_rawUShorts[27];
    };
};


struct SharedLevelMem
{
    RawGameObject m_objects[140];
};


#include "dos_inc.h"
#include "aclapi.h"

SharedLevelMem* g_sharedMemory = nullptr;
HANDLE g_hMapFile = nullptr;

SharedLevelMem* CreateSharedMemory()
{
    if (g_sharedMemory == nullptr)
    {
        int size = sizeof(SharedLevelMem);
        static bool create = true;
        DWORD error = 0;

        if(create)
        {
            g_hMapFile = CreateFileMappingW(
                INVALID_HANDLE_VALUE,    // use paging file
                nullptr,                 // default security
                PAGE_READWRITE,          // read/write access
                0,                       // maximum object size (high-order DWORD)
                size,                    // maximum object size (low-order DWORD)
                L"DesperabisGameplay");                 // name of mapping object
        }
        else
        {
            g_hMapFile = OpenFileMappingW(
                FILE_MAP_ALL_ACCESS,   // read/write access
                FALSE,                 // do not inherit the name
                L"DesperabisGameplay");

        }

        if(g_hMapFile == nullptr)
        {
            error = GetLastError();
            return nullptr;
        }
        g_sharedMemory = (SharedLevelMem*)MapViewOfFile(g_hMapFile,   // handle to map object
            FILE_MAP_ALL_ACCESS, // read/write permission
            0,
            0,
            size);

        if(g_sharedMemory == nullptr)
        {
            error = GetLastError();
            CloseHandle(g_hMapFile);
            return nullptr;
        }

        /*
        uint8_t* sharedMem = (uint8_t*)g_sharedMemory;
        for(int i = 0; i < 255; i++)
        {
            sharedMem[i] = i;
        }
        UnmapViewOfFile(g_sharedMemory);
        */
    }
    return g_sharedMemory;
}

void OnGameObjectsUpdated()
{
    static uint8_t data[64 * 1024];
    PhysPt physAddr = BaseDS;
    HostPt addr = get_tlb_read(physAddr) + physAddr;

    //static bool test = false;

    RawGameObject* gameObjectStart = (RawGameObject *)&addr[0x82a8];

    if(SharedLevelMem* sharedMem = CreateSharedMemory())
    {
        memcpy(sharedMem->m_objects, gameObjectStart, sizeof(sharedMem->m_objects));

        //for(int i = 0; i < 255; i++)
        //{
        //    sharedMem[i] = i;
        //}
    }

    /*
    if(test)
    {
        memcpy(data, addr, sizeof(data));
        //const uint8_t gameobjSeq[] = { 37,1,0,1,0,0,0,0,0,0,232,3,158,7,119,255,149,8,0,0,0,0,0,0,20,0,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,86,6,0,4,0,64 }; // object 0 in Museum level
        const uint8_t gameobjSeq[] = { 37,1,0,1 }; // object 0 in Museum level

        int found = FindSequence(data, sizeof(data), gameobjSeq, sizeof(gameobjSeq));
        if(found >= 0)
        {
            test = false;
        }
    }
    */
}

#define EALookupTable (core.ea_table)

Bits CPU_Core_Normal_Run(void) {
    if (CPU_Cycles <= 0)
	    return CBRET_NONE;

	while (CPU_Cycles-->0) {
		LOADIP;
		last_prefix=MP_NONE;
		core.opcode_index=cpu.code.big*(Bitu)0x200u;
		core.prefixes=cpu.code.big;
		core.ea_table=&EATable[cpu.code.big*256u];
		BaseDS=SegBase(ds);
		BaseSS=SegBase(ss);
		core.base_val_ds=ds;
#if C_DEBUG
#if C_HEAVY_DEBUG
		if (DEBUG_HeavyIsBreakpoint()) {
			FillFlags();
			return (Bits)debugCallback;
		}
#endif
#endif
		cycle_count++;
    restart_opcode:
        PhysPt currentCs = core.cseip;
        //static uint32_t csmin = 0xffffffff;
        //static uint32_t csmax = 0x00000000;
//        csmin = __min(csmin, currentCs);
//        csmax = __max(csmax, currentCs);


        /*
        //static uint8_t sequence[] = { 0xc8, 0xc6, 0xc7, 0xeb }; // inside game loop
        //static uint8_t sequence[] = { 0x31, 0xa3, 0xa3, 0x6a, 0xe8 }; // right before calling GameCode in GameTick
        static uint8_t sequence[] = { 0xff, 0xff, 0xff, 0xff, 0xa0, 0x30, 0x40, 0x50, 0x9a }; // right before calling GameCode in GameTick2
        static int sequenceIndex = 0;


        if (currentCs == 0x000169a4) // wrong
        {
            printf("Before calling Game Code\n");
        }
        else if(currentCs == 0x000169a7)
        {
            printf("After calling Game Code\n");
        }
        */
//        RealPt realP = PhysToReal416(currentCs);
        auto opcodeX = core.opcode_index + Fetchb();
        static bool insideGameCode = false;

        if(insideGameCode)
        {
            if(opcodeX == 0xc3) // exit
            {
                OnGameObjectsUpdated();
                insideGameCode = false;
            }
        }
        else if(currentCs == 0x000092c4) // 0x000092c4 is first instruction of DESP_GamePlay
        {
            //assert(opcodeX == 0xc8);
            insideGameCode = true;
        }


        /*
        if(opcodeX == sequence[sequenceIndex])
        {
            sequenceIndex++;
            if(sequenceIndex == (sizeof(sequence)/sizeof(sequence[0])))
            {
                sequenceIndex = 0; // found!
                printf("Sequence found at %x\n", currentCs);
            }
        }
        else
        {
            sequenceIndex = 0;
        }
        static bool logging = false;
        if(logging)
        {
            printf("CS:%d , %x\n", currentCs, opcodeX);
        }
        */


        switch(opcodeX) {
		#include "core_normal/prefix_none.h"
		#include "core_normal/prefix_0f.h"
		#include "core_normal/prefix_66.h"
		#include "core_normal/prefix_66_0f.h"
		default:
		illegal_opcode:
#if C_DEBUG	
			{
				bool ignore=false;
				Bitu len=(GETIP-reg_eip);
				LOADIP;
				if (len>16) len=16;
				char tempcode[16*2+1];char * writecode=tempcode;
				if (ignore_opcode_63 && mem_readb(core.cseip) == 0x63)
					ignore = true;
				for (;len>0;len--) {
					sprintf(writecode,"%02X",mem_readb(core.cseip++));
					writecode+=2;
				}
				if (!ignore)
					LOG(LOG_CPU,LOG_NORMAL)("Illegal/Unhandled opcode %s",tempcode);
			}
#endif
			CPU_Exception(6,0);
			continue;
		gp_fault:
			LOG_MSG("Segment limit violation");
			CPU_Exception(EXCEPTION_GP,0);
			continue;
		}
		SAVEIP;
	}
	FillFlags();
	return CBRET_NONE;
decode_end:
	SAVEIP;
	FillFlags();
	return CBRET_NONE;
}

Bits CPU_Core_Normal_Trap_Run(void) {
	Bits oldCycles = CPU_Cycles;
	CPU_Cycles = 1;
	cpu.trap_skip = false;

	Bits ret=CPU_Core_Normal_Run();
	if (!cpu.trap_skip) CPU_DebugException(DBINT_STEP,reg_eip);
	CPU_Cycles = oldCycles-1;
	cpudecoder = &CPU_Core_Normal_Run;

	return ret;
}



void CPU_Core_Normal_Init(void) {

}

