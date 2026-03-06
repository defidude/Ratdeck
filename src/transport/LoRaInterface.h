#pragma once

#include <Interface.h>
#include "radio/SX1262.h"

class LoRaInterface : public RNS::InterfaceImpl {
public:
    LoRaInterface(SX1262* radio, const char* name = "LoRaInterface");
    virtual ~LoRaInterface();

    virtual bool start() override;
    virtual void stop() override;
    virtual void loop() override;

    virtual inline std::string toString() const override {
        return "LoRaInterface[" + _name + "]";
    }

protected:
    virtual void send_outgoing(const RNS::Bytes& data) override;

private:
    SX1262* _radio;
};
