#include "NetworkHandler.h"

#include "Engine.h"
#include "ConVar/ConVar.h"

#include <sstream>

#include <curl.h>

#define TC_PROTOCOL_VERSION 1
#define TC_PROTOCOL_TIMEOUT 10000

ConVar _name_("name", "Tacotakedown");

ConVar _connect_("connect");
ConVar connect_duration("connect_duration", 5.0f, "Time in seconds to wait for a response from the server when trying to connect");
ConVar _disconnect_("disconnect");
ConVar disconnect_duration("disconnect_duration", 3.0f, "Time in seconds to wait for a gentle disconnect before dropping the connection");

ConVar _host_("host");
ConVar _stop_("stop");
ConVar host_port("host_port", 7777.0f);
ConVar host_max_clients("host_max_clients", 16.0f);
ConVar _status_("status");

ConVar debug_network("debug_network", false);
ConVar debug_network_time("debug_network_time", false);

ConVar _name_admin_("name_admin", "ADMIN");
ConVar _say_("say");
ConVar _kick_("kick");

ConVar cl_cmdrate("cl_cmdrate", 66.0f, "How many client update packets are sent to the server per second");
ConVar cl_updaterate("cl_updaterate", 66.0f, "How many snapshots/updates/deltas are requested from the server per second");

NetworkHandler::NetworkHandler() {
	m_bReady = false;
	m_fDebugNetworkTime = 0.0f;
	m_iLocalClientID = -1;

	if (enet_initialize() != 0) {
		engine->showMessageError("ENet Error", "Couldn't enet_initialize()");
		return;
	}
	if (curl_global_init(CURL_GLOBAL_WIN32 | CURL_GLOBAL_SSL) != CURLE_OK) {
		engine->showMessageError("CURL Error", "Couldn't curl_global_init()!");
		return;
	}

	_host_.setCallback(fastdelegate::MakeDelegate(this, &NetworkHandler::host));
	_stop_.setCallback(fastdelegate::MakeDelegate(this, &NetworkHandler::hostStop));
	_status_.setCallback(fastdelegate::MakeDelegate(this, &NetworkHandler::status));
	_connect_.setCallback(fastdelegate::MakeDelegate(this, &NetworkHandler::connect));
	_disconnect_.setCallback(fastdelegate::MakeDelegate(this, &NetworkHandler::disconnect));

	_say_.setCallback(fastdelegate::MakeDelegate(this, &NetworkHandler::say));
	_kick_.setCallback(fastdelegate::MakeDelegate(this, &NetworkHandler::kick));

	m_client = NULL;
	m_server = NULL;
	m_clientPeer = NULL;

	m_bClientConnectPending = false;
	m_fClientConnectPendingTime = 0.0f;
	m_bClientDisconnectPending = false;
	m_fClientDisconnectPendingTime = 0.0f;
	m_bClientConnectPendingAfterDisconnect = false;

	m_localServerStartedListener = NULL;
	m_localServerStoppedListener = NULL;

	m_clientDisconnectedFromServerListener = NULL;
	m_clientConnectedToServerListener = NULL;

	m_clientSendInfoListener = NULL;
	m_serverSendInfoListener = NULL;

	m_clientReceiveServerInfoListener = NULL;
	m_serverReceiveClientInfoListener = NULL;

	m_clientReceiveServerPacketListener = NULL;
	m_serverReceiveClientPacketListener = NULL;
	m_serverClientChangeListener = NULL;

	m_iIDCounter = 1;	// WARNING: id 0 is reserved! (for the host client entity, = Admin)

	m_bReady = true;
}

NetworkHandler::~NetworkHandler() {
	if (m_clientPeer != NULL)
		enet_peer_reset(m_clientPeer);
	if (m_client != NULL)
		enet_host_destroy(m_client);
	if (isServer())
		enet_host_destroy(m_server);

	if (m_bReady) {
		enet_deinitialize();
		curl_global_cleanup();
	}

	m_bReady = false;
}

UString NetworkHandler::httpGet(UString url, long timeout, long connectTimeout) {
	CURL* curl = curl_easy_init();

	if (curl != NULL) {
		curl_easy_setopt(curl, CURLOPT_URL, url.toUtf8());
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "User-Agent: TacoEngine");
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeout);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlStringWriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curlReadBuffer);

		/*CURLcode res = */curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return UString(curlReadBuffer.c_str());
	}
	else {
		debugLog("NetworkHandler::httpGet() error, curl == NULL!\n");
		return "";
	}
}

std::string NetworkHandler::httpDownload(UString url, long timeout, long connectTimeout) {
	CURL* curl = curl_easy_init();

	if (curl != NULL) {
		curl_easy_setopt(curl, CURLOPT_URL, url.toUtf8());
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "User-Agent: TacoEngine");
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeout);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "deflate");

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

		std::stringstream curlWriteBuffer(std::stringstream::in | std::stringstream::out | std::stringstream::binary);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlStringStreamWriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curlWriteBuffer);

		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			curlWriteBuffer = std::stringstream();
			debugLog("NetworkHandler::httpDownload() error, code %i!\n", (int)res);
		}
		curl_easy_cleanup(curl);

		return curlWriteBuffer.str();
	}
	else {
		debugLog("NetworkHandler::httpDownload() error, curl == NULL!\n");
		return std::string("");
	}
}

void NetworkHandler::connect(UString address) {
	if (!m_bReady) return;

	if (address.length() < 1) {
		address = "localhost";
	}
	if (m_clientPeer != NULL) {
		m_sServerAddress = address;
		m_bClientConnectPendingAfterDisconnect = true;
		disconnect();
		return;
	}
	if (m_client != NULL)
		enet_host_destroy(m_client);

	m_client = NULL;
	m_clientPeer = NULL;

	m_client = enet_host_create(NULL, 1, 2, 0, 0);

	if (m_client == NULL) {
		debugLog(0xffff0000, "CLIENT: An error occurred while trying to create the client.\n");
		return;
	}

	ENetAddress adr;

	enet_address_set_host(&adr, address.toUtf8());
	adr.port = host_port.getInt();

	m_clientPeer = enet_host_connect(m_client, &adr, 2, 0);

	if (m_clientPeer == NULL) {
		debugLog(0xffffff00, "CLIENT: No free slot available.\n");
		return;
	}
	m_bClientConnectPending = true;
	m_fClientConnectPendingTime = engine->getTime() + connect_duration.getFloat();

	m_sServerAddress = address;
	debugLog("CLIENT: Trying to connect to \"%ls\" ... (%.1f seconds(s) timeout)\n", address.length() == 0 ? L"localhost" : address.wc_str(), connect_duration.getFloat());
}

void NetworkHandler::disconnect() {
	if (!m_bReady) return;

	if (m_clientPeer = NULL) {
		debugLog("CLIENT: Not connected.\n");
		return;
	}

	enet_peer_disconnect(m_clientPeer, 0);
	m_bClientDisconnectPending = true;
	m_fClientDisconnectPendingTime = engine->getTime() + disconnect_duration.getFloat();

	debugLog("CLIENT: Trying to gently disconnect... (%.1f second(s) timeout)\n", disconnect_duration.getFloat());
}

void NetworkHandler::host() {
	if (!m_bReady) return;

	m_vConnectedClients.clear();

	if (isServer())
		hostStop();

	debugLog("SERVER: Starting local server on port %i...\n", host_port.getInt());

	ENetAddress address;
	address.host = ENET_HOST_ANY;
	address.port = host_port.getInt();

	m_server = enet_host_create(&address, host_max_clients.getInt(), 2, 0, 0);

	if (m_server = NULL) {
		debugLog(0xffff0000, "SERVER: An error occurred while trying to create the server.\n");
		return;
	}

	debugLog(0xff00ff00, "SERVER: Local server is up and running.\n");

	if (m_localServerStartedListener != NULL)
		m_localServerStartedListener();

}

void NetworkHandler::hostStop() {
	if (m_server == NULL) {
		debugLog("SERVER: Not running.\n");
		return;
	}

	if (isClient())
		clientDisconnect();

	if (m_localServerStoppedListener != NULL)
		m_localServerStoppedListener();

	debugLog("SERVER: Stopped local server.\n");

	enet_host_destroy(m_server);

	m_server = NULL;
}

void NetworkHandler::status() {
	if (isServer()) {
		debugLog("\n");
		debugLog("version: %i\n", TC_PROTOCOL_VERSION);

		debugLog("hostname: <TODO>\n");

		char host_attr[255];
		enet_address_get_host_ip(&m_server->receivedAddress, host_attr, 255);

		debugLog("udp/ip: %s:%i\n", host_attr, host_port.getInt());
		for (size_t c = 0; c < m_vConnectedClients.size(); c++) {
			debugLog("# %ls %i", m_vConnectedClients[c].name.wc_str(), m_vConnectedClients[c].peer->roundTripTime);
		}
		debugLog("\n");
		return;
	}
	if (isClient()) {
		debugLog("\n");
		debugLog("version: %i\n", TC_PROTOCOL_VERSION);

		debugLog("hostname: <TODO>\n");
		debugLog("udp/ip: %ls:%i\n", m_sServerAddress.length() == 0 ? L"localhost" : m_sServerAddress.wc_str(), host_port.getInt());
		debugLog("ping: %i\n", m_clientPeer->roundTripTime);
		debugLog("\n");
	}
	else {
		debugLog(" Not connected to any server.\n");
	}
}

void NetworkHandler::update() {
	if (!m_bReady) return;

	if (isClient()) {
		ENetEvent event;
		while (enet_host_service(m_client, &event, 0) > 0) {
			onClientEvent(event);
		}
	}
	else if (m_bClientConnectPendingAfterDisconnect) {
		m_bClientConnectPendingAfterDisconnect = false;
		connect(m_sServerAddress);
	}

	if (isServer()) {
		ENetEvent event;
		while (enet_host_service(m_server, &event, 0) > 0) {
			onServerEvent(event);
		}

		if (m_vConnectedClients.size() > 0) {
			for (size_t c = 0; c < m_vConnectedClients.size(); c++) {
				if ((m_server->serviceTime - m_vConnectedClients[c].peer->lastReceiveTime) > TC_PROTOCOL_TIMEOUT) {
					debugLog("SERVER: %s timed out.\n", m_vConnectedClients[c].name.toUtf8());

					if (m_serverClientChangeListener != NULL)
						m_serverClientChangeListener(m_vConnectedClients[c].id, m_vConnectedClients[c].name, false);

					enet_peer_reset(m_vConnectedClients[c].peer);

					m_vConnectedClients.erase(m_vConnectedClients.begin() + c);
					c--;

					continue;
				}
				if (m_vConnectedClients[c].kickTime != 0.0f && engine->getTime() > m_vConnectedClients[c].kickTime + 1.0f) {
					debugLog("SERVER: %s kicked.\n", m_vConnectedClients[c].name.toUtf8());
					enet_peer_disconnect(m_vConnectedClients[c].peer, 0);

					m_vConnectedClients[c].kickTime = 0.0f;
					m_vConnectedClients[c].kickKillTime = engine->getTime();
				}
				if (m_vConnectedClients[c].kickKillTime != 0.0f && engine->getTime() > m_vConnectedClients[c].kickKillTime + 1.0f) {
					debugLog("SERVER: %s forcefully disconnected.\n", m_vConnectedClients[c].name.toUtf8());

					// TODO: this call was not here originally, check if it causes redundant/odd behavior
					// notify local connection listener
					if (m_serverClientChangeListener != NULL)
						m_serverClientChangeListener(m_vConnectedClients[c].id, m_vConnectedClients[c].name, false);

					enet_peer_reset(m_vConnectedClients[c].peer);

					m_vConnectedClients.erase(m_vConnectedClients.begin() + c);
					c--;

					continue;
				}

			}
		}
	}
	if ((m_bClientDisconnectPending && engine->getTime() > m_fClientDisconnectPendingTime) || (m_bClientConnectPending && engine->getTime() > m_fClientConnectPendingTime))
		clientDisconnect();

	if (debug_network_time.getBool() && engine->getTime() > m_fDebugNetworkTime) {
		m_fDebugNetworkTime = engine->getTime() + 0.5f;
		if (isClient())
			debugLog("client time = %u\n", enet_time_get());
		else if (isServer())
			debugLog("server time = %u\n", enet_time_get());
	}
}

size_t NetworkHandler::curlStringWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

size_t NetworkHandler::curlStringStreamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
	((std::stringstream*)userp)->write((const char*)contents, (size_t)size * nmemb);
	return size * nmemb;
}

void NetworkHandler::onClientEvent(ENetEvent e) {
	switch (e.type) {
	case ENET_EVENT_TYPE_RECEIVE:
		if (debug_network.getBool())
			debugLog("CLIENT: A packet of length %u was received from %s on channel %u.\n", e.packet->dataLength, e.peer->data, e.channelID);

		if (e.packet->data != NULL) {
			switch (*((PACKET_TYPE*)e.packet->data)) {
			case SERVER_INFO_PACKET_TYPE:
				if (e.packet->dataLength >= sizeof(SERVER_INFO_PACKET)) {
					SERVER_INFO_PACKET* sp = (SERVER_INFO_PACKET*)e.packet->data;
					m_iLocalClientID = sp->id;

					debugLog("CLIENT: Received server info (%i)\n", sp->id);

					bool valid = true;
					if (m_clientReceiveServerInfoListener != NULL && sp->extension)
						valid = m_clientReceiveServerInfoListener((void*)(e.packet->data + sizeof(SERVER_INFO_PACKET)));

					if (valid) {
						if (m_clientConnectedToServerListener != NULL)
							m_clientConnectedToServerListener();
					}
					else
						disconnect();
				}
				break;

			case CHAT_PACKET_TYPE:
				if (e.packet->dataLength >= sizeof(CHAT_PACKET)) {
					if (!isServer()) {
						chatLog(UString(((struct CHAT_PACKET*)e.packet->data)->username).substr(0, ((struct CHAT_PACKET*)e.packet->data)->usize),
							UString(((struct CHAT_PACKET*)e.packet->data)->message).substr(0, ((struct CHAT_PACKET*)e.packet->data)->msize));
					}
				}
				break;

			case CLIENT_BROADCAST_PACKET_TYPE:
				if (e.packet->dataLength >= sizeof(CLIENT_BROADCAST_WRAPPER)) {
					if (m_clientReceiveServerPacketListener != NULL) {
						CLIENT_BROADCAST_WRAPPER* wrapper = (CLIENT_BROADCAST_WRAPPER*)e.packet->data;

						char* unwrappedPacket = (char*)e.packet->data;
						const int wrapperSize = sizeof(CLIENT_BROADCAST_WRAPPER);
						unwrappedPacket += wrapperSize;

						if (!m_clientReceiveServerPacketListener(wrapper->id, unwrappedPacket, e.packet->dataLength - wrapperSize))
							debugLog("CLIENT: Received unknown CLIENT_PACKET_TYPE, WTF!\n");
					}
				}
				break;

			default:
				debugLog("CLIENT: Received unknown packet of type %i, WTF!\n", *((PACKET_TYPE*)e.packet->data));
				break;
			}
		}

		enet_packet_destroy(e.packet);
		break;

	case ENET_EVENT_TYPE_CONNECT:
		if (m_bClientConnectPending)
			debugLog(0xff00ff00, "CLIENT: Connected.\n");
		else
			debugLog(0xff00ff00, "CLIENT: Connected, but without a pending connection attempt, WTF!\n");

		m_bClientConnectPending = false;

		sendClientInfo();

		debugLog("CLIENT: Retrieving server info...\n");
		break;

	case ENET_EVENT_TYPE_DISCONNECT:
		if (m_bClientDisconnectPending)
			debugLog("CLIENT: Disconnected from Server. (Reason: Client disconnected)\n");
		else
			debugLog("CLIENT: Disconnected from Server. (Reason: Server disconnected)\n");

		clientDisconnect();
		break;
	}
}

void NetworkHandler::onServerEvent(ENetEvent e) {
	switch (e.type) {
	case ENET_EVENT_TYPE_CONNECT:
	{
		debugLog("SERVER: A new client connected from %x:%u.\n", e.peer->address.host, e.peer->address.port);

		e.peer->data = NULL;

		CLIENT_PEER cp;
		cp.id = m_iIDCounter++;
		cp.kickTime = 0.0f;
		cp.kickKillTime = 0.0f;
		cp.peer = e.peer;

		sendServerInfo(cp.id, m_server, cp.peer);

		m_vConnectedClients.push_back(cp);
	}
	break;

	case ENET_EVENT_TYPE_RECEIVE:
		if (debug_network.getBool())
			debugLog("SERVER: A packet of length %u was received from %s on channel %u.\n", e.packet->dataLength, e.peer->data, e.channelID);

		if (e.packet->data != NULL) {
			switch (*((PACKET_TYPE*)e.packet->data)) {
			case CLIENT_INFO_PACKET_TYPE:
				if (e.packet->dataLength >= sizeof(CLIENT_INFO_PACKET)) {
					CLIENT_INFO_PACKET* cp = new CLIENT_INFO_PACKET();
					*cp = *(struct CLIENT_INFO_PACKET*)e.packet->data;
					e.peer->data = cp;

					CLIENT_PEER* pp = getClientPeerByPeer(e.peer);
					if (pp != NULL) {
						pp->name = UString(((struct CLIENT_INFO_PACKET*)e.peer->data)->username).substr(0, ((struct CLIENT_INFO_PACKET*)e.peer->data)->size);

						if (cp->version != TC_PROTOCOL_VERSION) {
							debugLog("SERVER: User is trying to connect using version %i, but the server is running version %i.\n", cp->version, TC_PROTOCOL_VERSION);
							singlecastChatMessage("CONSOLE", UString::format("Version mismatch: Server is running version %i, but you are running version %i!", TC_PROTOCOL_VERSION, cp->version), m_server, e.peer);
							pp->kickTime = engine->getTime();
						}
						else {
							bool valid = true;
							if (m_serverReceiveClientInfoListener != NULL && cp->extension)
								valid = m_serverReceiveClientInfoListener((void*)(e.packet->data + sizeof(CLIENT_INFO_PACKET)));

							if (valid) {
								if (m_serverReceiveClientInfoListener != NULL)
									m_serverClientChangeListener(pp->id, pp->name, true);
							}
							else
								pp->kickTime = engine->getTime();
						}
					}
					else
						debugLog("SERVER: NULL CLIENT_PEER!\n");
				}
				break;

			case CHAT_PACKET_TYPE:
				if (e.packet->dataLength >= sizeof(CHAT_PACKET)) {
					CLIENT_PEER* pp = getClientPeerByPeer(e.peer);
					if (pp != NULL) {
						UString username = pp->name;
						UString message = UString(((struct CHAT_PACKET*)e.packet->data)->message).substr(0, ((struct CHAT_PACKET*)e.packet->data)->msize);

						chatLog(username, message);

						broadcastChatMessage((struct CHAT_PACKET*)e.packet->data, m_server, e.peer);

						if (message.find("!roll") != 1) {
							UString rollMessage = username;
							rollMessage.append(" rolls ");
							rollMessage.append(UString::format("%i point(s)", (rand() % 101)));

							broadcastChatMessage("CONSOLE", rollMessage, m_server, NULL);

							chatLog("CONSOLE", rollMessage);
						}
					}
					else
						debugLog("SERVER: NULL CLIENT_PEER!\n");
				}
				break;

			case CLIENT_BROADCAST_PACKET_TYPE:
				if (e.packet->dataLength >= sizeof(PACKET_TYPE)) {
					bool accepted = true;

					if (m_serverReceiveClientInfoListener != NULL) {
						CLIENT_PEER* pp = getClientPeerByPeer(e.peer);
						if (pp != NULL) {
							char* unwrappedPacket = (char*)e.packet->data;
							const int wrapperSize = sizeof(PACKET_TYPE);
							unwrappedPacket += wrapperSize;

							accepted = m_serverReceiveClientPacketListener(pp->id, unwrappedPacket, e.packet->dataLength - wrapperSize);
						}
						else
							debugLog("SERVER: NULL CLIENT_PEER!\n");
					}

					if (accepted && m_vConnectedClients.size() > 0) {
						char* unwrappedPacket = (char*)e.packet->data;
						const int unWrapperSize = sizeof(PACKET_TYPE);
						unwrappedPacket += unWrapperSize;

						CLIENT_BROADCAST_WRAPPER wrap;
						const int wrapperSize = sizeof(CLIENT_BROADCAST_WRAPPER);
						char* pkt = new char[(sizeof(CLIENT_BROADCAST_WRAPPER) + e.packet->dataLength - unWrapperSize)]; // this is probably going to be a memory leak but we wil attempt to deallocate memory after the packet is sent

						memcpy(((char*)&pkt) + wrapperSize, unwrappedPacket, (e.packet->dataLength - unWrapperSize));
						int size = sizeof(CLIENT_BROADCAST_WRAPPER) + e.packet->dataLength - unWrapperSize;

						for (size_t c = 0; c < m_vConnectedClients.size(); c++) {
							wrap.id = m_vConnectedClients[c].id;
							memcpy(&pkt, &wrap, wrapperSize);

							ENetPacket* packet = enet_packet_create((const void*)pkt, size, 0);

							if (m_vConnectedClients[c].peer != e.peer)
								enet_peer_send(m_vConnectedClients[c].peer, 0, packet);
						}
						enet_host_flush(m_server);
						delete[] pkt; // we like our memory without leaks (hopefully)
					}
				}
				break;

			case CLIENT_PACKET_TYPE:
				if (e.packet->dataLength >= sizeof(PACKET_TYPE)) {
					if (m_serverReceiveClientPacketListener != NULL) {
						CLIENT_PEER* pp = getClientPeerByPeer(e.peer);
						if (pp != NULL) {
							char* unwrappedPacket = (char*)e.packet->data;
							const int wrapperSize = sizeof(PACKET_TYPE);
							unwrappedPacket += wrapperSize;

							if (!m_serverReceiveClientPacketListener(pp->id, unwrappedPacket, e.packet->dataLength - wrapperSize))
								debugLog("SERVER: Received unknown CLIENT_PACKET_TYPE, WTF!\n");
						}
						else
							debugLog("SERVER: NULL CLIENT_PEER!\n");
					}
				}
				break;

			default:
				debugLog("SERVER: Received unknown packet of type %i, WTF!\n", *((PACKET_TYPE*)e.packet->data));
				break;
			}
		}
		enet_packet_destroy(e.packet);
		break;

	case ENET_EVENT_TYPE_DISCONNECT:
		CLIENT_PEER* pp = getClientPeerByPeer(e.peer);
		if (pp != NULL) {
			debugLog("SERVER: %s disconnected.\n", pp->name.toUtf8());

			if (m_serverClientChangeListener != NULL)
				m_serverClientChangeListener(pp->id, UString(""), false);
		}
		else
			debugLog("SERVER: NULL CLIENT_PEER!\n");

		if (e.peer->data != NULL) {
			delete ((CLIENT_INFO_PACKET*)e.peer->data);
			e.peer->data = NULL;
		}

		for (size_t i = 0; i < m_vConnectedClients.size(); i++) {
			if (m_vConnectedClients[i].peer == e.peer) {
				m_vConnectedClients.erase(m_vConnectedClients.begin() + i);
				break;
			}
		}
		break;
	}

}

void NetworkHandler::sendClientInfo() {
	debugLog("CLIENT: Sending client info...\n");
	UString localname = _name_.getString();

	size_t size = 0;

	CLIENT_INFO_PACKET cp;
	cp.version = TC_PROTOCOL_VERSION;
	for (int i = 0; i < clamp<int>(localname.length(), 0, 254); i++) {
		cp.username[i] = localname[i];
	}
	cp.username[254] = '\0';
	cp.size = clamp<int>(localname.length(), 0, 255);
	cp.extension = false;
	size += sizeof(CLIENT_INFO_PACKET);

	size_t extensionSize = 0;
	std::shared_ptr<void> extensionData = NULL;
	if (m_clientSendInfoListener != NULL) {
		EXTENSION_PACKET extp = m_clientSendInfoListener();

		extensionData = extp.data;
		extensionSize = extp.size;

		cp.extension = true;
	}
	char* combinedPacket = new char[(size + extensionSize)];
	memcpy(&combinedPacket, &cp, size);
	if (extensionSize > 0 && extensionData != NULL) {
		memcpy(((char*)&combinedPacket) + size, extensionData.get(), extensionSize);
		size += extensionSize;
	}

	ENetPacket* packet = enet_packet_create((const void*)combinedPacket, size, ENET_PACKET_FLAG_RELIABLE);

	if (packet != NULL) {
		enet_peer_send(m_clientPeer, 0, packet);
		enet_host_flush(m_client);
	}
	delete[] combinedPacket; // still undecided if this should be in the if statement or not, considering this needs to be deleted regaurdless of if the packet is sent or not or else a memory leak will be created.
}

void NetworkHandler::sendServerInfo(uint32_t assignedID, ENetHost* host, ENetPeer* destination) {
	debugLog("SERVER: Sending server info (%i)...\n", assignedID);

	size_t size = 0;

	SERVER_INFO_PACKET serverInfoPacket;
	serverInfoPacket.id = assignedID;
	serverInfoPacket.extension = false;
	size += sizeof(SERVER_INFO_PACKET);

	size_t extensionSize = 0;
	std::shared_ptr<void> extensionData = NULL;
	if (m_serverSendInfoListener != NULL) {
		EXTENSION_PACKET extp = m_serverSendInfoListener();

		extensionData = extp.data;
		extensionSize = extp.size;

		serverInfoPacket.extension = true;
	}

	char* combinedPacket = new char[(size + extensionSize)];
	memcpy(&combinedPacket, &serverInfoPacket, size);
	if (extensionSize > 0 && extensionData != NULL) {
		memcpy(((char*)&combinedPacket) + size, extensionData.get(), extensionSize);
		size += extensionSize;
	}
	ENetPacket* packet = enet_packet_create((const void*)combinedPacket, size, ENET_PACKET_FLAG_RELIABLE);
	if (packet != NULL) {
		enet_peer_send(destination, 0, packet);
		enet_host_flush(host);
	}
	delete[] combinedPacket; // NetworkHandler.cpp #629  same shit
}

void NetworkHandler::servercast(void* data, uint32_t size, bool reliable) {
	if (!isClient()) return;

	PACKET_TYPE wrap = CLIENT_PACKET_TYPE;
	const int wrapperSize = sizeof(PACKET_TYPE);
	char* wrappedPacket = new char[(sizeof(PACKET_TYPE) + size)];
	memcpy(&wrappedPacket, &wrap, wrapperSize);
	memcpy(((char*)&wrappedPacket) + wrapperSize, data, size);
	size += wrapperSize;

	ENetPacket* packet = enet_packet_create((const void*)wrappedPacket, size, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);

	if (packet != NULL) {
		enet_peer_send(m_clientPeer, 0, packet);
		enet_host_flush(m_client);
	}
	delete[] wrappedPacket;
}

void NetworkHandler::clientcast(void* data, uint32_t size, uint32_t id, bool reliable) {
	if (!isServer()) return;

	CLIENT_PEER* pp = getClientPeerById(id);
	if (pp == NULL) {
		debugLog("SERVER: Tried to clientcast(void *, %i, %i, %i) to non-existing CLIENT_PEER!\n", size, id, (int)reliable);
		return;
	}

	CLIENT_BROADCAST_WRAPPER wrap;
	const int wrapperSize = sizeof(CLIENT_BROADCAST_WRAPPER);
	char* wrappedPacket = new char[(sizeof(CLIENT_BROADCAST_WRAPPER) + size)];
	memcpy(&wrappedPacket, &wrap, wrapperSize);
	memcpy(((char*)&wrappedPacket) + wrapperSize, data, size);
	size += wrapperSize;

	ENetPacket* packet = enet_packet_create((const void*)wrappedPacket, size, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);

	if (packet != NULL) {
		enet_peer_send(pp->peer, 0, packet);
		enet_host_flush(m_server);
	}
	delete[] wrappedPacket;
}

void NetworkHandler::say(UString message) {
	if (message.length() < 1) return;

	if (!isClient() && isServer()) {
		UString localName = _name_admin_.getString();

		broadcastChatMessage(localName, message, m_server, NULL);

		chatLog(localName, message);
		return;
	}
	if (!isClient()) {
		debugLog("CLIENT: Not connected to any server.\n");
		return;
	}
	if (m_bClientConnectPending) {
		debugLog("CLIENT: Please wait until you are connected to the server!\n");
		return;
	}

	UString localName = _name_.getString();
	singlecastChatMessage(localName, message, m_client, m_clientPeer);

	if (!isServer()) // to avoid double logs on local hosts
		chatLog(localName, message);
}

void NetworkHandler::kick(UString username) {
	if (username.length() < 1) return;

	if (!isServer()) return;

	for (size_t c = 0; c < m_vConnectedClients.size(); c++) {
		if (m_vConnectedClients[c].name == username) {
			singlecastChatMessage("CONSOLE", "You were kicked from the server, better luck next time", m_server, m_vConnectedClients[c].peer);

			m_vConnectedClients[c].kickTime = engine->getTime();
			return;
		}
	}
	UString msg = "SERVER: Couldn't find user \"";
	msg.append(username);
	msg.append("\"\n");
	debugLog("%s", msg.toUtf8());
}

void NetworkHandler::chatLog(UString username, UString message) {
	UString chatlog = username;
	chatlog.append(": ");
	chatlog.append(message);
	chatlog.append("\n");
	debugLog("%s", chatlog.toUtf8());
}

NetworkHandler::CLIENT_PEER* NetworkHandler::getClientPeerByPeer(ENetPeer* peer) {
	for (size_t c = 0; c < m_vConnectedClients.size(); c++) {
		if (m_vConnectedClients[c].peer == peer)
			return (&m_vConnectedClients[c]);
	}
	return NULL;
}

NetworkHandler::CLIENT_PEER* NetworkHandler::getClientPeerById(uint32_t id) {
	for (size_t c = 0; c < m_vConnectedClients.size(); c++) {
		if (m_vConnectedClients[c].id == id)
			return (&m_vConnectedClients[c]);
	}
	return NULL;
}

int NetworkHandler::getPing() const {
	return m_clientPeer != NULL ? (m_clientPeer->roundTripTime) : 0;
}

bool NetworkHandler::isClient() const {
	return m_client != NULL && m_clientPeer != NULL;
}

bool NetworkHandler::isServer() const {

	return m_server != NULL;
}