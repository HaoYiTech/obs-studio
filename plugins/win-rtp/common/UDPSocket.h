
#pragma once

#include "Socket.h"

class UDPSocket : public Socket
{
public:
	UDPSocket();
	~UDPSocket();
public:
	GM_Error 	Open() { return Socket::Open( SOCK_DGRAM ); }
	void 		SetTtl(UInt16 timeToLive);									// Set the TTL value...
	void		SetRemoteAddr(const char * lpAddr, int nPort);

	GM_Error	JoinMulticastForRecv(UINT MultiAddr, UINT InterAddr);		// Join Multicast for Receive...
	GM_Error	JoinMulticastForSend(UINT MultiAddr, UINT InterAddr);		// Join Multicast for Send...

	GM_Error 	LeaveMulticast(UInt32 inRemoteAddr);						// Leave the Multicast group...

	GM_Error	RecvFrom(UInt32* outRemoteAddr, UInt16* outRemotePort, void* ioBuffer, UInt32 inBufLen, UInt32* outRecvLen);
	GM_Error	SendTo(UInt32 inRemoteAddr, UInt16 inRemotePort, void* inBuffer, UInt32 inLength);
	GM_Error	SendTo(void* inBuffer, UInt32 inLength);

	UInt32		GetRemoteAddrV4() { return ntohl(m_MsgAddr.sin_addr.s_addr); }
	UInt16		GetRemotePortV4() { return ntohs(m_MsgAddr.sin_port); }
private:
	ip_mreq		m_IPmr;
	sockaddr_in	m_MsgAddr;
	int			m_nAddrLen;
	sockaddr_in	m_SendAddr;

	addrinfo  * m_lpMultiAddr;

	char       m_lpRemoteAddr[256];
	int        m_iPort;
};