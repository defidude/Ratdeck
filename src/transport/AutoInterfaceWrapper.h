#pragma once

#include <Arduino.h>
#include <Interfaces/AutoInterface.h>

#include <memory>

/*
 * Firmware-side glue around RNS::AutoInterface.  Handles the
 * WiFi-STA-driven start/stop lifecycle and exposes a small surface
 * (peer_count, multicast_address, online flag) for the UI / heartbeat.
 *
 * Use:
 *   1. wifi STA connects → resolve link-local IPv6 + scope id
 *   2. wrapper.start(group_id, max_peers, link_local, scope_id)
 *   3. wrapper.loop() each main-loop tick
 *   4. on STA disconnect → wrapper.stop()
 */
class AutoInterfaceWrapper {
public:
	bool start(const char* group_id,
			   uint8_t max_peers,
			   const String& link_local_addr,
			   uint32_t scope_id);
	void stop();
	void loop();

	bool   isOnline()    const;
	size_t peerCount()   const;
	const std::string& multicastAddress() const;

private:
	std::shared_ptr<RNS::AutoInterface> _impl;
	RNS::Interface _wrapper{RNS::Type::NONE};
	std::string    _empty_addr;  // sentinel returned when not started
	bool _started = false;
};
