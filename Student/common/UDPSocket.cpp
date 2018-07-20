
#include "UDPSocket.h"

UDPSocket::UDPSocket()
{
	::memset(&m_IPmr, 0, sizeof(m_IPmr));
	::memset(&m_MsgAddr, 0, sizeof(m_MsgAddr));

	m_lpMultiAddr = NULL;

	::memset(m_lpRemoteAddr,0,256);
}

UDPSocket::~UDPSocket()
{
	if( m_lpMultiAddr != NULL ) {
		freeaddrinfo(m_lpMultiAddr);
	}
	m_lpMultiAddr = NULL;
}

GM_Error UDPSocket::JoinMulticastForRecv(UINT MultiAddr, UINT InterAddr)
{
	ASSERT(m_hSocket != INVALID_SOCKET);
	memset(&m_IPmr, 0, sizeof(m_IPmr));

	m_IPmr.imr_interface.s_addr = InterAddr;
	m_IPmr.imr_multiaddr.s_addr = MultiAddr;
	int	err = setsockopt(m_hSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&m_IPmr, sizeof(m_IPmr));
	return ((err == SOCKET_ERROR) ? GetLastError() : GM_NoErr);
}


GM_Error UDPSocket::JoinMulticastForSend(UINT MultiAddr, UINT InterAddr)
{
	ASSERT(m_hSocket != INVALID_SOCKET);
	memset(&m_IPmr, 0, sizeof(m_IPmr));

	int err = setsockopt(m_hSocket, IPPROTO_IP, IP_MULTICAST_IF, (char *)&InterAddr, sizeof(DWORD));
	return ((err == SOCKET_ERROR) ? GetLastError() : GM_NoErr);
}

GM_Error UDPSocket::LeaveMulticast(UInt32 inRemoteAddr)
{
	ip_mreq	theMulti = {0};
	theMulti.imr_multiaddr.s_addr = htonl(inRemoteAddr);
	theMulti.imr_interface.s_addr = htonl(m_LocalAddrV4.sin_addr.s_addr);
	int err = setsockopt(m_hSocket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&theMulti, sizeof(theMulti));
	return ((err == -1) ? GetLastError() : GM_NoErr);
}

void UDPSocket::SetTtl(UInt16 timeToLive)
{
	u_char	nOptVal = (u_char)timeToLive;
	int err = setsockopt(m_hSocket, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&nOptVal, sizeof(nOptVal));
	ASSERT( err == 0 );
}

void UDPSocket::SetRemoteAddr(const char * lpAddr, int nPort)
{
	ASSERT( lpAddr != NULL);
	sockaddr_in	sRmtAdd;
	sRmtAdd.sin_addr.s_addr = inet_addr(lpAddr);
	sRmtAdd.sin_port = htons(nPort);
	sRmtAdd.sin_family = AF_INET;
	memcpy(&m_RemoteAddrV4, &sRmtAdd, sizeof(sockaddr_in));
}

GM_Error UDPSocket::SendTo(UInt32 inRemoteAddr, UInt16 inRemotePort, void* inBuffer, UInt32 inLength)
{
	ASSERT(inBuffer != NULL);
	sockaddr_in theRemoteAddr = {0};
	theRemoteAddr.sin_family = AF_INET;
	theRemoteAddr.sin_port	 = htons(inRemotePort);
	theRemoteAddr.sin_addr.s_addr = htonl(inRemoteAddr);
	//
	// Win32 says that inBuffer is a char*
	GM_Error theErr = GM_NoErr;
	int eErr = ::sendto(m_hSocket, (char*)inBuffer, inLength, 0, (sockaddr*)&theRemoteAddr, sizeof(theRemoteAddr));
	if( eErr == -1 ) {
		theErr = GetLastError();
		return theErr;
	}
	return GM_NoErr;
}

GM_Error UDPSocket::SendTo(void * inBuffer, UInt32 inLength)
{
	ASSERT(inBuffer != NULL);
	//struct sockaddr_in 	theRemoteAddr = {0};
	//theRemoteAddr.sin_family = AF_INET;
	//theRemoteAddr.sin_port	 = htons(inRemotePort);
	//theRemoteAddr.sin_addr.s_addr = htonl(inRemoteAddr);
	//
	// Win32 says that inBuffer is a char*
	GM_Error theErr = GM_NoErr;
	int eErr = ::sendto( m_hSocket, (char*)inBuffer, inLength, 0, (sockaddr*)&m_RemoteAddrV4, sizeof(m_RemoteAddrV4));
	if( eErr == -1 ) {
		theErr = GetLastError();
		return theErr;
	}
	return GM_NoErr;
}

GM_Error UDPSocket::RecvFrom(UInt32* outRemoteAddr, UInt16* outRemotePort, void* ioBuffer, UInt32 inBufLen, UInt32* outRecvLen)
{
	ASSERT(outRecvLen != NULL);
	ASSERT(outRemoteAddr != NULL);
	ASSERT(outRemotePort != NULL);

	GM_Error theErr = GM_NoErr;
	int addrLen = sizeof(m_MsgAddr);
	//
	// Win32 says that ioBuffer is a char*
	SInt32 theRecvLen = ::recvfrom(m_hSocket, (char*)ioBuffer, inBufLen, 0, (sockaddr*)&m_MsgAddr, &addrLen);
	if( theRecvLen == -1 ) {
		theErr = GetLastError();
		return theErr;
	}
	*outRemoteAddr = ntohl(m_MsgAddr.sin_addr.s_addr);
	*outRemotePort = ntohs(m_MsgAddr.sin_port);
	ASSERT(theRecvLen >= 0);
	*outRecvLen = (UInt32)theRecvLen;
	return GM_NoErr;		
}
