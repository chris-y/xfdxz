unsigned char version[] = "$VER: XZ 1.1 (06.09.2014)";

/* Objectheader

	Name:		xz.c
	Description:	xfd external decruncher for XZ files
	Author: 	Chris Young <chris@unsatisfactorysoftware.co.uk>
	Distribution:	
*/

#include <libraries/xfdmaster.h>
#include <proto/exec.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#ifdef __amigaos4__
#include <proto/xfdmaster.h>
#endif
#include "SDI_compiler.h"

#ifdef __amigaos4__
struct ExecIFace *IExec;
struct Library *newlibbase;
struct Interface *INewlib;
struct lzmaIFace *Ilzma;

#include "sys/types.h"
#else
#include <exec/types.h>
#endif
#include <proto/lzma.h>

struct Library *lzmaBase;

/************** Here starts xfd stuff - the xfdSlave structure **********/

#ifdef __amigaos4__
#define MASTER_VERS	53
#else
#define MASTER_VERS	39
#endif

typedef BOOL (*xfdFunc) ();

#ifdef __amigaos4__
BOOL RecogXZ(struct xfdMasterIFace *IxfdMaster, UBYTE *b, ULONG length, 
    struct xfdRecogResult *rr);
BOOL DecrunchXZ(struct xfdMasterIFace *IxfdMaster, struct xfdBufferInfo *xbi);
BOOL ScanXZ(struct xfdMasterIFace *IxfdMaster, UBYTE *b, ULONG length);
ULONG VerifyXZ(struct xfdMasterIFace *IxfdMaster, UBYTE *b, ULONG length);
#else
ASM(LONG) RecogXZ(REG(a0, ULONG * b), REG(a1, struct xfdRecogResult *rr),
 REG(d0, ULONG length));
ASM(ULONG) DecrunchXZ(REG(a0, struct xfdBufferInfo * xbi),
 REG(a6, struct xfdMasterBase *xfdMasterBase));
ASM(LONG) ScanXZ(REG(a0, ULONG *b), REG(d0, ULONG length));
ASM(ULONG) VerifyXZ(REG(a0, ULONG *b), REG(d0, ULONG length));
#endif

struct xfdSlave FirstSlave = {
  0, XFDS_VERSION, MASTER_VERS, "XZ", XFDPFF_DATA|XFDPFF_USERTARGET, 0, 
  (xfdFunc) RecogXZ, (xfdFunc) DecrunchXZ, (xfdFunc) ScanXZ, NULL, 0, 0, 12};

void close_lzma(void)
{
#ifdef __amigaos4__
	if(Ilzma)
	{
		DropInterface((struct Interface *)Ilzma);
		Ilzma=NULL;
	}
#endif
	if(lzmaBase)
	{
		CloseLibrary(lzmaBase);
		lzmaBase=NULL;
	}

#ifdef __amigaos4__
/*
    DropInterface(INewlib);
    CloseLibrary(newlibbase);
	INewlib = NULL;
	newlibbase = NULL;
*/
#endif
}

BOOL open_lzma(void)
{
#ifdef __amigaos4__
    IExec = (struct ExecIFace *)(*(struct ExecBase **)4)->MainInterface;

    newlibbase = OpenLibrary("newlib.library", 52);
    if(newlibbase)
        INewlib = GetInterface(newlibbase, "main", 1, NULL);
#endif

	if(lzmaBase = OpenLibrary("lzma.library",5))
	{
#ifdef __amigaos4__
		if(Ilzma = (struct lzmaIFace *)GetInterface(lzmaBase,"main",1,NULL))
		{
#endif
			return TRUE;
#ifdef __amigaos4__
		}
#endif
	}
	close_lzma();
	return FALSE;
}


#ifdef __amigaos4__
BOOL RecogXZ(struct xfdMasterIFace *IxfdMaster, UBYTE *b, ULONG length, 
    struct xfdRecogResult *rr)
#else
ASM(LONG) RecogXZ(REG(a0, UBYTE *b), REG(a1, struct xfdRecogResult *rr),
REG(d0, ULONG length))
#endif
{
  if(b[0]==0xfd & b[1]=='7' & b[2]=='z' & b[3]=='X' & b[4]=='Z' & b[5]==0x00)
    return TRUE; /* known file */
  else
    return FALSE; /* unknown file */
}

#ifdef __amigaos4__
BOOL DecrunchLZMA(struct xfdMasterIFace *IxfdMaster, struct xfdBufferInfo *xbi)
#else
ASM(BOOL) DecrunchLZMA(REG(a0, struct xfdBufferInfo * xbi),
REG(a6, struct xfdMasterBase *xfdMasterBase))
#endif
{
/**** THIS HASN'T BEEN WRITTEN, NB USERTARGETBUF LIKELY NOT POSSIBLE */
  STRPTR outbuf;
  ULONG flags;
  BOOL res = 0;
  struct ExecBase *SysBase;
  CLzmaDecoderState state;
	int i,waitEOS = 1;
  UInt32 outSize = 0;
  UBYTE inbuf[LZMA_PROPERTIES_SIZE]; /* = (UBYTE *)xbi->xfdbi_SourceBuffer; */
  SizeT inProcessed,outProcessed;
	SizeT lzmainsize= (SizeT)xbi->xfdbi_SourceBufLen;
	UBYTE *lzmainbuf = (UBYTE *)xbi->xfdbi_SourceBuffer;

	lzmainbuf+=13;
	lzmainsize-=13;

#ifdef __amigaos4__
	struct xfdMasterBase *xfdMasterBase = (struct xfdMasterBase *)IxfdMaster->Data.LibBase;
	struct ExecIFace *IExec = xfdMasterBase->xfdm_IExec;
#else
  SysBase = xfdMasterBase->xfdm_ExecBase;
  /* We need OS2.0 for AllocVec */
  if(SysBase->LibNode.lib_Version < 36)
  {
    xbi->xfdbi_Error = XFDERR_MISSINGRESOURCE;
    return 0;
  }
#endif

	CopyMem(xbi->xfdbi_SourceBuffer,&inbuf,LZMA_PROPERTIES_SIZE);

  if (LzmaDecodeProperties(&state.Properties,&inbuf, LZMA_PROPERTIES_SIZE) != LZMA_RESULT_OK)
	{
		xbi->xfdbi_Error = XFDERR_CORRUPTEDDATA;
	 return 0; /* error */
	}

	state.Probs = (CProb *)AllocVec(LzmaGetNumProbs(&state.Properties) * sizeof(CProb),MEMF_CLEAR);
	if (state.Probs == 0)
	{
	    xbi->xfdbi_Error = XFDERR_NOMEMORY;
		return 0;
	}

/*
    if((xbi->xfdbi_TargetBuffer = (UBYTE *) AllocMem(xbi->xfdbi_FinalTargetLen, xbi->xfdbi_TargetBufMemType)))
	{
*/

  if(LzmaDecode(&state, 
      lzmainbuf, lzmainsize, &inProcessed,
      xbi->xfdbi_UserTargetBuf,(SizeT)xbi->xfdbi_TargetBufSaveLen, &outProcessed) == LZMA_RESULT_OK)
		{
	        xbi->xfdbi_TargetBufLen = xbi->xfdbi_FinalTargetLen = xbi->xfdbi_TargetBufSaveLen;
    	    xbi->xfdbi_TargetBufSaveLen = outProcessed;
			FreeVec(state.Probs);
			xbi->xfdbi_Error = XFDERR_OK;
			return(TRUE);
		}
		else
		{
		    xbi->xfdbi_Error = XFDERR_BUFFERTRUNCATED;
		}
/*
	}
	else
	{
	    xbi->xfdbi_Error = XFDERR_NOMEMORY;
	}
*/

FreeVec(state.Probs);
return(0);
}

#ifdef __amigaos4__
BOOL ScanLZMA(struct xfdMasterIFace *IxfdMaster, UBYTE *b, ULONG length)
#else
ASM(LONG) ScanLZMA(REG(a0, ULONG *b), REG(d0, ULONG length))
#endif
{
  if(b[0]==0xfd & b[1]=='7' & b[2]=='z' & b[3]=='X' & b[4]=='Z' & b[5]==0x00)
    return TRUE; /* known file */
  else
    return FALSE; /* unknown file */
}

#ifdef __amigaos4__
ULONG VerifyLZMA(struct xfdMasterIFace *IxfdMaster, UBYTE *b, ULONG length)
#else
ASM(ULONG) VerifyLZMA(REG(a0, ULONG *b), REG(d0, ULONG length))
#endif
{
	return 0;
}
