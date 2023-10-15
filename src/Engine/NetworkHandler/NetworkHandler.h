#ifndef NETWORKHANDLER_H
#define NETWORKHANDLER_H

#include "cbase.h"
#include <enet/enet.h>

//we need leaderboards, i refuse to not have them, i NEED them

class NetworkHandler {
public:
	struct EXTENSION_PACKET {
		uint32_t size;
		std::shared_ptr<void> data;
	};
public:
	NetworkHandler();
	~NetworkHandler();

	UString httpGet(UString url, long timeout = 5, long connectTimeout = 5);
	std::string httpDownload(UString url, long timeout = 60, long connectTimeout = 5);

	typedef fastdelegate::FastDelegate0<> NetworkLocalServerStartedListener;
	typedef fastdelegate::FastDelegate0<> NetworkLocalServerStoppedListener;

	typedef fastdelegate::FastDelegate0<> NetworkClientDisconnectedFromServerListener;
	typedef fastdelegate::FastDelegate0<> NetworkClientConnectedToServerListener;

	typedef fastdelegate::FastDelegate0<EXTENSION_PACKET> NetworkClientSendInfoListener;
	typedef fastdelegate::FastDelegate0<EXTENSION_PACKET> NetworkServerSendInfoListener;

	typedef fastdelegate::FastDelegate1<void*, bool> NetworkClientReceiveServerInfoListener;
	typedef fastdelegate::FastDelegate1<void*, bool> NetworkServerReceiveClientInfoListener;

	typedef fastdelegate::FastDelegate3<uint32_t, void*, uint32_t, bool> NetworkServerReceiveClientPacketListener;
	typedef fastdelegate::FastDelegate3<uint32_t, void*, uint32_t, bool> NetworkClientReceiveServerPacketListener;
	typedef fastdelegate::FastDelegate3<uint32_t, UString, bool> NetworkServerClientChangeListener;

	void update();

	void host();
	void hostStop();
	void status();

	void connect(UString address);
	void disconnect();

	void say(UString message);
	void kick(UString username);

	void broadcast(void* data, uint32_t size, bool reliable = false);
	void servercast(void* data, uint32_t size, bool reliable = false);
	void clientcast(void* data, uint32_t size, uint32_t id, bool reliable = false);

	void setOnClientReceiveServerPacketListener(NetworkClientReceiveServerPacketListener listener) { m_clientReceiveServerPacketListener = listener; }
	void setOnServerReceiveClientPacketListener(NetworkServerReceiveClientPacketListener listener) { m_serverReceiveClientPacketListener = listener; }
	void setOnServerClientChangeListener(NetworkServerClientChangeListener listener) { m_serverClientChangeListener = listener; }

	void setOnClientSendInfoListener(NetworkClientSendInfoListener listener) { m_clientSendInfoListener = listener; }
	void setOnServerSendInfoListener(NetworkServerSendInfoListener listener) { m_serverSendInfoListener = listener; }

	void setOnClientReceiveServerInfoListener(NetworkClientReceiveServerInfoListener listener) { m_clientReceiveServerInfoListener = listener; }
	void setOnServerReceiveClientInfoListener(NetworkServerReceiveClientInfoListener listener) { m_serverReceiveClientInfoListener = listener; }

	void setOnClientConnectedToServerListener(NetworkClientConnectedToServerListener listener) { m_clientConnectedToServerListener = listener; }
	void setOnClientDisconnectedFromServerListener(NetworkClientDisconnectedFromServerListener listener) { m_clientDisconnectedFromServerListener = listener; }

	void setOnLocalServerStartedListener(NetworkLocalServerStartedListener listener) { m_localServerStartedListener = listener; }
	void setOnLocalServerStoppedListener(NetworkLocalServerStoppedListener listener) { m_localServerStoppedListener = listener; }

	int getPing() const;

	inline uint32_t getLocalClientID() const { return m_iLocalClientID; }
	inline UString getServerAddress() const { return m_sServerAddress; }

	bool isClient() const;
	bool isServer() const;


private:
	static size_t curlStringWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
	static size_t curlStringStreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

	void onClientEvent(ENetEvent e);
	void onServerEvent(ENetEvent e);

	enum PACKET_TYPE {
		CLIENT_INFO_PACKET_TYPE,
		SERVER_INFO_PACKET_TYPE,
		CHAT_PACKET_TYPE,
		CLIENT_PACKET_TYPE,
		CLIENT_BROADCAST_PACKET_TYPE,
		CLIENT_CONNECTION_PACKET_TYPE
	};

	struct CHAT_PACKET;
	struct CLIENT_INFO_PACKET;
	struct CLIENT_PEER;

	void sendClientInfo();
	void clientDisconnect();

	void sendServerInfo(uint32_t assignedID, ENetHost* host, ENetPeer* destination);

	void singlecastChatMessage(CHAT_PACKET* cp, ENetHost* host, ENetPeer* destination);
	void singlecastChatMessage(UString username, UString message, ENetHost* host, ENetPeer* destination);
	void broadcastChatMessage(CHAT_PACKET* cp, ENetHost* host, ENetPeer* origin);
	void broadcastChatMessage(UString username, UString message, ENetHost* host, ENetPeer* origin);

	void chatLog(UString username, UString message);

	CLIENT_PEER* getClientPeerByPeer(ENetPeer* peer);
	CLIENT_PEER* getClientPeerById(uint32_t id);

	bool m_bReady;
	float m_fDebugNetworkTime;
	std::string curlReadBuffer;

	uint32_t m_iLocalClientID;
	UString m_sServerAddress;

	ENetHost* m_client;
	ENetPeer* m_clientPeer;

	bool m_bClientConnectPending;
	float m_fClientConnectPendingTime;
	bool m_bClientConnectPendingAfterDisconnect;

	ENetHost* m_server;
	std::vector<CLIENT_PEER> m_vConnectedClients;

	bool m_bClientDisconnectPending;
	float m_fClientDisconnectPendingTime;

	NetworkLocalServerStartedListener m_localServerStartedListener;
	NetworkLocalServerStoppedListener m_localServerStoppedListener;

	NetworkClientConnectedToServerListener m_clientConnectedToServerListener;
	NetworkClientDisconnectedFromServerListener m_clientDisconnectedFromServerListener;

	NetworkClientSendInfoListener m_clientSendInfoListener;
	NetworkServerSendInfoListener m_serverSendInfoListener;

	NetworkClientReceiveServerInfoListener m_clientReceiveServerInfoListener;
	NetworkServerReceiveClientInfoListener m_serverReceiveClientInfoListener;

	NetworkClientReceiveServerPacketListener m_clientReceiveServerPacketListener;
	NetworkServerReceiveClientPacketListener m_serverReceiveClientPacketListener;
	NetworkServerClientChangeListener m_serverClientChangeListener;

	struct CLIENT_PEER {
		ENetPeer* peer;
		UString name;
		float kickTime;
		float kickKillTime;
		uint32_t id;
	};

	unsigned int m_iIDCounter;

#pragma pack(1)
	struct CLIENT_BROADCAST_WRAPPER {
		PACKET_TYPE ntype = CLIENT_BROADCAST_PACKET_TYPE;
		uint32_t id;
	};

	struct SERVER_INFO_PACKET {
		PACKET_TYPE type = SERVER_INFO_PACKET_TYPE;
		uint32_t id;
		bool extension;
	};

	struct CLIENT_INFO_PACKET {
		PACKET_TYPE type = CLIENT_INFO_PACKET_TYPE;
		uint32_t version;
		uint32_t size;
		wchar_t username[255];
		bool extension;
	};

	struct CHAT_PACKET {
		PACKET_TYPE type = CHAT_PACKET_TYPE;
		uint32_t usize;
		uint32_t msize;
		wchar_t username[255];
		wchar_t message[255];
	};

#pragma pack()
};
#endif // !NETWORKHANDLER_H
