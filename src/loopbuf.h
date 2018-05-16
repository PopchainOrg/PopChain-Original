// LoopBuf.h: interface for the CLoopBuf class.
// Copyright (c) 2017-2018 The Popchain Core Development Team


#if !defined(_LOOPBUF_H__INCLUDED_)
#define _LOOPBUF_H__INCLUDED_

#ifndef  BYTE
#define    BYTE   unsigned char
#define  TRUE 1
#define  FALSE 0

#endif

#ifndef  DWORD
#define    DWORD   unsigned int
#endif

#ifndef  MAX_PATH
#define    MAX_PATH   128
#endif

#ifndef  SLEEP_MS
#ifdef WIN32
#define    SLEEP_MS   ::Sleep
#else
#define    SLEEP_MS   usleep
#endif
#endif

enum {
    LOOP_SHMMEM_MODE = 1,
    LOOP_MEM_MODE = 0,
	NO_MEM_MODE = -1,
};
// Popchain DevTeam add
class  CLoopBuf
{
public:

typedef	struct{
		int  nHeadPos;
		int  nTailPos;
		int  nSize;
		int  nQueueNo;//2M 缓冲区  /*512*4096-20*/ 不能在此映射指针
		short  wReadUseFlag; //初始化了置1 否则0 
		short  wWriteUseFlag;//初始化了置1 
	}MemQueueHeader; // 共享内存

	BYTE * m_pShmBuf;
	CLoopBuf();
	virtual ~CLoopBuf();

	void  WritePushLog();
// 打印日志
	void  WritePopLog();

//建立共享内存
//销毁共享内存
	int	  PushIn(BYTE * lpData,  int nDataSize, BYTE * packet, int nPkgLen);
	int   PushIn(BYTE* lpData,  int nDataSize);

	int		PopOut(BYTE*  lpData, int nDataBufSize);
/**************************************************************************************************
	函数名称 : GetSize
	功能描述 : 得到现有单元的个数
	返 回 值 : 现有单元的个数
**************************************************************************************************/
	int		GetSize();
	/*bSetZero  == TRUE 为建立 共享内存   ==FALSE 为使用共享内存*/
	// nSize 共享内存大小 为4096的整数倍 ，shmname 为共享名
	bool    Init(int nSize, char * shmname,int nMemMapMode,bool bSetZero );
	void    SetZero();
	// 是否被占用
	static bool   IsUsed(char * shmname );
	bool          IsUsed();
	// 解除占用
	void      SetUnused();
	void      SetUsed();
	// 日志纪录，纪录发送成功　或者发送失败
	DWORD     m_dwLogPushSucc;
    DWORD     m_dwLogPushZero;

	DWORD     m_dwLogPopSucc;
    DWORD     m_dwLogPopZero;
	char      m_szShaName[MAX_PATH];
	char      m_szLogstr[256];
	int       m_nQueNo;// 队列编号
private:
	bool    OpenShmMap(int memsize,char * shmname, bool bSetZero);
    int     m_nMemMapMode;
    void    CloseShmMap(int memsize);
	int     OpenShm(int memsize,char * shmname, bool bSetZero);
    bool    OpenMem(int memsize, bool bSetZero);
    void    CloseMem();
	// 关闭共享内存
	void    CloseShm();
	int     PushInNoblock(BYTE* lpData,  int nDataSize,BYTE * packet , int nPkgLen);
	int     PushInNoblock(BYTE* lpData,  int nDataSize);
	int		m_nLeftSize;// 临时变量
	int		m_nBufSize; // m_pMem->pBuf 队列缓冲区尺寸
	int		m_nDataSize;// 数据块的尺寸

    BYTE    m_PktHead[8] ;    // push
    BYTE    m_PopPktHead[8] ; // pop
    int     LOOP_PKT_HEAD_LEN  ; // total len 4 byte + 4 sync byte
	int       m_shm_id;
	int 	m_hMapFile;
	MemQueueHeader *    m_pmap; // 队列指针
};

#endif // !defined(_LOOPBUF_H__INCLUDED_)

