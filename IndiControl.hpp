#include <indiapi.h>
#include <libindi/baseclient.h>
#include <libindi/basedevice.h>
#include <libindi/inditelescope.h>
#include <memory>
#include <cstring>
#include <iostream>
#include <vector>
#include <stdlib.h>

//#define MOUNTNAME "Telescope Simulator"
#define MOUNTNAME "EQMod Mount"

class ScopeClient : public INDI::BaseClient
{
public:

	ScopeClient(bool quiet=false): quiet(quiet) {
		scope = NULL;
		setServer("localhost", 7624);
		watchDevice(MOUNTNAME);
		connectServer();
		//wait for connection
		while (scope == NULL || !scope->isConnected() ||
				scope->getSwitch("ON_COORD_SET") == NULL) {}
		//now, make the telescope track after slew
		ISwitchVectorProperty *switchprop = scope->getSwitch("ON_COORD_SET");

		for (int i=0; i < switchprop->nsp; i++) {
			if (!strcmp(switchprop->sp[i].name, "TRACK")) {
				switchprop->sp[i].s = ISS_ON;
			}
			else switchprop->sp[i].s = ISS_OFF;
		}
		sendNewSwitch(switchprop);
		std::cout << "Scope ready!" << std::endl;
	}
	~ScopeClient() {

	}
	void Goto(double ra, double dec)
	{
		INumberVectorProperty *coords = NULL;
		coords = scope->getNumber("EQUATORIAL_EOD_COORD");

		while (coords == NULL) {
			std::cout << "No EQUATORIAL_EOD_COORD property yet. Waiting...\n";
			sleep(1);
			coords = scope->getNumber("EQUATORIAL_EOD_COORD");
		}
		coords->np[0].value = ra;
		coords->np[1].value = dec;
		sendNewNumber(coords);
	}

	void MoveBack(double amount=1)
	{
		INumberVectorProperty *coords = NULL;
		coords = scope->getNumber("EQUATORIAL_EOD_COORD");

		if (coords == NULL)
		{
			std::cout << "Can't find EQUATORIAL_EOD_COORD property\n";
			return;
		}
		double dest_ra = coords->np[0].value + amount;
		if (dest_ra >= 24) dest_ra -= 24;
		coords->np[0].value = dest_ra;
		sendNewNumber(coords);
	}

	std::vector<double> getPos() {
		INumberVectorProperty *coords = NULL;
		coords = scope->getNumber("EQUATORIAL_EOD_COORD");
		if (coords == NULL)
		{
			std::cout << "Can't find EQUATORIAL_EOD_COORD property\n";
			return std::vector<double>();
		}
		std::vector<double> ret;
		ret.push_back(coords->np[0].value);
		ret.push_back(coords->np[1].value);
		return ret;
	}

protected:
	void newDevice(INDI::BaseDevice *dp)
	{
		if (!strcmp(dp->getDeviceName(), MOUNTNAME))
			IDLog("Receiving '%s'...\n", dp->getDeviceName());
		scope = dp;
	}
	//virtual void newDevice(INDI::BaseDevice *dp);
	void newProperty(INDI::Property *property)
	{
		if (!strcmp(property->getDeviceName(), MOUNTNAME) &&
				!strcmp(property->getName(), "CONNECTION"))
		{
			connectDevice(MOUNTNAME);
			return;
		}
	}
	virtual void removeProperty(INDI::Property *property) {}
	virtual void newBLOB(IBLOB *bp) {}
	virtual void newSwitch(ISwitchVectorProperty *svp) {}
	void newNumber(INumberVectorProperty *nvp)
	{
		// Let's check if we get any new values for CCD_TEMPERATURE
		if (!strcmp(nvp->name, "EQUATORIAL_EOD_COORD"))
		{
			ra = nvp->np[0].value;
			dec = nvp->np[1].value;
			if (!quiet) std::cout << " RA DEC " << ra << " " << dec << std::endl;
		}
	}
	void newMessage(INDI::BaseDevice *dp, int messageID) {
		if (strcmp(dp->getDeviceName(), MOUNTNAME))
			return;
		std::cout << dp->messageQueue(messageID) << std::endl;
	}
	virtual void newText(ITextVectorProperty *tvp) {}
	virtual void newLight(ILightVectorProperty *lvp) {}
	virtual void serverConnected() {}
	virtual void serverDisconnected(int exit_code) {}
private:
	INDI::BaseDevice* scope;
	bool quiet;
	double ra, dec;
};
