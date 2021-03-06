// This code is part of Pcap_DNSProxy
// Pcap_DNSProxy, A local DNS server base on WinPcap and LibPcap.
// Copyright (C) 2012-2014 Chengr28
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "Pcap_DNSProxy.h"

extern Configuration Parameter;
extern std::string LocalhostPTR[QUEUE_PARTNUM / 2U];
extern std::vector<AddressRange> *AddressRangeUsing;
extern std::vector<ResultBlacklistTable> *ResultBlacklistUsing;
extern std::mutex AddressRangeLock, ResultBlacklistLock;

//Check empty buffer
bool __fastcall CheckEmptyBuffer(const void *Buffer, const size_t Length)
{
	if (Buffer == nullptr)
		return true;

	for (size_t Index = 0;Index < Length;Index++)
	{
		if (((uint8_t *)Buffer)[Index] != NULL)
			return false;
	}

	return true;
}

/*
//Convert host values to network byte order with 64 bits
uint64_t __fastcall hton64(const uint64_t Val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return (((uint64_t)htonl((int32_t)((Val << 32U) >> 32U))) << 32U)|(uint32_t)htonl((int32_t)(Val >> 32U));
#else //Big-Endian
	return Val;
#endif
}

//Convert network byte order to host values with 64 bits
uint64_t __fastcall ntoh64(const uint64_t Val)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return (((uint64_t)ntohl((int32_t)((Val << 32U) >> 32U))) << 32U)|(uint32_t)ntohl((int32_t)(Val >> 32U));
#else //Big-Endian
	return Val;
#endif
}
*/

//Convert address strings to binary.
size_t __fastcall AddressStringToBinary(const PSTR AddrString, void *pAddr, const uint16_t Protocol, SSIZE_T &ErrorCode)
{
	SSIZE_T Result = 0;

//inet_ntop() and inet_pton() was only support in Windows Vista and newer system. [Roy Tam]
#ifdef _WIN64
#else //x86
	sockaddr_storage SockAddr = {0};
	int SockLength = 0;
#endif

//IPv6
	if (Protocol == AF_INET6)
	{
	//Check IPv6 addresses
		for (Result = 0;Result < (SSIZE_T)strlen(AddrString);Result++)
		{
			if (AddrString[Result] < 48 || AddrString[Result] > 58 && AddrString[Result] < 65 || AddrString[Result] > 70 && AddrString[Result] < 97 || AddrString[Result] > 102)
				break;
		}

		std::string sAddrString(AddrString);
	//Check abbreviation format.
		if (sAddrString.find(58) == std::string::npos)
		{
			sAddrString.clear();
			sAddrString.append("::");
			sAddrString.append(AddrString);
		}
		else if (sAddrString.find(58) == sAddrString.rfind(58))
		{
			sAddrString.replace(sAddrString.find(58), 1U, ("::"));
		}

	//Convert to binary.
	#ifdef _WIN64
		Result = inet_pton(AF_INET6, sAddrString.c_str(), pAddr);
		if (Result == SOCKET_ERROR || Result == FALSE)
	#else //x86
		SockLength = sizeof(sockaddr_in6);
		if (WSAStringToAddressA((PSTR)sAddrString.c_str(), AF_INET6, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
	#endif
		{
			ErrorCode = WSAGetLastError();
			return EXIT_FAILURE;
		}
	#ifdef _WIN64
	#else //x86
		memcpy(pAddr, &((PSOCKADDR_IN6)&SockAddr)->sin6_addr, sizeof(in6_addr));
	#endif
	}
//IPv4
	else {
		size_t CommaNum = 0;
		for (Result = 0;Result < (SSIZE_T)strlen(AddrString);Result++)
		{
			if (AddrString[Result] != 46 && AddrString[Result] < 48 || AddrString[Result] > 57)
				return EXIT_FAILURE;
			else if (AddrString[Result] == 46)
				CommaNum++;
		}

		std::string sAddrString(AddrString);
	//Delete zero(s) before whole data.
		while (sAddrString.length() > 1U && sAddrString[0] == 48 && sAddrString[1U] != 46)
			sAddrString.erase(0, 1U);

	//Check abbreviation format.
		if (CommaNum == 0)
		{
			sAddrString.clear();
			sAddrString.append("0.0.0.");
			sAddrString.append(AddrString);
		}
		else if (CommaNum == 1U)
		{
			sAddrString.replace(sAddrString.find(46), 1U, (".0.0."));
		}
		else if (CommaNum == 2U)
		{
			sAddrString.replace(sAddrString.find(46), 1U, (".0."));
		}

	//Delete zero(s) before data.
		while (sAddrString.find(".00") != std::string::npos)
			sAddrString.replace(sAddrString.find(".00"), 3U, ("."));
		while (sAddrString.find(".0") != std::string::npos)
			sAddrString.replace(sAddrString.find(".0"), 2U, ("."));
		while (sAddrString.find("..") != std::string::npos)
			sAddrString.replace(sAddrString.find(".."), 2U, (".0."));
		if (sAddrString[sAddrString.length() - 1U] == 46)
			sAddrString.append("0");

	//Convert to binary.
	#ifdef _WIN64
		Result = inet_pton(AF_INET, sAddrString.c_str(), pAddr);
		if (Result == SOCKET_ERROR || Result == FALSE)
	#else //x86
		SockLength = sizeof(sockaddr_in);
		if (WSAStringToAddressA((PSTR)sAddrString.c_str(), AF_INET, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
	#endif
		{
			ErrorCode = WSAGetLastError();
			return EXIT_FAILURE;
		}
	#ifdef _WIN64
	#else //x86
		memcpy(pAddr, &((PSOCKADDR_IN)&SockAddr)->sin_addr, sizeof(in_addr));
	#endif
	}

	return EXIT_SUCCESS;
}

//Convert local address(es) to reply DNS PTR Record(s)
size_t __fastcall LocalAddressToPTR(const uint16_t Protocol)
{
//Initialization
	std::shared_ptr<char> Addr(new char[ADDR_STRING_MAXSIZE]());
	sockaddr_storage SockAddr = {0};
	std::string Result;

	SSIZE_T Index = 0;
	size_t Location = 0, Colon = 0;
	std::string::iterator LocalAddressIter;
//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
#ifdef _WIN64
#else //x86
	DWORD BufferLength = ADDR_STRING_MAXSIZE;
#endif

	while (true)
	{
	//Get localhost address(es)
		memset(&SockAddr, 0, sizeof(sockaddr_storage));
		if (!GetLocalAddress(SockAddr, Protocol))
			return EXIT_FAILURE;

	//IPv6
		if (Protocol == AF_INET6)
		{
			std::string Temp[2U];
			Location = 0;
			Colon = 0;

		//Convert from in6_addr to string.
		//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
		#ifdef _WIN64
			if (inet_ntop(AF_INET6, &((PSOCKADDR_IN6)&SockAddr)->sin6_addr, Addr.get(), ADDR_STRING_MAXSIZE) == nullptr)
		#else //x86
			if (WSAAddressToStringA((LPSOCKADDR)&SockAddr, sizeof(sockaddr_in6), NULL, Addr.get(), &BufferLength) == SOCKET_ERROR)
		#endif
			{
				PrintError(WINSOCK_ERROR, L"Local IPv6 Address format error", WSAGetLastError(), nullptr, NULL);
				return EXIT_FAILURE;
			}
			Temp[0].append(Addr.get());

		//Convert to standard IPv6 address format A part(":0:" -> ":0000:").
			while (Temp[0].find(":0:", Index) != std::string::npos)
			{
				Index = Temp[0].find(":0:", Index);
				Temp[0].replace(Index, 3U, ":0000:");
			}

		//Count colon
			for (auto StringIndex:Temp[0])
			{
				if (StringIndex == 58)
					Colon++;
			}

		//Convert to standard IPv6 address format B part("::" -> ":0000:...").
			Location = Temp[0].find("::");
			Colon = 8U - Colon;
			Temp[1U].append(Temp[0], 0, Location);
			while (Colon != 0)
			{
				Temp[1U].append(":0000");
				Colon--;
			}
			Temp[1U].append(Temp[0], Location + 1U, Temp[0].length() - Location + 1U);

			for (LocalAddressIter = Temp[1U].begin();LocalAddressIter != Temp[1U].end();LocalAddressIter++)
			{
				if (*LocalAddressIter == 58)
					LocalAddressIter = Temp[1U].erase(LocalAddressIter);
			}

		//Convert to DNS PTR Record and copy to Result.
			for (Index = (SSIZE_T)(Temp[1U].length() - 1U);Index != -1;Index--)
			{
				char Word[] = {0, 0};
				Word[0] = Temp[1U].at(Index);
				Result.append(Word);
				Result.append(".");
			}

			Result.append("ip6.arpa");
			LocalhostPTR[0].swap(Result);
			Result.clear();
			Result.shrink_to_fit();
		}
	//IPv4
		else {
			char CharAddr[4U][4U] = {0};
			size_t Localtion[] = {0, 0};

		//Convert from in_addr to string.
		//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
		#ifdef _WIN64
			if (inet_ntop(AF_INET, &((PSOCKADDR_IN)&SockAddr)->sin_addr, Addr.get(), ADDR_STRING_MAXSIZE) == nullptr)
		#else //x86
			if (WSAAddressToStringA((LPSOCKADDR)&SockAddr, sizeof(sockaddr_in), NULL, Addr.get(), &BufferLength) == SOCKET_ERROR)
		#endif
			{
				PrintError(WINSOCK_ERROR, L"Local IPv4 Address format error", WSAGetLastError(), nullptr, NULL);
				return EXIT_FAILURE;
			}

		//Detach Address data.
			for (Index = 0;Index < (SSIZE_T)strlen(Addr.get());Index++)
			{
				if (Addr.get()[Index] == 46)
				{
					Localtion[1U] = 0;
					Localtion[0]++;
				}
				else {
					CharAddr[Localtion[0]][Localtion[1U]] = Addr.get()[Index];
					Localtion[1U]++;
				}
			}

		//Convert to DNS PTR Record and copy to Result.
			for (Index = 4;Index > 0;Index--)
			{
				Result.append(CharAddr[Index - 1]);
				Result.append(".");
			}

			Result.append("in-addr.arpa");
			LocalhostPTR[1U].swap(Result);
			Result.clear();
			Result.shrink_to_fit();
		}

	//Auto-refresh
		if (Parameter.FileRefreshTime == 0)
			return EXIT_SUCCESS;
		else 
			Sleep((DWORD)Parameter.FileRefreshTime);
	}

	return EXIT_SUCCESS;
}

//Check IP(v4/v6) special addresses
bool __fastcall CheckSpecialAddress(const void *Addr, const uint16_t Protocol, const PSTR Domain)
{
	if (Protocol == AF_INET6) //IPv6
	{
	//Some DNS Poisoning addresses from CERNET2
		if (((in6_addr *)Addr)->u.Word[0] == 0 && ((in6_addr *)Addr)->u.Word[1U] == 0 && ((in6_addr *)Addr)->u.Word[2U] == 0 && ((in6_addr *)Addr)->u.Word[3U] == 0 && ((in6_addr *)Addr)->u.Byte[8U] == 0x90 && ((in6_addr *)Addr)->u.Word[6U] == 0 && ((in6_addr *)Addr)->u.Word[7U] == 0 || //::90xx:xxxx:0:0
			((in6_addr *)Addr)->u.Word[0] == htons(0x0010) && ((in6_addr *)Addr)->u.Word[1U] == 0 && ((in6_addr *)Addr)->u.Word[2U] == 0 && ((in6_addr *)Addr)->u.Word[3U] == 0 && ((in6_addr *)Addr)->u.Word[4U] == 0 && ((in6_addr *)Addr)->u.Word[5U] == 0 && ((in6_addr *)Addr)->u.Word[6U] == 0 && ((in6_addr *)Addr)->u.Word[7U] == htons(0x2222) || //10:2222
			((in6_addr *)Addr)->u.Word[0] == htons(0x0021) && ((in6_addr *)Addr)->u.Word[1U] == htons(0x0002) && ((in6_addr *)Addr)->u.Word[2U] == 0 && ((in6_addr *)Addr)->u.Word[3U] == 0 && ((in6_addr *)Addr)->u.Word[4U] == 0 && ((in6_addr *)Addr)->u.Word[5U] == 0 && ((in6_addr *)Addr)->u.Word[6U] == 0 && ((in6_addr *)Addr)->u.Word[7U] == htons(0x0002) || //21:2::2
			((in6_addr *)Addr)->u.Word[0] == htons(0x2001) && 
//			(((in6_addr *)Addr)->u.Word[1U] == 0 && ((in6_addr *)Addr)->u.Word[2U] == 0 && ((in6_addr *)Addr)->u.Word[3U] == 0 && ((in6_addr *)Addr)->u.Word[4U] == 0 && ((in6_addr *)Addr)->u.Word[5U] == 0 && ((in6_addr *)Addr)->u.Word[6U] == 0 && ((in6_addr *)Addr)->u.Word[7U] == htons(0x0212) || //2001::212
			((in6_addr *)Addr)->u.Word[1U] == htons(0x0DA8) && ((in6_addr *)Addr)->u.Word[2U] == htons(0x0112) && ((in6_addr *)Addr)->u.Word[3U] == 0 && ((in6_addr *)Addr)->u.Word[4U] == 0 && ((in6_addr *)Addr)->u.Word[5U] == 0 && ((in6_addr *)Addr)->u.Word[6U] == 0 && ((in6_addr *)Addr)->u.Word[7U] == htons(0x21AE) || //2001:DA8:112::21AE
			((in6_addr *)Addr)->u.Word[0] == htons(0x2003) && ((in6_addr *)Addr)->u.Word[1U] == htons(0x00FF) && ((in6_addr *)Addr)->u.Word[2U] == htons(0x0001) && ((in6_addr *)Addr)->u.Word[3U] == htons(0x0002) && ((in6_addr *)Addr)->u.Word[4U] == htons(0x0003) && ((in6_addr *)Addr)->u.Word[5U] == htons(0x0004) && ((in6_addr *)Addr)->u.Word[6U] == htons(0x5FFF) && ((in6_addr *)Addr)->u.Word[7U] == htons(0x0006) || //2003:FF:1:2:3:4:5FFF:6
			((in6_addr *)Addr)->u.Word[0] == htons(0x2123) && ((in6_addr *)Addr)->u.Word[1U] == 0 && ((in6_addr *)Addr)->u.Word[2U] == 0 && ((in6_addr *)Addr)->u.Word[3U] == 0 && ((in6_addr *)Addr)->u.Word[4U] == 0 && ((in6_addr *)Addr)->u.Word[5U] == 0 && ((in6_addr *)Addr)->u.Word[6U] == 0 && ((in6_addr *)Addr)->u.Word[7U] == htons(0x3E12) || //2123::3E12
		//About this list, see https://en.wikipedia.org/wiki/IPv6_address#Presentation and https://en.wikipedia.org/wiki/Reserved_IP_addresses#Reserved_IPv6_addresses.
			(((in6_addr *)Addr)->u.Word[0] == 0 && ((in6_addr *)Addr)->u.Word[1U] == 0 && ((in6_addr *)Addr)->u.Word[2U] == 0 && ((in6_addr *)Addr)->u.Word[3U] == 0 && ((in6_addr *)Addr)->u.Word[4U] == 0 && 
			((((in6_addr *)Addr)->u.Word[5U] == 0 && 
			((((in6_addr *)Addr)->u.Word[6U] == 0 && ((in6_addr *)Addr)->u.Word[7U] == 0 || //Unspecified ((in6_addr *)Addr)ess(::, Section 2.5.2 in RFC 4291)
			((in6_addr *)Addr)->u.Word[6U] == 0 && ((in6_addr *)Addr)->u.Word[7U] == htons(0x0001)) || //Loopback ((in6_addr *)Addr)ess(::1, Section 2.5.3 in RFC 4291)
			((in6_addr *)Addr)->u.Word[5U] == 0)) || //IPv4-Compatible Contrast ((in6_addr *)Addr)ess(::/96, Section 2.5.5.1 in RFC 4291)
			((in6_addr *)Addr)->u.Word[5U] == htons(0xFFFF))) || //IPv4-mapped ((in6_addr *)Addr)ess(::FFFF:0:0/96, Section 2.5.5 in RFC 4291)
//			((in6_addr *)Addr)->u.Word[0] == htons(0x0064) && ((in6_addr *)Addr)->u.Word[1U] == htons(0xFF9B) && ((in6_addr *)Addr)->u.Word[2U] == 0 && ((in6_addr *)Addr)->u.Word[3U] == 0 && ((in6_addr *)Addr)->u.Word[4U] == 0 && ((in6_addr *)Addr)->u.Word[5U] == 0 || //Well Known Prefix(64:FF9B::/96, Section 2.1 in RFC 4773)
			((in6_addr *)Addr)->u.Word[0] == htons(0x0100) && ((in6_addr *)Addr)->u.Word[1U] == 0 && ((in6_addr *)Addr)->u.Word[1U] == 0 && ((in6_addr *)Addr)->u.Word[1U] == 0 && ((in6_addr *)Addr)->u.Word[1U] == 0 && ((in6_addr *)Addr)->u.Word[1U] == 0 || //Discard Prefix(100::/64, Section 4 RFC 6666)
			((in6_addr *)Addr)->u.Word[0] == htons(0x2001) && 
			(((in6_addr *)Addr)->u.Word[1U] == 0 || //Teredo relay/tunnel ((in6_addr *)Addr)ess(2001::/32, RFC 4380)
			((in6_addr *)Addr)->u.Byte[2U] == 0 && ((in6_addr *)Addr)->u.Byte[3U] <= 0x07 || //Sub-TLA IDs assigned to IANA(2001:0000::/29, Section 2 in RFC 4773)
			((in6_addr *)Addr)->u.Byte[3U] >= 0x10 && ((in6_addr *)Addr)->u.Byte[3U] <= 0x1F || //Overlay Routable Cryptographic Hash IDentifiers/ORCHID ((in6_addr *)Addr)ess(2001:10::/28 in RFC 4843)
			((in6_addr *)Addr)->u.Byte[2U] >= 0x01 && ((in6_addr *)Addr)->u.Byte[3U] >= 0xF8 || //Sub-TLA IDs assigned to IANA(2001:01F8::/29, Section 2 in RFC 4773)
			((in6_addr *)Addr)->u.Word[1U] == htons(0x0DB8)) || //Contrast ((in6_addr *)Addr)ess prefix reserved for documentation(2001:DB8::/32, RFC 3849)
//			((in6_addr *)Addr)->u.Word[0] == htons(0x2002) && ((in6_addr *)Addr)->u.Word[1U] == 0 || //6to4 relay/tunnel ((in6_addr *)Addr)ess(2002::/16, Section 2 in RFC 3056)
			((in6_addr *)Addr)->u.Word[0] == htons(0x3FFE) && ((in6_addr *)Addr)->u.Word[1U] == 0 || //6bone ((in6_addr *)Addr)ess(3FFE::/16, RFC 3701)
			((in6_addr *)Addr)->u.Byte[0] == 0x5F || //6bone ((in6_addr *)Addr)ess(5F00::/8, RFC 3701)
//			((in6_addr *)Addr)->u.Byte[0] >= 0xFC && ((in6_addr *)Addr)->u.Byte[0] <= 0xFD || //Unique Local Unicast ((in6_addr *)Addr)ess/ULA(FC00::/7, Section 2.5.7 in RFC 4193)
			((in6_addr *)Addr)->u.Byte[0] == 0xFE && 
			(((in6_addr *)Addr)->u.Byte[1U] >= 0x80 && ((in6_addr *)Addr)->u.Byte[1U] <= 0xBF || //Link-Local Unicast Contrast ((in6_addr *)Addr)ess(FE80::/10, Section 2.5.6 in RFC 4291)
			((in6_addr *)Addr)->u.Byte[1U] >= 0xC0) || //Site-Local scoped ((in6_addr *)Addr)ess(FEC0::/10, RFC 3879)
//			((in6_addr *)Addr)->u.Byte[0] == 0xFF || //Multicast ((in6_addr *)Addr)ess(FF00::/8, Section 2.7 in RFC 4291)
			((in6_addr *)Addr)->u.Word[5U] == htons(0x5EFE)) //ISATAP Interface Identifiers(Prefix::5EFE:0:0:0:0/64, Section 6.1 in RFC 5214)
				return true;

	//Extended check
		if (Domain != nullptr)
		{
		//Domain case conversion
			for (size_t Index = 0;Index < strlen(Domain);Index++)
			{
				if (Domain[Index] > 64U && Domain[Index] < 91U)
					Domain[Index] += 32U;
			}

		//Main check
			for (auto ResultBlacklistIter:*ResultBlacklistUsing)
			{
				if (ResultBlacklistIter.Addresses.front().ss_family == AF_INET6 && std::regex_match(Domain, ResultBlacklistIter.Pattern))
				{
					for (auto SockAddrIter:ResultBlacklistIter.Addresses)
					{
						if (memcmp(Addr, &((PSOCKADDR_IN6)&SockAddrIter)->sin6_addr, sizeof(in6_addr)) == 0)
							return true;
					}
				}
			}
		}
	}
	else { //IPv4
	//About this list, see https://zh.wikipedia.org/wiki/%E5%9F%9F%E5%90%8D%E6%9C%8D%E5%8A%A1%E5%99%A8%E7%BC%93%E5%AD%98%E6%B1%A1%E6%9F%93#.E8.99.9A.E5.81.87IP.E5.9C.B0.E5.9D.80.
		if (((in_addr *)Addr)->S_un.S_addr == inet_addr("1.1.1.1") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("4.36.66.178") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("8.7.198.45") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("37.61.54.158") || 
			((in_addr *)Addr)->S_un.S_addr == inet_addr("46.82.174.68") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("59.24.3.173") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("64.33.88.161") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("64.33.99.47") || 
			((in_addr *)Addr)->S_un.S_addr == inet_addr("64.66.163.251") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("65.104.202.252") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("65.160.219.113") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("66.45.252.237") || 
			((in_addr *)Addr)->S_un.S_addr == inet_addr("72.14.205.99") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("72.14.205.104") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("78.16.49.15") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("93.46.8.89") || 
			((in_addr *)Addr)->S_un.S_addr == inet_addr("128.121.126.139") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("159.106.121.75") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("169.132.13.103") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("192.67.198.6") || 
			((in_addr *)Addr)->S_un.S_addr == inet_addr("202.106.1.2") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("202.181.7.85") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("203.98.7.65") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("203.161.230.171") || 
			((in_addr *)Addr)->S_un.S_addr == inet_addr("207.12.88.98") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("208.56.31.43") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("209.36.73.33") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("209.145.54.50") || 
			((in_addr *)Addr)->S_un.S_addr == inet_addr("209.220.30.174") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("211.94.66.147") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("213.169.251.35") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("216.221.188.182") || 
		//From http://forums.internetfreedom.org/index.php?topic=7953.0
			((in_addr *)Addr)->S_un.S_addr == inet_addr("23.89.5.60") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("49.2.123.56") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("54.76.135.1") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("77.4.7.92") || 
			((in_addr *)Addr)->S_un.S_addr == inet_addr("118.5.49.6") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("188.5.4.96") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("189.163.17.5") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("197.4.4.12") || 
			((in_addr *)Addr)->S_un.S_addr == inet_addr("216.234.179.13") || 
//			((in_addr *)Addr)->S_un.S_addr == inet_addr("243.185.187.39") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("249.129.46.48") || ((in_addr *)Addr)->S_un.S_addr == inet_addr("253.157.14.165") || 
		//About this list, see https://en.wikipedia.org/wiki/IPv4#Special-use_addresses and https://en.wikipedia.org/wiki/Reserved_IP_addresses#Reserved_IPv4_addresses.
			((in_addr *)Addr)->S_un.S_un_b.s_b1 == 0 || //Current network whick only valid as source address(0.0.0.0/8, Section 3.2.1.3 in RFC 1122)
//			((in_addr *)Addr)->S_un.S_un_b.s_b1 == 0x0A || //Private class A address(10.0.0.0/8, Section 3 in RFC 1918)
			((in_addr *)Addr)->S_un.S_un_b.s_b1 == 0x7F || //Loopback address(127.0.0.0/8, Section 3.2.1.3 in RFC 1122)
//			((in_addr *)Addr)->S_un.S_un_b.s_b1 == && ((in_addr *)Addr)->S_un.S_un_b.s_b2 > 0x40 && ((in_addr *)Addr)->S_un.S_un_b.s_b2 < 0x7F || //Carrier-grade NAT(100.64.0.0/10, Section 7 in RFC 6598)
			((in_addr *)Addr)->S_un.S_un_b.s_b1 == 0xA9 && ((in_addr *)Addr)->S_un.S_un_b.s_b2 >= 0xFE || //Link-local address(169.254.0.0/16, Section 1.5 in RFC 3927)
//			((in_addr *)Addr)->S_un.S_un_b.s_b1 == 0xAC && ((in_addr *)Addr)->S_un.S_un_b.s_b2 >= 0x10 && ((in_addr *)Addr)->S_un.S_un_b.s_b2 <= 0x1F || //Private class B address(172.16.0.0/16, Section 3 in RFC 1918)
			((in_addr *)Addr)->S_un.S_un_b.s_b1 == 0xC0 && ((in_addr *)Addr)->S_un.S_un_b.s_b2 == 0 && ((in_addr *)Addr)->S_un.S_un_b.s_b3 == 0 && ((in_addr *)Addr)->S_un.S_un_b.s_b4 >= 0 && ((in_addr *)Addr)->S_un.S_un_b.s_b4 < 0x08 || //DS-Lite transition mechanism(192.0.0.0/29, Section 3 in RFC 6333)
			((in_addr *)Addr)->S_un.S_un_b.s_b1 == 0xC0 && (((in_addr *)Addr)->S_un.S_un_b.s_b2 == 0 && (((in_addr *)Addr)->S_un.S_un_b.s_b3 == 0 || //Reserved for IETF protocol assignments address(192.0.0.0/24, Section 3 in RFC 5735)
			((in_addr *)Addr)->S_un.S_un_b.s_b3 == 0x02)) || //TEST-NET-1 address(192.0.2.0/24, Section 3 in RFC 5735)
//			((in_addr *)Addr)->S_un.S_un_b.s_b2 == 0x58 && ((in_addr *)Addr)->S_un.S_un_b.s_b3 == 0x63 || //6to4 relay/tunnel address(192.88.99.0/24, Section 2.3 in RFC 3068)
//			((in_addr *)Addr)->S_un.S_un_b.s_b1 == 0xC0 && ((in_addr *)Addr)->S_un.S_un_b.s_b2 == 0xA8 || //Private class C address(192.168.0.0/24, Section 3 in RFC 1918)
			((in_addr *)Addr)->S_un.S_un_b.s_b1 == 0xC6 && (((in_addr *)Addr)->S_un.S_un_b.s_b2 == 0x12 || //Benchmarking Methodology for Network Interconnect Devices address(198.18.0.0/15, Section 11.4.1 in RFC 2544)
			((in_addr *)Addr)->S_un.S_un_b.s_b2 == 0x33 && ((in_addr *)Addr)->S_un.S_un_b.s_b3 == 0x64) || //TEST-NET-2(198.51.100.0/24, Section 3 in RFC 5737)
			((in_addr *)Addr)->S_un.S_un_b.s_b1 == 0xCB && ((in_addr *)Addr)->S_un.S_un_b.s_b2 == 0 && ((in_addr *)Addr)->S_un.S_un_b.s_b3 == 0x71 || //TEST-NET-3(203.0.113.0/24, Section 3 in RFC 5737)
//			((in_addr *)Addr)->S_un.S_un_b.s_b1 == 0xE0 || //Multicast address(224.0.0.0/4, Section 2 in RFC 3171)
			((in_addr *)Addr)->S_un.S_un_b.s_b1 >= 0xF0) //Reserved for future use address(240.0.0.0/4, Section 4 in RFC 1112) and Broadcast address(255.255.255.255/32, Section 7 in RFC 919/RFC 922)
				return true;

	//Extended check
		if (Domain != nullptr)
		{
		//Domain case conversion
			for (size_t Index = 0;Index < strlen(Domain);Index++)
			{
				if (Domain[Index] > 64U && Domain[Index] < 91U)
					Domain[Index] += 32U;
			}

		//Main check
			for (auto ResultBlacklistIter:*ResultBlacklistUsing)
			{
				if (ResultBlacklistIter.Addresses.front().ss_family == AF_INET && std::regex_match(Domain, ResultBlacklistIter.Pattern))
				{
					for (auto SockAddrIter:ResultBlacklistIter.Addresses)
					{
						if (((in_addr *)Addr)->S_un.S_addr == ((PSOCKADDR_IN)&SockAddrIter)->sin_addr.S_un.S_addr)
							return true;
					}
				}
			}
		}
	}

	return false;
}

//Custom Mode addresses filter
bool __fastcall CustomModeFilter(const void *pAddr, const uint16_t Protocol)
{
	size_t Index = 0;

//IPv6
	if (Protocol == AF_INET6)
	{
		auto Addr = (in6_addr *)pAddr;
	//Permit
		if (Parameter.IPFilterOptions.Type)
		{
			std::unique_lock<std::mutex> AddressRangeMutex(AddressRangeLock);
			for (auto AddressRangeIter:*AddressRangeUsing)
			{
			//Check Protocol and Level.
				if (AddressRangeIter.Begin.ss_family != AF_INET6 || Parameter.IPFilterOptions.IPFilterLevel != 0 && AddressRangeIter.Level < Parameter.IPFilterOptions.IPFilterLevel)
					continue;

			//Check address.
				for (Index = 0;Index < sizeof(in6_addr) / sizeof(uint16_t);Index++)
				{
					if (ntohs(Addr->u.Word[Index]) > ntohs(((PSOCKADDR_IN6)&AddressRangeIter.Begin)->sin6_addr.u.Word[Index]) && ntohs(Addr->u.Word[Index]) < ntohs(((PSOCKADDR_IN6)&AddressRangeIter.End)->sin6_addr.u.Word[Index]))
					{
						return true;
					}
					else if (Addr->u.Word[Index] == ((PSOCKADDR_IN6)&AddressRangeIter.Begin)->sin6_addr.u.Word[Index] || Addr->u.Word[Index] == ((PSOCKADDR_IN6)&AddressRangeIter.End)->sin6_addr.u.Word[Index])
					{
						if (Index == sizeof(in6_addr) / sizeof(uint16_t) - 1U)
							return true;
						else 
							continue;
					}
					else {
						return false;
					}
				}
			}
		}
	//Deny
		else {
			std::unique_lock<std::mutex> AddressRangeMutex(AddressRangeLock);
			for (auto AddressRangeIter:*AddressRangeUsing)
			{
			//Check Protocol and Level.
				if (AddressRangeIter.Begin.ss_family != AF_INET6 || Parameter.IPFilterOptions.IPFilterLevel != 0 && AddressRangeIter.Level < Parameter.IPFilterOptions.IPFilterLevel)
					continue;

			//Check address.
				for (Index = 0;Index < sizeof(in6_addr) / sizeof(uint16_t);Index++)
				{
					if (ntohs(Addr->u.Word[Index]) > ntohs(((PSOCKADDR_IN6)&AddressRangeIter.Begin)->sin6_addr.u.Word[Index]) && ntohs(Addr->u.Word[Index]) < ntohs(((PSOCKADDR_IN6)&AddressRangeIter.End)->sin6_addr.u.Word[Index]))
					{
						return false;
					}
					else if (Addr->u.Word[Index] == ((PSOCKADDR_IN6)&AddressRangeIter.Begin)->sin6_addr.u.Word[Index] || Addr->u.Word[Index] == ((PSOCKADDR_IN6)&AddressRangeIter.End)->sin6_addr.u.Word[Index])
					{
						if (Index == sizeof(in6_addr) / sizeof(uint16_t) - 1U)
							return false;
						else 
							continue;
					}
					else {
						return true;
					}
				}
			}
		}
	}
//IPv4
	else {
		auto Addr = (in_addr *)pAddr;
	//Permit
		if (Parameter.IPFilterOptions.Type)
		{
			std::unique_lock<std::mutex> AddressRangeMutex(AddressRangeLock);
			for (auto AddressRangeIter:*AddressRangeUsing)
			{
			//Check Protocol and Level.
				if (AddressRangeIter.Begin.ss_family != AF_INET || Parameter.IPFilterOptions.IPFilterLevel != 0 && AddressRangeIter.Level < Parameter.IPFilterOptions.IPFilterLevel)
					continue;

			//Check address.
				if (Addr->S_un.S_un_b.s_b1 > ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b1 && Addr->S_un.S_un_b.s_b1 < ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b1)
				{
					return true;
				}
				else if (Addr->S_un.S_un_b.s_b1 == ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b1 || Addr->S_un.S_un_b.s_b1 == ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b1)
				{
					if (Addr->S_un.S_un_b.s_b2 > ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b2 && Addr->S_un.S_un_b.s_b2 < ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b2)
					{
						return true;
					}
					else if (Addr->S_un.S_un_b.s_b2 == ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b2 || Addr->S_un.S_un_b.s_b2 == ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b2)
					{
						if (Addr->S_un.S_un_b.s_b3 > ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b3 && Addr->S_un.S_un_b.s_b3 < ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b3)
						{
							return true;
						}
						else if (Addr->S_un.S_un_b.s_b3 == ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b3 || Addr->S_un.S_un_b.s_b3 == ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b3)
						{
							if (Addr->S_un.S_un_b.s_b4 >= ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b4 && Addr->S_un.S_un_b.s_b4 <= ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b4)
							{
								return true;
							}
							else {
								return false;
							}
						}
						else {
							return false;
						}
					}
					else {
						return false;
					}
				}
				else {
					return false;
				}
			}
		}
	//Deny
		else {
			std::unique_lock<std::mutex> AddressRangeMutex(AddressRangeLock);
			for (auto AddressRangeIter:*AddressRangeUsing)
			{
			//Check Protocol and Level.
				if (AddressRangeIter.Begin.ss_family != AF_INET || Parameter.IPFilterOptions.IPFilterLevel != 0 && AddressRangeIter.Level < Parameter.IPFilterOptions.IPFilterLevel)
					continue;

			//Check address.
				if (Addr->S_un.S_un_b.s_b1 > ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b1 && Addr->S_un.S_un_b.s_b1 < ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b1)
				{
					return false;
				}
				else if (Addr->S_un.S_un_b.s_b1 == ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b1 || Addr->S_un.S_un_b.s_b1 == ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b1)
				{
					if (Addr->S_un.S_un_b.s_b2 > ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b2 && Addr->S_un.S_un_b.s_b2 < ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b2)
					{
						return false;
					}
					else if (Addr->S_un.S_un_b.s_b2 == ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b2 || Addr->S_un.S_un_b.s_b2 == ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b2)
					{
						if (Addr->S_un.S_un_b.s_b3 > ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b3 && Addr->S_un.S_un_b.s_b3 < ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b3)
						{
							return false;
						}
						else if (Addr->S_un.S_un_b.s_b3 == ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b3 || Addr->S_un.S_un_b.s_b3 == ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b3)
						{
							if (Addr->S_un.S_un_b.s_b4 >= ((PSOCKADDR_IN)&AddressRangeIter.Begin)->sin_addr.S_un.S_un_b.s_b4 && Addr->S_un.S_un_b.s_b4 <= ((PSOCKADDR_IN)&AddressRangeIter.End)->sin_addr.S_un.S_un_b.s_b4)
							{
								return false;
							}
							else {
								return true;
							}
						}
						else {
							return true;
						}
					}
					else {
						return true;
					}
				}
				else {
					return true;
				}
			}
		}
	}

	return true;
}

/*
//Get Ethernet Frame Check Sequence/FCS
uint32_t __fastcall GetFCS(const PUINT8 Buffer, const size_t Length)
{
	uint32_t Table[256] = {0}, Gx = 0x04C11DB7, Temp = 0, CRCTable = 0, Value = 0, UI = 0;
	char ReflectNum[] = {8, 32};
	int Index[3U] = {0};

	for (Index[0] = 0;Index[0] <= U8_MAXNUM;Index[0]++)
	{
		Value = 0;
		UI = Index[0];
		for (Index[1U] = 1U;Index[1U] < 9;Index[1U]++)
		{
			if (UI & 1)
				Value |= 1 << (ReflectNum[0]-Index[1U]);
			UI >>= 1;
		}
		Temp = Value;
		Table[Index[0]] = Temp << 24;

		for (Index[2U] = 0;Index[2U] < 8;Index[2U]++)
		{
			unsigned long int t1 = 0, t2 = 0, Flag = Table[Index[0]] & 0x80000000;
			t1 = (Table[Index[0]] << 1);
			if (Flag == 0)
				t2 = 0;
			else
				t2 = Gx;
			Table[Index[0]] = t1 ^ t2;
		}
		CRCTable = Table[Index[0]];

		UI = Table[Index[0]];
		Value = 0;
		for (Index[1U] = 1;Index[1U] < 33;Index[1U]++)
		{
			if (UI & 1)
				Value |= 1 << (ReflectNum[1U] - Index[1U]);
			UI >>= 1;
		}
		Table[Index[0]] = Value;
	}

	uint32_t CRC = 0xFFFFFFFF;
	for (Index[0] = 0;Index[0] < (int)Length;Index[0]++)
		CRC = Table[(CRC ^ (*(Buffer + Index[0]))) & U8_MAXNUM]^(CRC >> 8);

	return ~CRC;
}
*/

//Get Checksum
uint16_t __fastcall GetChecksum(const uint16_t *Buffer, const size_t Length)
{
	uint32_t Checksum = 0;
	size_t InnerLength = Length;

	while (InnerLength > 1U)
	{ 
		Checksum += *Buffer++;
		InnerLength -= sizeof(uint16_t);
	}
	
	if (InnerLength)
		Checksum += *(PUINT8)Buffer;

	Checksum = (Checksum >> 16U) + (Checksum & U16_MAXNUM);
	Checksum += (Checksum >> 16U);

	return (uint16_t)(~Checksum);
}

//Get ICMPv6 checksum
uint16_t __fastcall ICMPv6Checksum(const PUINT8 Buffer, const size_t Length, const in6_addr Destination, const in6_addr Source)
{
	std::shared_ptr<char> Validation(new char[sizeof(ipv6_psd_hdr) + Length]());

//Get checksum
	auto psd = (ipv6_psd_hdr *)Validation.get();
	psd->Dst = Destination;
	psd->Src = Source;
	psd->Length = htonl((uint32_t)Length);
	psd->Next_Header = IPPROTO_ICMPV6;
	memcpy(Validation.get() + sizeof(ipv6_psd_hdr), Buffer + sizeof(ipv6_hdr), Length);
	return GetChecksum((PUINT16)Validation.get(), sizeof(ipv6_psd_hdr) + Length);
}

//Get UDP checksum
uint16_t __fastcall TCPUDPChecksum(const PUINT8 Buffer, const size_t Length, const uint16_t NetworkLayer, const uint16_t TransportLayer)
{
//Get checksum.
	uint16_t Result = EXIT_FAILURE;
	if (NetworkLayer == AF_INET6) //IPv6
	{
		std::shared_ptr<char> Validation(new char[sizeof(ipv6_psd_hdr) + Length]());
		auto psd = (ipv6_psd_hdr *)Validation.get();
		psd->Dst = ((ipv6_hdr *)Buffer)->Dst;
		psd->Src = ((ipv6_hdr *)Buffer)->Src;
		psd->Length = htonl((uint32_t)Length);
		psd->Next_Header = (uint8_t)TransportLayer;

		memcpy(Validation.get() + sizeof(ipv6_psd_hdr), Buffer + sizeof(ipv6_hdr), Length);
		Result = GetChecksum((PUINT16)Validation.get(), sizeof(ipv6_psd_hdr) + Length);
	}
	else if (NetworkLayer == AF_INET && Length >= sizeof(ipv4_hdr)) //IPv4
	{
		std::shared_ptr<char> Validation(new char[sizeof(ipv4_psd_hdr) + Length - sizeof(ipv4_hdr)]());
		auto psd = (ipv4_psd_hdr *)Validation.get();
		psd->Dst = ((ipv4_hdr *)Buffer)->Dst;
		psd->Src = ((ipv4_hdr *)Buffer)->Src;
		psd->Length = htons((uint16_t)(Length - sizeof(ipv4_hdr)));
		psd->Protocol = (uint8_t)TransportLayer;

		memcpy(Validation.get() + sizeof(ipv4_psd_hdr), Buffer + sizeof(ipv4_hdr), Length - sizeof(ipv4_hdr));
		Result = GetChecksum((PUINT16)Validation.get(), sizeof(ipv4_psd_hdr) + Length - sizeof(ipv4_hdr));
	}

	return Result;
}

//Add length parameters to TCP DNS transmission
size_t __fastcall AddLengthToTCPDNSHead(PSTR Buffer, const size_t RecvLen, const size_t MaxLen)
{
	if (Buffer != nullptr && MaxLen >= RecvLen + sizeof(uint16_t))
	{
		memmove(Buffer + sizeof(uint16_t), Buffer, RecvLen);
		auto ptcp_dns_hdr = (tcp_dns_hdr *)Buffer;
		ptcp_dns_hdr->Length = htons((uint16_t)RecvLen);
		return RecvLen + sizeof(uint16_t);
	}
	
	return EXIT_FAILURE;
}

//Convert data from char(s) to DNS query
size_t __fastcall CharToDNSQuery(const PSTR FName, PSTR TName)
{
	int Index[] = {(int)strlen(FName) - 1, 0, 0};
	Index[2U] = Index[0] + 1;
	TName[Index[0] + 2] = 0;

	for (;Index[0] >= 0;Index[0]--,Index[2U]--)
	{
		if (FName[Index[0]] == 46)
		{
			TName[Index[2U]] = Index[1U];
			Index[1U] = 0;
		}
		else
		{
			TName[Index[2U]] = FName[Index[0]];
			Index[1U]++;
		}
	}
	TName[Index[2U]] = Index[1U];

	return strlen(TName) + 1U;
}

//Convert data from DNS query to char(s)
size_t __fastcall DNSQueryToChar(const PSTR TName, PSTR FName)
{
	size_t uIndex = 0;
	int Index[] = {0, 0};
	for (uIndex = 0;uIndex < DOMAIN_MAXSIZE;uIndex++)
	{
		if (uIndex == 0)
		{
			Index[0] = TName[uIndex];
		}
		else if (uIndex == Index[0] + Index[1U] + 1U)
		{
			Index[0] = TName[uIndex];
			if (Index[0] == 0)
				break;
			Index[1U] = (int)uIndex;

			FName[uIndex - 1U] = 46;
		}
		else {
			FName[uIndex - 1U] = TName[uIndex];
		}
	}

	return uIndex;
}

//Get local address(es)
bool __fastcall GetLocalAddress(sockaddr_storage &SockAddr, const uint16_t Protocol)
{
//Initialization
	std::shared_ptr<char> HostName(new char[DOMAIN_MAXSIZE]());
	addrinfo Hints = {0}, *Result = nullptr, *PTR = nullptr;

	if (Protocol == AF_INET6) //IPv6
		Hints.ai_family = AF_INET6;
	else //IPv4
		Hints.ai_family = AF_INET;
	Hints.ai_socktype = SOCK_DGRAM;
	Hints.ai_protocol = IPPROTO_UDP;

//Get localhost name.
	if (gethostname(HostName.get(), DOMAIN_MAXSIZE) == SOCKET_ERROR)
	{
		PrintError(WINSOCK_ERROR, L"Get localhost name error", WSAGetLastError(), nullptr, NULL);
		return false;
	}

//Get localhost data.
	SSIZE_T ResultGetaddrinfo = getaddrinfo(HostName.get(), NULL, &Hints, &Result);
	if (ResultGetaddrinfo != 0)
	{
		PrintError(WINSOCK_ERROR, L"Get localhost address(es) error", ResultGetaddrinfo, nullptr, NULL);

		freeaddrinfo(Result);
		return false;
	}
	HostName.reset();

//Report
	for (PTR = Result;PTR != nullptr;PTR = PTR->ai_next)
	{
	//IPv6
		if (PTR->ai_family == AF_INET6 && Protocol == AF_INET6 && 
			!IN6_IS_ADDR_LINKLOCAL((in6_addr *)(PTR->ai_addr)) &&
			!(((PSOCKADDR_IN6)(PTR->ai_addr))->sin6_scope_id == 0)) //Get port from first(Main) IPv6 device
		{
			SockAddr.ss_family = AF_INET6;
			((PSOCKADDR_IN6)&SockAddr)->sin6_addr = ((PSOCKADDR_IN6)(PTR->ai_addr))->sin6_addr;
			freeaddrinfo(Result);
			return true;
		}
	//IPv4
		else if (PTR->ai_family == AF_INET && Protocol == AF_INET && 
			((PSOCKADDR_IN)(PTR->ai_addr))->sin_addr.S_un.S_addr != INADDR_LOOPBACK && 
			((PSOCKADDR_IN)(PTR->ai_addr))->sin_addr.S_un.S_addr != INADDR_BROADCAST)
		{
			SockAddr.ss_family = AF_INET;
			((PSOCKADDR_IN)&SockAddr)->sin_addr = ((PSOCKADDR_IN)(PTR->ai_addr))->sin_addr;
			freeaddrinfo(Result);
			return true;
		}
	}

	freeaddrinfo(Result);
	return false;
}

//Make ramdom domains
void __fastcall RamdomDomain(PSTR Domain)
{
//Initialization
	std::random_device RamdomDevice;
	std::mt19937 RamdomEngine(RamdomDevice()); //Mersenne Twister Engine
	std::uniform_int_distribution<int> Distribution(0, 63); //Domain length is between 3 and 63(Labels must be 63 characters/bytes or less, Section 2.3.1 in RFC 1035).
	auto RamdomGenerator = std::bind(Distribution, RamdomEngine);

//Make ramdom domain length.
	size_t RamdomLength = RamdomGenerator(), Index = 0;
	if (RamdomLength < 4U)
		RamdomLength += 4U;

//Make ramdom domain.
	if (RamdomLength % 2U == 0)
	{
		for (Index = 0;Index < RamdomLength - 3U;Index++)
		{
			Domain[Index] = Parameter.DomainTable[RamdomGenerator()];
		//Convert to lowercase letters.
			if (Domain[Index] > 64 && Domain[Index] < 91)
				Domain[Index] += 32;
		}

	//Make random domain like a normal Top-level domain/TLD.
		Domain[RamdomLength - 3U] = 46;
		Index = RamdomGenerator();
		if (Index < 12U)
			Index += 52U;
		else if (Index < 38U)
			Index += 26U;
		Domain[RamdomLength - 2U] = Parameter.DomainTable[Index];
		Index = RamdomGenerator();
		if (Index < 12U)
			Index += 52U;
		else if (Index < 38U)
			Index += 26U;
		Domain[RamdomLength - 1U] = Parameter.DomainTable[Index];
	}
	else {
		for (Index = 0;Index < RamdomLength - 4U;Index++)
		{
			Domain[Index] = Parameter.DomainTable[RamdomGenerator()];
		//Convert to lowercase letters.
			if (Domain[Index] > 64 && Domain[Index] < 91)
				Domain[Index] += 32;
		}

	//Make random domain like a normal Top-level domain/TLD.
		Domain[RamdomLength - 4U] = 46;
		Index = RamdomGenerator();
		if (Index < 12U)
			Index += 52U;
		else if (Index < 38U)
			Index += 26U;
		Domain[RamdomLength - 3U] = Parameter.DomainTable[Index];
		Index = RamdomGenerator();
		if (Index < 12U)
			Index += 52U;
		else if (Index < 38U)
			Index += 26U;
		Domain[RamdomLength - 2U] = Parameter.DomainTable[Index];
		Index = RamdomGenerator();
		if (Index < 12U)
			Index += 52U;
		else if (Index < 38U)
			Index += 26U;
		Domain[RamdomLength - 1U] = Parameter.DomainTable[Index];
	}

	return;
}

//Check all DNS last result.
bool __fastcall CheckDNSLastResult(const PSTR Buffer, const size_t Length)
{
//DNS Responses which only have 1 Answer RR and no any Authority RRs or Additional RRs need to check.
	auto pdns_hdr = (dns_hdr *)Buffer;
	if (pdns_hdr->Questions == htons(0x0001) && /* pdns_hdr->Answer == htons(0x0001) && */ pdns_hdr->Authority == 0)
	{
		auto pdns_qry = (dns_qry *)(Buffer + sizeof(dns_hdr) + strlen(Buffer + sizeof(dns_hdr)) + 1U);
		if (pdns_qry->Classes == htons(DNS_CLASS_IN)) //Class IN
		{
		//AAAA Records
			if (pdns_qry->Type == htons(DNS_AAAA_RECORDS))
			{
				dns_aaaa_record *DNS_AAAA = nullptr;
				if (pdns_hdr->Additional == 0) //No any Additional records.
					DNS_AAAA = (dns_aaaa_record *)(Buffer + (Length - sizeof(dns_aaaa_record)));
				else //EDNS0 Label
					DNS_AAAA = (dns_aaaa_record *)(Buffer + (Length - sizeof(dns_edns0_label) - sizeof(dns_aaaa_record)));

			//Record(s) Type in responses check
				if (Parameter.DNSDataCheck && DNS_AAAA->Type != pdns_qry->Type)
					return false;

			//Fake responses check
				if (Parameter.Blacklist)
				{
					std::shared_ptr<char> Domain(new char[DOMAIN_MAXSIZE]());
					DNSQueryToChar(Buffer + sizeof(dns_hdr), Domain.get());

					if (DNS_AAAA->Type == htons(DNS_AAAA_RECORDS) && DNS_AAAA->Classes == htons(DNS_CLASS_IN) && 
						DNS_AAAA->TTL != 0 && DNS_AAAA->Length == htons(sizeof(in6_addr)) && 
					//Extended fake responses check
						CheckSpecialAddress(&DNS_AAAA->Addr, AF_INET6, Domain.get()))
							return false;
				}
			}
		//A Records or other records.
			else {
				dns_a_record *DNS_A = nullptr;
				if (pdns_hdr->Additional == 0) //No any Additional records.
					DNS_A = (dns_a_record *)(Buffer + (Length - sizeof(dns_a_record)));
				else //EDNS0 Label
					DNS_A = (dns_a_record *)(Buffer + (Length - sizeof(dns_edns0_label) - sizeof(dns_a_record)));

			//Record(s) Type in responses check
				if (Parameter.DNSDataCheck && pdns_qry->Type == htons(DNS_A_RECORDS) && DNS_A->Type != pdns_qry->Type) //A Records
					return false;

			//Fake responses check
				if (Parameter.Blacklist)
				{
					std::shared_ptr<char> Domain(new char[DOMAIN_MAXSIZE]());
					DNSQueryToChar(Buffer + sizeof(dns_hdr), Domain.get());

					if (DNS_A->Type == htons(DNS_A_RECORDS) && DNS_A->Classes == htons(DNS_CLASS_IN) && 
						DNS_A->TTL != 0 && DNS_A->Length == htons(sizeof(in_addr)) && 
					//Extended fake responses check
						CheckSpecialAddress(&DNS_A->Addr, AF_INET, Domain.get()))
							return false;
				}
			}
		}
	}

	return true;
}
