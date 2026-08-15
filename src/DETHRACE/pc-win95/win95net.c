#include "pd/net.h"

#include "brender.h"
#include "dr_types.h"
#include "errors.h"
#include "globvrpb.h"
#include "harness/config.h"
#include "harness/hooks.h"
#include "harness/trace.h"
#include "harness/winsock.h"
#include "network.h"
#include "pd/net.h"
#include "pd/sys.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// dethrace: have switched out IPX implementation for IP

tU32 gNetwork_init_flags;
tPD_net_game_info* gJoinable_games;
int gMatts_PC;
tU16 gSocket_number_pd_format;
//_IPX_ELEMENT gListen_elements[16];
char gLocal_ipx_addr_string[32];
//_IPX_ELEMENT gSend_elements[16];
struct sockaddr_in* gLocal_addr_ipx;
char gReceive_buffer[512];
tPD_net_player_info gRemote_net_player_info;
struct sockaddr_in* gBroadcast_addr_ipx;
tPD_net_player_info gLocal_net_player_info;
char gSend_buffer[512];
tIPX_netnum gNetworks[16];
struct sockaddr_in* gRemote_addr_ipx;
tU8* gSend_packet;
// W32 gListen_segment;
tU8* gSend_packet_ptr;
// W32 gSend_segment;
tU8* gListen_packet;
tU8* gListen_packet_ptr;
size_t gMsg_header_strlen;
int gNumber_of_networks;
int gNumber_of_hosts;
//_IPX_HEADER gLast_received_IPX_header;
tU16 gSocket_number_network_order;
// unsigned short gECB_offset;
tU16 gListen_selector;
tU16 gSend_selector;

struct sockaddr_in gLocal_addr;
struct sockaddr_in gRemote_addr;
struct sockaddr_in gBroadcast_addr;
struct sockaddr_in gLast_received_addr;
int gSocket;

#define MESSAGE_HEADER_STR "CW95MSG"
#define JOINABLE_GAMES_CAPACITY 16

DR_STATIC_ASSERT(offsetof(tNet_message, pd_stuff_so_DO_NOT_USE) == 0);
DR_STATIC_ASSERT(offsetof(tNet_message, magic_number) == 4);
DR_STATIC_ASSERT(offsetof(tNet_message, guarantee_number) == 8);
DR_STATIC_ASSERT(offsetof(tNet_message, sender) == 12);
DR_STATIC_ASSERT(offsetof(tNet_message, version) == 16);
DR_STATIC_ASSERT(offsetof(tNet_message, senders_time_stamp) == 20);
DR_STATIC_ASSERT(offsetof(tNet_message, num_contents) == 24);
DR_STATIC_ASSERT(offsetof(tNet_message, overall_size) == 26);
DR_STATIC_ASSERT(offsetof(tNet_message, contents) == 28);

// IDA: void __cdecl ClearupPDNetworkStuff()
void ClearupPDNetworkStuff(void) {
    // Stubbed
}

// IDA: void __usercall MATTMessageCheck(char *pFunction_name@<EAX>, tNet_message *pMessage@<EDX>, int pAlleged_size@<EBX>)
void MATTMessageCheck(char* pFunction_name, tNet_message* pMessage, int pAlleged_size) {
    // Stubbed
}

// IDA: int __usercall GetProfileText@<EAX>(char *pDest@<EAX>, int pDest_len@<EDX>, char *pFname@<EBX>, char *pKeyname@<ECX>)
int GetProfileText(char* pDest, int pDest_len, char* pFname, char* pKeyname) {
    // Stubbed
}

// IDA: int __cdecl GetSocketNumberFromProfileFile()
int GetSocketNumberFromProfileFile(void) {
    // Stubbed
}

void NetNowIPXLocalTarget2String(char* pString, struct sockaddr_in* pSock_addr_ipx) {
    // Stubbed
}

// IDA: int __usercall GetMessageTypeFromMessage@<EAX>(char *pMessage_str@<EAX>)
int GetMessageTypeFromMessage(char* pMessage_str) {
    // Stubbed
}

int SameEthernetAddress(struct sockaddr_in* pAddr_ipx1, struct sockaddr_in* pAddr_ipx2) {
    // Stubbed
}

/*SOCKADDR_IPX_* */ void* GetIPXAddrFromPlayerID(tPlayer_ID pPlayer_id) {
    // Stubbed
}

// IDA: void __usercall MakeMessageToSend(int pMessage_type@<EAX>)
void MakeMessageToSend(int pMessage_type) {
    // Stubbed
}

// IDA: int __cdecl ReceiveHostResponses()
int ReceiveHostResponses(void) {
    // Stubbed
}

// IDA: int __cdecl BroadcastMessage()
int BroadcastMessage(void) {
    // Stubbed
}


// IDA: int __cdecl PDNetInitialise()
int PDNetInitialise(void) {
    // Stubbed
}

// IDA: int __cdecl PDNetShutdown()
int PDNetShutdown(void) {
    // Stubbed
}

// IDA: void __cdecl PDNetStartProducingJoinList()
void PDNetStartProducingJoinList(void) {
    // Stubbed
}


// IDA: void __cdecl PDNetEndJoinList()
void PDNetEndJoinList(void) {
    // Stubbed
}

// IDA: int __usercall PDNetGetNextJoinGame@<EAX>(tNet_game_details *pGame@<EAX>, int pIndex@<EDX>)
int PDNetGetNextJoinGame(tNet_game_details* pGame, int pIndex) {
    // Stubbed
}

// IDA: void __usercall PDNetDisposeGameDetails(tNet_game_details *pDetails@<EAX>)
void PDNetDisposeGameDetails(tNet_game_details* pDetails) {
    // Stubbed
}

// IDA: int __usercall PDNetHostGame@<EAX>(tNet_game_details *pDetails@<EAX>, char *pHost_name@<EDX>, void **pHost_address@<EBX>)
int PDNetHostGame(tNet_game_details* pDetails, char* pHost_name, void** pHost_address) {
    // Stubbed
}

// IDA: int __usercall PDNetJoinGame@<EAX>(tNet_game_details *pDetails@<EAX>, char *pPlayer_name@<EDX>)
int PDNetJoinGame(tNet_game_details* pDetails, char* pPlayer_name) {
    // Stubbed
}

// IDA: void __usercall PDNetLeaveGame(tNet_game_details *pDetails@<EAX>)
void PDNetLeaveGame(tNet_game_details* pDetails) {
    // Stubbed
}

// IDA: void __usercall PDNetHostFinishGame(tNet_game_details *pDetails@<EAX>)
void PDNetHostFinishGame(tNet_game_details* pDetails) {
    // Stubbed
}

// IDA: tU32 __usercall PDNetExtractGameID@<EAX>(tNet_game_details *pDetails@<EAX>)
tU32 PDNetExtractGameID(tNet_game_details* pDetails) {
    // Stubbed
}

// IDA: tPlayer_ID __usercall PDNetExtractPlayerID@<EAX>(tNet_game_details *pDetails@<EAX>)
tPlayer_ID PDNetExtractPlayerID(tNet_game_details* pDetails) {
    // Stubbed
}

// IDA: void __usercall PDNetObtainSystemUserName(char *pName@<EAX>, int pMax_length@<EDX>)
void PDNetObtainSystemUserName(char* pName, int pMax_length) {
    // Stubbed
}


// IDA: int __usercall PDNetSendMessageToPlayer@<EAX>(tNet_game_details *pDetails@<EAX>, tNet_message *pMessage@<EDX>, tPlayer_ID pPlayer@<EBX>)
int PDNetSendMessageToPlayer(tNet_game_details* pDetails, tNet_message* pMessage, tPlayer_ID pPlayer) {
    // Stubbed
}

// IDA: int __usercall PDNetSendMessageToAllPlayers@<EAX>(tNet_game_details *pDetails@<EAX>, tNet_message *pMessage@<EDX>)
int PDNetSendMessageToAllPlayers(tNet_game_details* pDetails, tNet_message* pMessage) {
    // Stubbed
}

// IDA: tNet_message* __usercall PDNetGetNextMessage@<EAX>(tNet_game_details *pDetails@<EAX>, void **pSender_address@<EDX>)
tNet_message* PDNetGetNextMessage(tNet_game_details* pDetails, void** pSender_address) {
    // Stubbed
}

// IDA: tNet_message* __usercall PDNetAllocateMessage@<EAX>(tU32 pSize@<EAX>, tS32 pSize_decider@<EDX>)
tNet_message* PDNetAllocateMessage(tU32 pSize, tS32 pSize_decider) {
    // Stubbed
}

// IDA: void __usercall PDNetDisposeMessage(tNet_game_details *pDetails@<EAX>, tNet_message *pMessage@<EDX>)
void PDNetDisposeMessage(tNet_game_details* pDetails, tNet_message* pMessage) {
    // Stubbed
}

// IDA: void __usercall PDNetSetPlayerSystemInfo(tNet_game_player_info *pPlayer@<EAX>, void *pSender_address@<EDX>)
void PDNetSetPlayerSystemInfo(tNet_game_player_info* pPlayer, void* pSender_address) {
    // Stubbed
}

// IDA: void __usercall PDNetDisposePlayer(tNet_game_player_info *pPlayer@<EAX>)
void PDNetDisposePlayer(tNet_game_player_info* pPlayer) {
    // Stubbed
}

// IDA: int __usercall PDNetSendMessageToAddress@<EAX>(tNet_game_details *pDetails@<EAX>, tNet_message *pMessage@<EDX>, void *pAddress@<EBX>)
int PDNetSendMessageToAddress(tNet_game_details* pDetails, tNet_message* pMessage, void* pAddress) {
    // Stubbed
}

// IDA: int __usercall PDNetInitClient@<EAX>(tNet_game_details *pDetails@<EAX>)
int PDNetInitClient(tNet_game_details* pDetails) {
    // Stubbed
}

// IDA: int __cdecl PDNetGetHeaderSize()
int PDNetGetHeaderSize(void) {
    // Stubbed
}
