// bfme-native: link stubs for builds without GeneralsOnline (SAGE_USE_NGMP=OFF).
// The WOL menus reference these symbols unconditionally on Linux; they are only reached
// from the online lobby, which a non-NGMP build never opens.
#include "PreRTS.h"
#include "GameNetwork/GeneralsOnline/NGMPGame.h"
#include "GameNetwork/GeneralsOnline/OnlineServices_Manager.h"
#include "GameNetwork/GeneralsOnline/OnlineServices_LobbyInterface.h"
#include "GameNetwork/GeneralsOnline/OnlineServices_RoomsInterface.h"
#include <cstdlib>

NGMPGame* TheNGMPGame = nullptr;

static void ngmpUnavailable(const char* what)
{
	DEBUG_CRASH(("GeneralsOnline is not built into this executable (%s)", what));
	fprintf(stderr, "GeneralsOnline is not built into this executable (%s)\n", what);
	std::abort();
}

NGMPGameSlot* NGMPGame::getGameSpySlot(Int) { ngmpUnavailable("NGMPGame::getGameSpySlot"); return nullptr; }
NGMP_OnlineServicesManager& NGMP_OnlineServicesManager::getInstance()
{
	ngmpUnavailable("NGMP_OnlineServicesManager::getInstance");
	return *static_cast<NGMP_OnlineServicesManager*>(nullptr);
}
void NGMP_OnlineServices_LobbyInterface::LeaveCurrentLobby() {}
void NGMP_OnlineServices_LobbyInterface::UpdateCurrentLobby_Map(AsciiString, AsciiString, bool, int) {}
void NGMP_OnlineServices_RoomsInterface::SendChatMessageToCurrentRoom(UnicodeString&, bool) {}
void WebSocket::SendData_ChangeName(UnicodeString&) {}
