#include "AutoInterfaceWrapper.h"

#include <Transport.h>

bool AutoInterfaceWrapper::start(const char* group_id,
								 uint8_t max_peers,
								 const String& link_local_addr,
								 uint32_t scope_id) {
	if (_started) return true;
	if (link_local_addr.length() == 0) {
		Serial.println("[AUTOIFACE] start: no link-local address");
		return false;
	}

	auto* impl = new RNS::AutoInterface(
		"AutoInterface",
		group_id,
		RNS::AutoInterface::MCAST_ADDR_TYPE_TEMPORARY,
		RNS::AutoInterface::SCOPE_LINK,
		RNS::AutoInterface::DEFAULT_DISCOVERY_PORT,
		RNS::AutoInterface::DEFAULT_DATA_PORT,
		max_peers);
	impl->set_link_local(link_local_addr.c_str(), scope_id);

	_wrapper = impl;
	_impl    = std::shared_ptr<RNS::AutoInterface>(
		impl, [](RNS::AutoInterface*) {});  // wrapper owns lifetime via Interface

	_wrapper.mode(RNS::Type::Interface::MODE_FULL);
	if (!_wrapper.start()) {
		Serial.println("[AUTOIFACE] start: open_sockets failed");
		_wrapper.clear();
		_impl.reset();
		return false;
	}
	RNS::Transport::register_interface(_wrapper);
	_started = true;
	Serial.printf("[AUTOIFACE] online — mcast=%s ll=%s scope=%u\n",
		_impl->multicast_address().c_str(),
		_impl->link_local_address().c_str(),
		(unsigned)scope_id);
	return true;
}

void AutoInterfaceWrapper::stop() {
	if (!_started) return;
	if (_wrapper) {
		RNS::Transport::deregister_interface(_wrapper);
		_wrapper.stop();
		_wrapper.clear();
	}
	_impl.reset();
	_started = false;
	Serial.println("[AUTOIFACE] stopped");
}

void AutoInterfaceWrapper::loop() {
	if (_started && _wrapper) _wrapper.loop();
}

bool AutoInterfaceWrapper::isOnline() const {
	return _started && _wrapper && _wrapper.online();
}

size_t AutoInterfaceWrapper::peerCount() const {
	return _impl ? _impl->peer_count() : 0;
}

const std::string& AutoInterfaceWrapper::multicastAddress() const {
	if (_impl) return _impl->multicast_address();
	return _empty_addr;
}
