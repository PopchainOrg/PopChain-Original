// LoopBuf.cpp: implementation of the CLoopBuf class.
//
//////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2018 The Popchain Core Development Team

#ifndef  WIN32 
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#else

#endif

#include "loopbuf.h"
#include <stdio.h>
#include <stdlib.h>

#ifndef  g_SendLog
#define g_SendLog   printf
#endif

#define INTERVAL_SPACE  16   //尾到头之间隔16个字节
#define LEN_INT         4
#define  SET_SYNCWORD(p)    memcpy((p),"@@@@",4);
#define  CHECK_SYNCWORD(p)  {if(memcmp((p),"@@@@",4)!=0){assert(0);}}
#define  RUN_CHECK_SYNCWORD(p)  {if(memcmp((p),"@@@@",4)!=0){m_pmap->nHeadPos = 0;\
		m_pmap->nTailPos = 0;m_pmap->nSize = m_nBufSize;\
		m_pmap->nQueueNo =1;;g_SendLog("loop buf run error \n\n");return 0;}}


#define LOOP_SET_PKT_HEAD(pos,len) {memcpy(pos,&len,LEN_INT);SET_SYNCWORD(pos+LEN_INT) }

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
// Popchain DevTeam add
#define  IS_PUSH_SIZE_ENOUGH(nDataLen,bEnough)   \
{\
	bEnough = false;\
	if(nHeadPos <=	m_pmap->nTailPos)\
	{\
		if(m_nBufSize -  m_pmap->nTailPos >= nDataLen)\
			bEnough= true;\
		else\
		{\
			m_nLeftSize = nDataLen - (m_nBufSize -  m_pmap->nTailPos);\
			if(nHeadPos - INTERVAL_SPACE >= m_nLeftSize) \
				bEnough= true;\
		}\
	}\
	else if(nHeadPos - m_pmap->nTailPos >nDataLen+INTERVAL_SPACE )\
	{\
		bEnough= true;\
	};\
}

#define  PUSH_DATA(lpData,nDataLen)   \
{\
	if(nHeadPos <=	m_pmap->nTailPos)\
	{\
		if(m_nBufSize -  m_pmap->nTailPos >= nDataLen)\
		{\
			memcpy(m_pShmBuf + m_pmap->nTailPos,lpData, nDataLen  );\
			m_pmap->nTailPos =  m_pmap->nTailPos + nDataLen;\
		}\
		else\
		{\
			nTempSize = m_nBufSize -  m_pmap->nTailPos;\
			m_nLeftSize = nDataLen - nTempSize;\
			memcpy(m_pShmBuf+m_pmap->nTailPos,lpData, nTempSize);\
			memcpy(m_pShmBuf, lpData+nTempSize, m_nLeftSize );\
			m_pmap->nTailPos = m_nLeftSize;\
		}\
	}\
	else\
	{\
		memcpy(m_pShmBuf + m_pmap->nTailPos,lpData, nDataLen  );\
		m_pmap->nTailPos =  m_pmap->nTailPos + nDataLen;\
	}\
}

#define IS_POP_SIZE_ENOUGH()  \
{\
	bEnough = false;nFullDataSize =0;\
	if(m_pmap->nHeadPos <= nTailPos)\
	{\
		if( m_pmap->nHeadPos+LEN_INT <= nTailPos)\
		{\
			memcpy(&nFullDataSize, m_pShmBuf + m_pmap->nHeadPos , LEN_INT);\
			if(nTailPos - m_pmap->nHeadPos >= nFullDataSize)\
			{\
				bEnough = true;\
			}\
		}\
	}\
	else\
	{\
		int nTempLen;\
		if(m_nBufSize -  m_pmap->nHeadPos >=LEN_INT )\
			memcpy(&nFullDataSize ,m_pShmBuf+m_pmap->nHeadPos,4);\
		else  if((m_nBufSize - m_pmap->nHeadPos + nTailPos)>= LEN_INT )\
		{\
			BYTE * pDataSize = (BYTE *)(&nFullDataSize);\
			nTempLen = m_nBufSize - m_pmap->nHeadPos;\
			memcpy(pDataSize, m_pShmBuf+m_pmap->nHeadPos ,nTempLen);\
			memcpy(pDataSize +nTempLen, m_pShmBuf , LEN_INT-nTempLen );\
		}\
		if( nFullDataSize> 0){\
			if(m_nBufSize - m_pmap->nHeadPos + nTailPos >= nFullDataSize)\
				bEnough = true;\
		}\
	}\
}

#define POP_DATA(lpData,nDataSize)  \
{\
	if(m_pmap->nHeadPos <= nTailPos)\
	{\
		memcpy(lpData, m_pShmBuf + m_pmap->nHeadPos , nDataSize);\
		m_pmap->nHeadPos = m_pmap->nHeadPos + nDataSize ;\
	}\
	else\
	{\
		if(m_nBufSize -  m_pmap->nHeadPos >= nDataSize)\
		{\
			memcpy(lpData, m_pShmBuf + m_pmap->nHeadPos, nDataSize);\
			m_pmap->nHeadPos = m_pmap->nHeadPos +nDataSize ;\
		}\
		else\
		{\
			int nTempSize = m_nBufSize - m_pmap->nHeadPos;\
			memcpy(lpData, m_pShmBuf + m_pmap->nHeadPos ,nTempSize);\
			m_pmap->nHeadPos = m_nBufSize;\
			int nLeftSize = nDataSize - nTempSize;\
			memcpy(lpData + nTempSize , m_pShmBuf ,nLeftSize);\
			m_pmap->nHeadPos = nLeftSize;\
		}\
	}\
}

CLoopBuf::CLoopBuf()
{
	m_pmap = NULL;
	m_pShmBuf = NULL;
	m_dwLogPushSucc = 0;
    m_dwLogPushZero = 0;

	m_dwLogPopSucc = 0;
    m_dwLogPopZero = 0;
	LOOP_PKT_HEAD_LEN = sizeof(m_PktHead);
	m_nMemMapMode = NO_MEM_MODE;
}

CLoopBuf::~CLoopBuf()
{
	if(m_nMemMapMode ==NO_MEM_MODE)
		return ;
	if(m_nMemMapMode== LOOP_SHMMEM_MODE)
		CloseShmMap(m_nBufSize+sizeof(MemQueueHeader));
	else if(m_nMemMapMode== LOOP_MEM_MODE)
		CloseMem();
	else
		CloseShm();
}
// 直接引用共享内存的缓冲
bool CLoopBuf::Init(int nSize, char * shmname,int nMemMapMode,bool bSetZero )
{
	//assert(nSize >0);
	//多分配一个m_nItemSize 当环形缓存区 m_pmap->nHeadPos -  m_pmap->nTailPos <= 16
	//不在向里面填充数据
	if(nSize<INTERVAL_SPACE+sizeof(MemQueueHeader))
		return false;		
	char path[MAX_PATH];
	m_nMemMapMode = nMemMapMode;
	strcpy(m_szShaName, shmname);
#ifndef WIN32
	sprintf(path,"/dev/shm/%s",shmname );
#else
	sprintf(path,"%s",shmname );
#endif
	m_nBufSize=nSize-sizeof(MemQueueHeader);
	if( m_nMemMapMode== LOOP_SHMMEM_MODE )
		return OpenShmMap(nSize, path, bSetZero);
	else if(m_nMemMapMode== LOOP_MEM_MODE )
		return  OpenMem(nSize, bSetZero);
	else
		OpenShm(nSize , path, bSetZero);
	return true;
}

bool CLoopBuf::OpenMem(int memsize, bool bSetZero)
{
	m_pShmBuf = new BYTE[memsize + sizeof(MemQueueHeader)];
	if(m_pShmBuf == NULL)
		return false;
	m_pmap = (MemQueueHeader*)m_pShmBuf;

	m_pShmBuf+= sizeof(MemQueueHeader);
	if(bSetZero ==true)
	{
		m_pmap->nHeadPos = 0;
		m_pmap->nTailPos = 0;
		m_pmap->nSize = m_nBufSize;
		m_pmap->nQueueNo =1;
		memset( m_pShmBuf, 0 , memsize- sizeof(MemQueueHeader));
	}
	return true;
}
void CLoopBuf::CloseMem()
{
	m_pShmBuf-= sizeof(MemQueueHeader);
	delete m_pShmBuf;
}

// 打印日志
void CLoopBuf::WritePushLog( )
{
	sprintf(m_szLogstr,"RN:%s push succ %d zero %d \n",m_szShaName ,m_dwLogPushSucc ,m_dwLogPushZero);
}
// 打印日志
void CLoopBuf::WritePopLog( )
{
	sprintf(m_szLogstr,"RN:%s pop succ %d zero %d \n", m_szShaName,m_dwLogPopSucc ,m_dwLogPopZero );
}

bool  CLoopBuf::OpenShmMap(int memsize,char * shmname, bool bSetZero)
{
	int fd;
#ifndef WIN32
	if(bSetZero ==true )
		fd=open(shmname,O_CREAT|O_RDWR|O_TRUNC,00777);
	else
		fd=open(shmname,O_CREAT|O_RDWR,00777);

	lseek(fd,0,SEEK_SET);
	//write(fd,"", memsize);
	if(bSetZero ==true )
		ftruncate(fd,memsize );
	m_pmap = (MemQueueHeader*) mmap( NULL,memsize,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0 );
#else
	HANDLE hMapFile;
	LPVOID lpMapAddress;
	CString NameStr(shmname);
	//打开
	hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, NameStr);
	if (hMapFile != NULL) 
	{
		lpMapAddress = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		if(lpMapAddress == NULL)
		{
			CloseHandle(hMapFile); 
			return false;
		}
	}
	else
	{
		//不能打开，则创建
        SECURITY_ATTRIBUTES sa;
        SECURITY_DESCRIPTOR sd;
        ::InitializeSecurityDescriptor (&sd,SECURITY_DESCRIPTOR_REVISION);
        ::SetSecurityDescriptorDacl (&sd, TRUE, 0, FALSE);
        sa.nLength = sizeof (SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = &sd;
        sa.bInheritHandle = FALSE;  
		hMapFile = CreateFileMapping((HANDLE)0xFFFFFFFF, &sa,PAGE_READWRITE,	0, memsize, NameStr);
		if(hMapFile == NULL)
		{
			return false;
		}
		lpMapAddress = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		if(lpMapAddress == NULL)
		{
			CloseHandle(hMapFile); 
			return false;
		}
		//初始化内存
		memset(lpMapAddress,0,memsize);
		m_hMapFile = (int )hMapFile;
	}
	//成功创建
	m_pmap = (MemQueueHeader*) lpMapAddress;
#endif
	if(m_pmap == 0x0)
	{
		//printf("ER: mmap error %s %d  \n",m_szShaName,memsize );
		g_SendLog("ER: mmap error %s %d  \n",m_szShaName,memsize );
	}
	m_pShmBuf = (BYTE*)m_pmap;
	m_pShmBuf +=sizeof(MemQueueHeader);

	if(bSetZero ==true)
	{
		m_pmap->nHeadPos = 0;
		m_pmap->nTailPos = 0;
		m_pmap->nSize = m_nBufSize;
		m_pmap->wReadUseFlag = 0;
		m_pmap->wWriteUseFlag = 0;
		m_pmap->nQueueNo =1;
		memset( m_pShmBuf, 0 , memsize- sizeof(MemQueueHeader));
	}
	else // 占用情况
	{
		m_pmap->wReadUseFlag = 1;
	}
#ifndef WIN32
	close( fd );
#endif
	g_SendLog("FG:map mem initialize fin \n ");
	return true ;

}
bool  CLoopBuf::IsUsed(char * shmname )
{
	HANDLE hMapFile;
	LPVOID lpMapAddress;
	//CString NameStr(shmname);
	bool  bRet;
#ifdef win32
	//打开
	//if(bSetZero == false)
	//{
	CString NameStr(shmname);
	hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, NameStr);
	if (hMapFile != NULL) 
	{
		lpMapAddress = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		if(lpMapAddress == NULL)
		{
			CloseHandle(hMapFile); 
			return false;
		}
		//}
		//if(hMapFile ==NULL)
		//	return false;
	}
	else
	{
		return false;
	}
	MemQueueHeader *    pmap = (MemQueueHeader*) lpMapAddress;


	if( pmap->wReadUseFlag  == 1) // 被占用
		bRet =  true;
	else
		bRet =false;
	// 释放
	UnmapViewOfFile(lpMapAddress);
	CloseHandle(hMapFile);

	//g_SendLog("FG:map mem initialize fin \n ");
#endif
	return bRet ;

}
void CLoopBuf::SetZero()
{
	m_pmap->nHeadPos = 0;
	m_pmap->nTailPos = 0;
	m_pmap->nSize = m_nBufSize;
	m_pmap->nQueueNo =1;
	//memset( m_pShmBuf, 0 , memsize- sizeof(MemQueueHeader));
}
void CLoopBuf::CloseShmMap(int memsize)
{
#ifndef WIN32
	munmap( m_pmap, memsize);
#else
	LPVOID lpMapAddress = (LPVOID) m_pmap;
	UnmapViewOfFile(lpMapAddress);
	CloseHandle((HANDLE)m_hMapFile);
#endif
}

int  CLoopBuf::OpenShm(int memsize,char * shmname, bool bSetZero) // index 第几个共享内存队列 memsize 整数页  n* 4096
{
	//int m_shm_id;
#ifndef WIN32
	key_t key;

	//char *path = "/dev/shm/myshm2";
	/*
	使用 ftok根据path和作为项目标识符的单个字符生成key值确保进程间使用相同的 key
	使用相同key值的shmget只会在第一次时创建新结构,*/
	key = ftok(shmname,0);
	if(key == -1)
	{
		//printf("ER:shm %s size %d ftok error \n",m_szShaName, memsize);
		g_SendLog("ER:shm %s size %d ftok error \n",m_szShaName, memsize);
		return -1;
	}
	if(bSetZero == true)
	{
		m_shm_id = shmget(key,memsize,IPC_CREAT|IPC_EXCL|0666); // 最大32M  IPC_EXCL
		if(m_shm_id == -1)
		{
			m_shm_id = shmget(key,memsize,0);
			// 删除也有的
			shmctl(m_shm_id, IPC_RMID,0);
			//
			m_shm_id = shmget(key,memsize,IPC_CREAT|IPC_EXCL|0666); // 最大32M  IPC_EXCL
		}
	}
	else
	{
		m_shm_id = shmget(key,memsize,0); // 最大32M  IPC_EXCL
	}

	if(m_shm_id == -1)
	{
		//printf("ER:shm %s size %d shmget error \n",m_szShaName, memsize );
		g_SendLog("ER:shm %s size %d shmget error \n",m_szShaName, memsize );
		return -1;
	}
	m_pmap = (MemQueueHeader*)shmat(m_shm_id,NULL,0);

	if(m_pmap ==(MemQueueHeader*) -1 )
	{
		//printf("ER:shm %s size %d shmat failed \n",m_szShaName, memsize );
		g_SendLog("ER:shm %s size %d shmat failed \n",m_szShaName, memsize );
		return -1 ;
	}
	m_pShmBuf = (BYTE*)m_pmap;
	m_pShmBuf +=sizeof(MemQueueHeader);

	if(bSetZero ==true)
	{
		m_pmap->nHeadPos = 0;
		m_pmap->nTailPos = 0;
		m_pmap->nSize = m_nBufSize;
		m_pmap->nQueueNo =1;//m_nQueNo;
		memset( m_pShmBuf, 0 , memsize- sizeof(MemQueueHeader));
	}
#endif
	//printf("FG:shm %s size %d open shm initialize over \n",m_szShaName, memsize);
	g_SendLog("FG:shm %s size %d open shm initialize over \n ",m_szShaName, memsize);
	return 1 ;
}


void  CLoopBuf::CloseShm()
{
#ifndef WIN32
	//system("ipcs -m");
	if(shmdt(m_pmap) == -1)
	{
		perror("detach error");
	}
	shmctl(m_shm_id, IPC_RMID,0);
	//system("ipcs -m");
#endif
}
//
int CLoopBuf::PushIn(BYTE* lpData,  int nDataSize,BYTE * packet , int nPkgLen)
{
	int nLen =PushInNoblock( lpData,   nDataSize, packet , nPkgLen);
	if(nLen == 0)
	{
		SLEEP_MS(2);
		nLen =PushInNoblock(lpData,   nDataSize, packet , nPkgLen);
	}
	if(nLen == 0)
	{
		SLEEP_MS(2);
		nLen =PushInNoblock(lpData,   nDataSize, packet , nPkgLen);
	}
	return nLen;
}

int CLoopBuf::PushInNoblock(BYTE* lpData,  int nDataSize,BYTE * packet , int nPkgLen)
{
	int nFullDataSize = nDataSize + nPkgLen + LOOP_PKT_HEAD_LEN;
	//m_nDataSize = nDataSize+(LEN_INT- nDataSize%LEN_INT)%LEN_INT;  // 四字节对齐

	bool bEnough;
	int nHeadPos = m_pmap->nHeadPos;
	int nTempSize;
	IS_PUSH_SIZE_ENOUGH(nFullDataSize,bEnough)
	if( bEnough == false)
	{
		m_dwLogPushZero++;
		return 0;
	}

	LOOP_SET_PKT_HEAD(m_PktHead,nFullDataSize);

	PUSH_DATA(m_PktHead,LOOP_PKT_HEAD_LEN)
	PUSH_DATA(lpData,nDataSize)
	PUSH_DATA(packet,nPkgLen)
	return nDataSize+nPkgLen;
}

int CLoopBuf::PushIn(BYTE* lpData,  int nDataSize)
{
	int nLen =PushInNoblock(lpData,  nDataSize);
	if(nLen == 0)
	{
		SLEEP_MS(2);
		nLen =PushInNoblock(lpData,  nDataSize);
	}
	if(nLen == 0)
	{
		SLEEP_MS(2);
		nLen =PushInNoblock(lpData,  nDataSize);
	}
	return nLen;
}
// 不带有原始包的入队 比如说是只发送统计纪录
int CLoopBuf::PushInNoblock(BYTE* lpData,  int nDataSize)
{
	int nFullDataSize = nDataSize + LOOP_PKT_HEAD_LEN;
	//m_nDataSize = nDataSize+(LEN_INT- nDataSize%LEN_INT)%LEN_INT;  // 四字节对齐

	bool bEnough;
	int nHeadPos = m_pmap->nHeadPos;
	int nTempSize;
	IS_PUSH_SIZE_ENOUGH(nFullDataSize,bEnough)
	if( bEnough == false)
	{
		m_dwLogPushZero++;
		return 0;
	}

	LOOP_SET_PKT_HEAD(m_PktHead,nFullDataSize);

	PUSH_DATA(m_PktHead,LOOP_PKT_HEAD_LEN)
	PUSH_DATA(lpData,nDataSize)
	return nDataSize;
}

int CLoopBuf::PopOut(BYTE* lpData, int nDataBufSize)
{
	int nTailPos =  m_pmap->nTailPos;
	bool bEnough = false;
	int  nFullDataSize =0;

	IS_POP_SIZE_ENOUGH()
	if(bEnough == false)
	{
		if(nFullDataSize > 1024) // 1K 校验检查
		{
			if(nFullDataSize > nDataBufSize || nFullDataSize < 0)
			{
				SetZero();
				sprintf(m_szLogstr,"RN:%s popsize %d bufsize %d queue_share_mem error\n",m_szShaName,nFullDataSize,nDataBufSize);
			}
		}
		m_dwLogPopZero++;
		return 0;
	}
	POP_DATA(m_PopPktHead,LOOP_PKT_HEAD_LEN)
	RUN_CHECK_SYNCWORD(m_PopPktHead+4)
	//CHECK_SYNCWORD(m_PopPktHead+4)
	int nDataSize = nFullDataSize - LOOP_PKT_HEAD_LEN;
	if(nDataBufSize<nDataSize)
		return -1;
	POP_DATA(lpData,nDataSize)
	return nDataSize;
}

//
int CLoopBuf::GetSize()
{
	if(m_pmap->nHeadPos <=  m_pmap->nTailPos) //
	{
		return ( m_pmap->nTailPos - m_pmap->nHeadPos);
	}
	else //
	{
		return (m_nBufSize - m_pmap->nHeadPos +  m_pmap->nTailPos);
	}
}
// 解除占用
void  CLoopBuf::SetUnused()
{
	m_pmap->wReadUseFlag = 0;

}
void  CLoopBuf::SetUsed()
{
	m_pmap->wReadUseFlag = 1;

}
bool  CLoopBuf::IsUsed()
{
	if(	m_pmap->wReadUseFlag == 1)
	{
		return true;
	}
	return false;
}
