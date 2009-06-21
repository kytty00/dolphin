// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Common.h"

#include "Memmap.h"
#include "WII_IOB.h"
#include "../Core.h"
#include "../PowerPC/PowerPC.h"
#include "../Debugger/Debugger_BreakPoints.h"

namespace Memory
{

// =================================
// From Memmap.cpp
// ----------------

// Pointers to low memory
extern u8 *m_pFakeVMEM;
extern u8 *m_pEXRAM;  // Wii
extern u8 *m_pEFB;

// Init
extern bool m_IsInitialized;
extern bool bFakeVMEM;

// Read and write shortcuts

// It appears that some clever games use stfd to write 64 bits to the fifo. Hence the hwWrite64.

extern writeFn8  hwWrite8 [NUMHWMEMFUN];
extern writeFn16 hwWrite16[NUMHWMEMFUN];
extern writeFn32 hwWrite32[NUMHWMEMFUN];
extern writeFn64 hwWrite64[NUMHWMEMFUN];

extern readFn8   hwRead8 [NUMHWMEMFUN];
extern readFn16  hwRead16[NUMHWMEMFUN];
extern readFn32  hwRead32[NUMHWMEMFUN];

extern writeFn8  hwWriteWii8 [NUMHWMEMFUN];
extern writeFn16 hwWriteWii16[NUMHWMEMFUN];
extern writeFn32 hwWriteWii32[NUMHWMEMFUN];
extern writeFn64 hwWriteWii64[NUMHWMEMFUN];

extern readFn8   hwReadWii8 [NUMHWMEMFUN];
extern readFn16  hwReadWii16[NUMHWMEMFUN];
extern readFn32  hwReadWii32[NUMHWMEMFUN];

///////////////////////////////////////////////////////////////////////////////////
// Read and write
// ----------------
// The read and write macros that direct us to the right functions
// ----------------
/* Instructions: To test the TLB functions in F-Zero disable the "&& ((_Address & 0xFE000000)
   == 0x7e000000)" condition next to bFakeVMEM below. */
// ----------------

// All these little inline functions are needed because we can't paste symbols together in templates
// like we can in macros.
inline void hwRead(u8 &var, u32 addr)  {hwRead8 [(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}
inline void hwRead(u16 &var, u32 addr) {hwRead16[(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}
inline void hwRead(u32 &var, u32 addr) {hwRead32[(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}
inline void hwRead(u64 &var, u32 addr) {PanicAlert("hwRead: There's no 64-bit HW read. %08x", addr);}

inline void hwWrite(u8 var, u32 addr)  {hwWrite8[(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}
inline void hwWrite(u16 var, u32 addr) {hwWrite16[(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}
inline void hwWrite(u32 var, u32 addr) {hwWrite32[(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}
inline void hwWrite(u64 var, u32 addr) {hwWrite64[(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}

inline void hwReadWii(u8 &var, u32 addr)  {hwReadWii8 [(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}
inline void hwReadWii(u16 &var, u32 addr) {hwReadWii16[(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}
inline void hwReadWii(u32 &var, u32 addr) {hwReadWii32[(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}
inline void hwReadWii(u64 &var, u32 addr) {PanicAlert("hwReadWii: There's no 64-bit HW read. %08x", addr);}

inline void hwWriteWii(u8 var, u32 addr)  {hwWriteWii8[(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}
inline void hwWriteWii(u16 var, u32 addr) {hwWriteWii16[(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}
inline void hwWriteWii(u32 var, u32 addr) {hwWriteWii32[(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}
inline void hwWriteWii(u64 var, u32 addr) {hwWriteWii64[(addr>>HWSHIFT) & (NUMHWMEMFUN-1)](var, addr);}

inline void hwReadIOBridge(u8 &var, u32 addr)  {WII_IOBridge::Read8(var, addr);}
inline void hwReadIOBridge(u16 &var, u32 addr) {WII_IOBridge::Read16(var, addr);}
inline void hwReadIOBridge(u32 &var, u32 addr) {WII_IOBridge::Read32(var, addr);}
inline void hwReadIOBridge(u64 &var, u32 addr) {PanicAlert("hwReadIOBridge: There's no 64-bit HW read. %08x", addr);}

inline void hwWriteIOBridge(u8 var, u32 addr)  {WII_IOBridge::Write8(var, addr);}
inline void hwWriteIOBridge(u16 var, u32 addr) {WII_IOBridge::Write16(var, addr);}
inline void hwWriteIOBridge(u32 var, u32 addr) {WII_IOBridge::Write32(var, addr);}
inline void hwWriteIOBridge(u64 var, u32 addr) {PanicAlert("hwWriteIOBridge: There's no 64-bit HW write. %08x", addr);}

template <class T>
void ReadFromHardware(T &_var, u32 em_address, u32 effective_address, Memory::XCheckTLBFlag flag)
{
	// TODO: Figure out the fastest order of tests for both read and write (they are probably different).
	if ((em_address & 0xC8000000) == 0xC8000000)
		if (em_address < 0xcc000000)
			_var = bswap((*(const T*)&m_pEFB[em_address & EFB_MASK]));
		else if (em_address <= 0xcc009000)
			hwRead(_var, em_address);
		/* WIIMODE */
		else if (((em_address & 0xFF000000) == 0xCD000000) &&
			(em_address <= 0xcd009000))
			hwReadWii(_var, em_address);
		else if (((em_address & 0xFFF00000) == 0xCD800000) &&
			(em_address <= 0xCD809000))
			hwReadIOBridge(_var, em_address);
		else
		{
			/* Disabled because the debugger makes trouble with */
			/*_dbg_assert_(MEMMAP,0); */
		}
	else if (((em_address & 0xF0000000) == 0x80000000) ||
		((em_address & 0xF0000000) == 0xC0000000) ||
		((em_address & 0xF0000000) == 0x00000000))
		_var = bswap((*(const T*)&m_pRAM[em_address & RAM_MASK]));
	else if (((em_address & 0xF0000000) == 0x90000000) ||
		((em_address & 0xF0000000) == 0xD0000000) ||
		((em_address & 0xF0000000) == 0x10000000))
		_var = bswap((*(const T*)&m_pEXRAM[em_address & EXRAM_MASK]));
	else if ((em_address >= 0xE0000000) && (em_address < (0xE0000000+L1_CACHE_SIZE)))
	{
		_var = bswap((*(const T*)&m_pL1Cache[em_address & L1_CACHE_MASK]));
	}
	else if (em_address >= 0xE0000000)
		PanicAlert("READ: Invalid address: %08x", em_address);

	/* If we get this far we may be in the TLB area */
	else
	{
		/*if (bFakeVMEM && ((em_address & 0xFE000000) == 0x7e000000) )*/
		/*F-Zero:*/if (bFakeVMEM)
		{
			_var = bswap((*(const T*)&m_pFakeVMEM[em_address & FAKEVMEM_MASK]));
		}
		else {/* LOG(MEMMAP,"READ (unknown): %08x (PC: %08x)",em_address,PC);*/
			/*PanicAlert("READ: Unknown Address", "1", MB_OK);*/
			u32 TmpAddress = CheckDTLB(effective_address, flag);
			TmpAddress = (TmpAddress & 0xFFFFFFF0) | (em_address & 0xF);
			if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
				_var = bswap((*(const T*)&m_pRAM[TmpAddress & RAM_MASK]));
		}
	}
	/* Debugging: CheckForBadAddresses##_type(em_address, _var, true);*/
}


template <class T>
void WriteToHardware(u32 em_address, const T data, u32 effective_address, Memory::XCheckTLBFlag flag)
{
	/* Debugging: CheckForBadAddresses##_type(em_address, data, false);*/
	if ((em_address & 0xC8000000) == 0xC8000000)
	{
		if (em_address < 0xcc000000)
		{
			*(T*)&m_pEFB[em_address & EFB_MASK] = bswap(data);
			return;
		}
		else if (em_address <= 0xcc009000) {
			hwWrite(data, em_address);
			return;
		}
		/* WIIMODE */
		else if (((em_address & 0xFF000000) == 0xCD000000) &&
			(em_address <= 0xcd009000)) {
				hwWriteWii(data,em_address);
				return;
		}
		else if (((em_address & 0xFFF00000) == 0xCD800000) &&
			(em_address <= 0xCD809000)) {
				hwWriteIOBridge(data,em_address);
				return;
		}
		else {
			ERROR_LOG(MEMMAP, "hwwrite [%08x] := %08x (PC: %08x)", em_address, data, PC);
			_dbg_assert_msg_(MEMMAP,0,"Memory - Unknown HW address %08x", em_address);
		}
	}
	else if (((em_address & 0xF0000000) == 0x80000000) ||
		((em_address & 0xF0000000) == 0xC0000000) ||
		((em_address & 0xF0000000) == 0x00000000))
	{
		*(T*)&m_pRAM[em_address & RAM_MASK] = bswap(data);
		return;
	}
	else if (((em_address & 0xF0000000) == 0x90000000) ||
		((em_address & 0xF0000000) == 0xD0000000) ||
		((em_address & 0xF0000000) == 0x10000000))
	{
		*(T*)&m_pEXRAM[em_address & EXRAM_MASK] = bswap(data);
		return;
	}
	else if ((em_address >= 0xE0000000) && (em_address < (0xE0000000+L1_CACHE_SIZE)))
	{
		*(T*)&m_pL1Cache[em_address & L1_CACHE_MASK] = bswap(data);
		return;
	}
	else if (em_address >= 0xE0000000)
	{
		ERROR_LOG(MEMMAP,"WRITE: Cache address out of bounds (addr: %08x data: %08x)", em_address, data);
		/* PanicAlert("WRITE: Cache address %08x out of bounds", em_address); */
	}
	else
	{
		/*if (bFakeVMEM && ((em_address & 0xFE000000) == 0x7e000000))*/
		/*F-Zero: */ if (bFakeVMEM)
		{
			*(T*)&m_pFakeVMEM[em_address & FAKEVMEM_MASK] = bswap(data);
			return;
		}
		/* LOG(MEMMAP,"WRITE: %08x (PC: %08x)",em_address,PC);*/
		/*MessageBox(NULL, "WRITE: unknown Address", "1", MB_OK);*/
		u32 tmpAddress = CheckDTLB(effective_address, flag);
		tmpAddress = (tmpAddress & 0xFFFFFFF0) | (em_address & 0xF);
		*(T*)&m_pRAM[tmpAddress & RAM_MASK] = bswap(data);
	}
}
// =====================


// =================================
/* These functions are primarily called by the Interpreter functions and are routed to the correct
   location through ReadFromHardware and WriteToHardware */
// ----------------
u32 Read_Opcode(const u32 _Address)
{
	if (_Address == 0x00000000)
	{
		// FIXME use assert?
		PanicAlert("Program tried to read an opcode from [00000000]. It has crashed.");
		return 0x00000000;
	}

	u32 _var = 0;	
	ReadFromHardware<u32>(_var, _Address, _Address, FLAG_OPCODE);
	return _var;
}

u8 Read_U8(const u32 _Address)
{    
	u8 _var = 0;
	ReadFromHardware<u8>(_var, _Address, _Address, FLAG_READ);
#ifndef NOCHECK
    TMemCheck *mc = g_memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(_var, _Address,false, 1, PC);
	}
#endif
	return (u8)_var;
}

u16 Read_U16(const u32 _Address)
{
	u16 _var = 0;
	ReadFromHardware<u16>(_var, _Address, _Address, FLAG_READ);
#ifndef NOCHECK
	TMemCheck *mc = g_memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(_var, _Address,false, 2, PC);
	}
#endif
	return (u16)_var;
}

u32 Read_U32(const u32 _Address)
{
	/*#if MAX_LOGLEVEL >= 4
	if (_Address == 0x00000000)
	{
		//PanicAlert("Program tried to read from [00000000]");
		//return 0x00000000;
	}
	#endif*/
	u32 _var = 0;	
	ReadFromHardware<u32>(_var, _Address, _Address, FLAG_READ);
#ifndef NOCHECK
	TMemCheck *mc = g_memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(_var, _Address, false, 4, PC);
	}
#endif
	return _var;
}


u64 Read_U64(const u32 _Address)
{
	u64 _var = 0;
	ReadFromHardware<u64>(_var, _Address, _Address, FLAG_READ);
#ifndef NOCHECK
	TMemCheck *mc = g_memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action((u32)_var, _Address, false, 8, PC);
	}
#endif
	return _var;
}


void Write_U8(const u8 _Data, const u32 _Address)	
{
#ifndef NOCHECK
	TMemCheck *mc = g_memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(_Data,_Address,true,1,PC);
	}
#endif
	WriteToHardware<u8>(_Address, _Data, _Address, FLAG_WRITE);
}


void Write_U16(const u16 _Data, const u32 _Address)
{
#ifndef NOCHECK
	TMemCheck *mc = g_memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(_Data,_Address,true,2,PC);
	}
#endif

	WriteToHardware<u16>(_Address, _Data, _Address, FLAG_WRITE);
}


void Write_U32(const u32 _Data, const u32 _Address)
{	
#ifndef NOCHECK
	TMemCheck *mc = g_memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(_Data,_Address,true,4,PC);
	}
#endif
	WriteToHardware<u32>(_Address, _Data, _Address, FLAG_WRITE);
}


void Write_U64(const u64 _Data, const u32 _Address)
{
#ifndef NOCHECK
    TMemCheck *mc = g_memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action((u32)_Data,_Address,true,8,PC);
	}
#endif

	WriteToHardware<u64>(_Address, _Data, _Address + 4, FLAG_WRITE);
}

u8 ReadUnchecked_U8(const u32 _Address)
{    
	u8 _var = 0;
	ReadFromHardware<u8>(_var, _Address, _Address, FLAG_NO_EXCEPTION);
	return _var;
}


u32 ReadUnchecked_U32(const u32 _Address)
{
	u32 _var = 0;
	ReadFromHardware<u32>(_var, _Address, _Address, FLAG_NO_EXCEPTION);
	return _var;
}

void WriteUnchecked_U8(const u8 _iValue, const u32 _Address)
{
	WriteToHardware<u8>(_Address, _iValue, _Address, FLAG_NO_EXCEPTION);
}
 

void WriteUnchecked_U32(const u32 _iValue, const u32 _Address)
{
	WriteToHardware<u32>(_Address, _iValue, _Address, FLAG_NO_EXCEPTION);
}
// =====================

//////////////////////////////////////////////////////////




// *********************************************************************************
// Warning: Test Area
//
// This code is for TESTING and it works in interpreter mode ONLY. Some games (like 
// COD iirc) work thanks to this basic TLB emulation.
// It is just a small hack and we have never spend enough time to finalize it.
// Cheers PearPC!
//
// *********************************************************************************

/*
*	PearPC
*	ppc_mmu.cc
*
*	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
*
*	This program is free software; you can redistribute it and/or modify
*	it under the terms of the GNU General Public License version 2 as
*	published by the Free Software Foundation.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with this program; if not, write to the Free Software
*	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#define PPC_EXC_DSISR_PAGE (1<<30)
#define PPC_EXC_DSISR_PROT (1<<27)
#define PPC_EXC_DSISR_STORE (1<<25) 

#define SDR1_HTABORG(v) (((v)>>16)&0xffff)
#define SDR1_HTABMASK(v) ((v)&0x1ff)
#define SDR1_PAGETABLE_BASE(v) ((v)&0xffff) 
#define SR_T  (1<<31)
#define SR_Ks (1<<30)
#define SR_Kp (1<<29)
#define SR_N  (1<<28)
#define SR_VSID(v)       ((v)&0xffffff)
#define SR_BUID(v)       (((v)>>20)&0x1ff)
#define SR_CNTRL_SPEC(v) ((v)&0xfffff) 

#define EA_SR(v)         (((v)>>28)&0xf) 
#define EA_PageIndex(v)  (((v)>>12)&0xffff) 
#define EA_Offset(v)	((v)&0xfff)
#define EA_API(v)		(((v)>>22)&0x3f) 

#define PA_RPN(v)        (((v)>>12)&0xfffff)
#define PA_Offset(v)     ((v)&0xfff) 

#define PTE1_V       (1<<31)
#define PTE1_VSID(v) (((v)>>7)&0xffffff)
#define PTE1_H       (1<<6)
#define PTE1_API(v)  ((v)&0x3f)

#define PTE2_RPN(v)  ((v)&0xfffff000)
#define PTE2_R       (1<<8)
#define PTE2_C       (1<<7)
#define PTE2_WIMG(v) (((v)>>3)&0xf)
#define PTE2_PP(v)   ((v)&3)

union UPTE1
{
	struct 
	{
		unsigned API    : 6;
		unsigned H      : 1;
		unsigned VSID   : 24;
		unsigned V      : 1;        
	};
	u32 Hex;
};

union UPTE2
{
	struct 
	{
		unsigned PP     : 2;
		unsigned        : 1;
		unsigned WIMG   : 4;
		unsigned C      : 1;
		unsigned R      : 1;
		unsigned        : 3;
		unsigned RPN    : 20;
	};
	u32 Hex;
};

u32 pagetable_base = 0;
u32 pagetable_hashmask = 0;

void GenerateDSIException(u32 _EffectiveAdress, bool _bWrite)
{
	if (_bWrite)
		PowerPC::ppcState.spr[SPR_DSISR] = PPC_EXC_DSISR_PAGE | PPC_EXC_DSISR_STORE;
	else
		PowerPC::ppcState.spr[SPR_DSISR] = PPC_EXC_DSISR_PAGE;

	PowerPC::ppcState.spr[SPR_DAR] = _EffectiveAdress;

	INFO_LOG(MEMMAP, "Generate DSI Exception 0x%08x", _EffectiveAdress);
	PowerPC::ppcState.Exceptions |= EXCEPTION_DSI;
}


void GenerateISIException()
{
	// shuffle2: ISI exception doesn't modify DSISR at all, to my knowledge...
	//PowerPC::ppcState.spr[SPR_DSISR] = 0x4000000; // maybe this was a typo for PPC_EXC_DSISR_PAGE?

	// Instead, it modifies bits 1-4 in SRR1 depending on conditions:
	// Bit 1: set if the translation of an attempted access is not found in the primary hash table entry group
	//	(HTEG), or in the rehashed secondary HTEG, or in the range of a IBAT register (page fault
	//	condition); otherwise cleared.
	// Bit 2: cleared
	// Bit 3: Set if the fetch access occurs to a direct-store segment (SR[T] = 1), to a noexecute
	//	segment (N bit set in segment descriptor), or to guarded memory
	//	when MSR[IR] = 1. Otherwise, cleared.
	// Bit 4: Set if a memory access is not permitted by the page or IBAT protection
	//	mechanism, described in Chapter 7, �Memory Management�; otherwise cleared.
	// Only one of 1,3, or 4 may be set at a time

	// For now let's just say that hash lookup failed
	SRR1 = 0x10000000;
	INFO_LOG(MEMMAP, "Generate ISI Exception");
	PowerPC::ppcState.Exceptions |= EXCEPTION_ISI;
}


void SDRUpdated()
{
	u32 htabmask = SDR1_HTABMASK(PowerPC::ppcState.spr[SPR_SDR]);
	u32 x = 1;
	u32 xx = 0;
	int n = 0;
	while ((htabmask & x) && (n < 9)) 
	{
		n++;
		xx|=x;
		x<<=1;
	}
	if (htabmask & ~xx) 
	{
		return;
	}
	u32 htaborg = SDR1_HTABORG(PowerPC::ppcState.spr[SPR_SDR]);
	if (htaborg & xx) 
	{
		return;
	}
	pagetable_base = htaborg<<16;
	pagetable_hashmask = ((xx<<10)|0x3ff);
} 


u32 CheckDTLB(u32 _Address, XCheckTLBFlag _Flag)
{
	// Don't show the warnings if we are testing this code
	if(!bFakeVMEM)
	{
		if (Core::GetStartupParameter().bWii) {
			// TLB is never used on Wii (except linux and stuff, but we don't care about that)
			PanicAlert("%s invalid memory region (0x%08x)\n\n"
				"There is no way to recover from this error,"
				"so Dolphin will now exit. Sorry!",
				_Flag == FLAG_WRITE ? "Write to" : "Read from", _Address);
		}
		else {
			PanicAlert("%s invalid memory region (0x%08x)\n\n"
				"This is either the game crashing randomly, or a TLB write."
				"Several games uses the TLB to map memory. This\n"
				"function is not supported in Dolphin. "
				"Unfortunately there is no way to recover from this error,"
				"so Dolphin will now exit abruptly. Sorry!",
				 _Flag == FLAG_WRITE ? "Write to" : "Read from", _Address);
		}
		exit(0);
	}

	u32 sr = PowerPC::ppcState.sr[EA_SR(_Address)]; 

	u32 offset = EA_Offset(_Address);			// 12 bit 
	u32 page_index = EA_PageIndex(_Address);	// 16 bit
	u32 VSID = SR_VSID(sr);						// 24 bit 
	u32 api = EA_API(_Address);					//  6 bit (part of page_index) 

	u8* pRAM = GetPointer(0);

	// hash function no 1 "xor" .360
	u32 hash1 = (VSID ^ page_index);
	u32 pteg_addr = ((hash1 & pagetable_hashmask)<<6) | pagetable_base; 

	// hash1
	for (int i = 0; i < 8; i++) 
	{ 
		UPTE1 PTE1;
		PTE1.Hex = bswap(*(u32*)&pRAM[pteg_addr]);

		if (PTE1.V && !PTE1.H) 
		{
			if (VSID == PTE1.VSID && (api == PTE1.API)) 
			{
				UPTE2 PTE2;
				PTE2.Hex = bswap((*(u32*)&pRAM[(pteg_addr + 4)]));

				// set the access bits
				switch (_Flag)
				{
				case FLAG_READ:     PTE2.R = 1; break;
				case FLAG_WRITE:    PTE2.C = 1; break;
				case FLAG_NO_EXCEPTION: break;
				case FLAG_OPCODE: break;
				}              
				*(u32*)&pRAM[(pteg_addr + 4)] = bswap(PTE2.Hex);

				return ((PTE2.RPN << 12) | offset);
			}
		}
		pteg_addr+=8; 
	}

	// hash function no 2 "not" .360
	hash1 = ~hash1;
	pteg_addr = ((hash1 & pagetable_hashmask)<<6) | pagetable_base;
	for (int i = 0; i < 8; i++) 
	{ 
		u32 pte = bswap(*(u32*)&pRAM[pteg_addr]);
		if ((pte & PTE1_V) && (pte & PTE1_H)) 
		{
			if (VSID == PTE1_VSID(pte) && (api == PTE1_API(pte))) 
			{
				PanicAlert("TLB: Address found at the second hash function.\n"
						   "i have never seen this before");

				pte = bswap(*(u32*)&pRAM[(pteg_addr+4)]);

				u32 physAddress = PTE2_RPN(pte) | offset; 

				// missing access bits
				return physAddress;
			}
		}
		pteg_addr+=8; 
	}


	// If we got this far something went wrong and we save the exception data
	switch(_Flag)
	{
	case FLAG_NO_EXCEPTION:
		break;

	case FLAG_READ:
		GenerateDSIException(_Address, false);
		break;

	case FLAG_WRITE:
		GenerateDSIException(_Address, true);
		break;

	case FLAG_OPCODE:
		GenerateISIException();
		break;
	}

	return 0;
}
// ***********************


} // namespace
