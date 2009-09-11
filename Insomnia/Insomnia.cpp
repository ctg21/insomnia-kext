/*
	File:			Insomnia.cpp
	Program:		Insomnia
	Author:			Michael Roßberg/Alexey Manannikov/Dominik Wickenhauser/Andrew James
	Description:	Insomnia is a kext module to disable sleep on ClamshellClosed
 
	Insomnia is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
 
	Insomnia is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
 
	You should have received a copy of the GNU General Public License
	along with Insomnia; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <IOKit/IOLib.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <sys/kern_control.h>


#include <mach/mach_types.h>
//#include <sys/systm.h>
#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <sys/kern_control.h>

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
//#include <sys/systm.h>
#include <sys/kern_event.h>


#include "Insomnia.h"


#define super IOService

static int sysctl_lidsleep SYSCTL_HANDLER_ARGS;
static int lidvar = 0;

static UInt32	insomniaDebug;

SYSCTL_PROC(_kern, OID_AUTO, lidsleep, CTLTYPE_INT|CTLFLAG_RW|CTLFLAG_KERN, &lidvar, 0, &sysctl_lidsleep, "I", "Insomnia.kext");

struct kern_ctl_reg ep_ctl; // Initialize control 
kern_ctl_ref kctlref;
class Insomnia *myinstance;

OSDefineMetaClassAndStructors(Insomnia, IOService);

#pragma mark -

/* init function for Insomnia, unchanged from orginal Insomnia */
bool Insomnia::init(OSDictionary* properties) {
	
	insomniaDebug=0;
	
	IOLog("Insomnia:init. 0 to disable\n");
	
    if (super::init(properties) == false) {
		if(insomniaDebug) IOLog("Insomnia::init: super::init failed\n");
		return false;
    }
	
	myinstance = this;
	
	messageClamshellClosed=kIOPMClamshellClosed;
	
	sysctl_register_oid(&sysctl__kern_lidsleep);
	
    return true;
}


/* start function for Insomnia, fixed send_event to match other code */
bool Insomnia::start(IOService* provider) {
	
	if(insomniaDebug) IOLog("Insomnia:start\n");
	
	lastLidState = 0;//was true
	
	//lidState=1;
	//sleepState=1;
	
	if (!super::start(provider)) {
		if(insomniaDebug) IOLog("Insomnia::start: super::start failed\n");
		return false;
    }
	
	
	
	//if(insomniaDebug) IOLog("Insomina: Sending event kIOPMDisableClamshell\n");
	//Insomnia::send_event(kIOPMDisableClamshell);
	
	
    return true;
}


/* free function for Insomnia, fixed send_event to match other code */
void Insomnia::free() {
    IOPMrootDomain *root = NULL;
    
    root = getPMRootDomain();
	
    if (!root) {
        if(insomniaDebug) IOLog("Insomnia: Fatal error could not get RootDomain.\n");
        return;
    }
    
	/* Reset the system to orginal state */
    Insomnia::send_event(kIOPMAllowSleep | kIOPMEnableClamshell);
	
	sysctl_unregister_oid(&sysctl__kern_lidsleep);
	
    if(insomniaDebug) IOLog("Insomnia: Lid close is now processed again.\n");
	
    super::free();
    return;
}

// ###########################################################################
void Insomnia::stop(IOService* provider) {
 
	//sysctl_unregister_oid(&sysctl__kern_lidsleep);
	
    super::stop(provider);
}

#pragma mark -

/* Send power messages to rootDomain */
bool Insomnia::send_event(UInt32 msg) {
    IOPMrootDomain *root = NULL;
	IOReturn		ret=kIOReturnSuccess;
	char			err_str[100];
	
    root = getPMRootDomain();
    if (!root) {
        if(insomniaDebug) IOLog("Insomnia: Fatal error could not get RootDomain.\n");
        return false;
    }
	
	ret = root->receivePowerNotification(msg);
	
	if(ret!=kIOReturnSuccess)
	{
		IOLog("Insomina: Error sending event: %d\n", ret);
		if(insomniaDebug) IOLog(err_str);
	}
	
	return true;
}


/* kIOPMMessageClamshallStateChange Notification */
IOReturn Insomnia::message(UInt32 type, IOService * provider, void * argument) {
	
//  from kernel source	kClamshellStateBit
//	true        == clamshell is closed
//  false       == clamshell is open
 
 
	
	
	if (type == kIOPMMessageClamshellStateChange) {
		if(insomniaDebug) IOLog("========================\n");
		if(insomniaDebug) IOLog("Insomnia: Clamshell State Changed\n");
		
		if(insomniaDebug) IOLog("Insomnia: lastLidState %d\n", lastLidState);
		if(insomniaDebug) IOLog("Insomnia: counter %d\n", counter);
		
		lidState=argument && kClamshellStateBit;
		
		if(insomniaDebug) IOLog("Insomnia: 1 lidState %d\n", lidState);
			
		if(insomniaDebug) IOLog("Insomnia: insomniaState %d\n", insomniaState);
		
		// If lid was closed 
		if ( (lidState==1) ) //lid is closed now
			{
			if( (lastLidState==0) ) //lid was opened before
				{
			
				if(insomniaDebug) IOLog("Insomnia: kClamshellStateBit set - lid was closed\n");
			
				if(insomniaState==1)
					{
					if(insomniaDebug) IOLog("Insomina: Sending event kIOPMDisableClamshell\n");
					
					lastLidState = 1;
					
					Insomnia::send_event(kIOPMDisableClamshell);// | kIOPMClamshellOpened);//kIOPMPreventSleep | kIOPMClamshellOpened);
				
					
				
					counter=1;
				
					if(insomniaDebug) IOLog("Insomnia: counter %d\n", counter);
					}
				else {
					if(insomniaDebug) IOLog("Insomina: not enabled while closing lid\n");
					
					//Insomnia::send_event(kIOPMEnableClamshell);
			
					}
								
		// If lastLidState is true - lid closed 
				}
			}
		else // lid opened
			{
			if (lastLidState==1) //lid was closed before
				{
				if(insomniaDebug) IOLog("Insomnia: kClamshellStateBit not set - lid was opened\n");
				lastLidState = 0;
			
				if(insomniaDebug) IOLog("Insomnia: counter %d\n", counter);
			
				if(counter==1)
					counter=2;
				
				if(insomniaDebug) IOLog("Insomnia: counter %d\n", counter);
			
				}
			else {
				/*if(lidState==0 && counter==2 && lastLidState==1)
					counter=3;
				else 
					if(lidState==0 && counter==3)
						counter=0;
				*/
				}
			}
			
		if(( argument && kClamshellSleepBit)) 
				if(insomniaDebug) IOLog("Insomnia: kClamshellSleepBit set - system will sleep\n");
			else
				if(insomniaDebug) IOLog("Insomnia: kClamshellSleepBit NOT set - system will NOT sleep\n");
		
				
		if(insomniaDebug) IOLog("Insomnia: lastLidState %d\n", lastLidState);
		if(insomniaDebug) IOLog("Insomnia: lidState %d\n", lidState);
		if(insomniaDebug) IOLog("Insomnia: counter %d\n", counter);
		
	}
	

	
	return super::message(type, provider, argument);
}

// ###########################################################################

static int sysctl_lidsleep SYSCTL_HANDLER_ARGS {
	int error;

	//error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2,  req);

	error = sysctl_handle_int(oidp, &lidvar, sizeof(lidvar),  req);
	
	if (!error && req->newptr) {//we got value // 
	
		if(insomniaDebug) IOLog("Insomnia: we got value %d\n", lidvar);
		
		if(lidvar!=1 && lidvar!=0 && lidvar!=2 && lidvar!=3)
			return EINVAL;
		
        if(lidvar==2)
			insomniaDebug=0;
		if(lidvar==3)
			insomniaDebug=1;
		
		if(lidvar>1)
			{
			lidvar = myinstance->insomniaState;
			
			return 0;
			}
		
		if(lidvar != myinstance->insomniaState)
			{
			
			
			myinstance->insomniaState = lidvar;
			
			switch(myinstance->insomniaState)
				{
				case 1:
				
				if(insomniaDebug) IOLog("Insomnia: got Enable value\n");
				
				//if(insomniaDebug) IOLog("Insomina: Sending event kIOPMDisableClamshell\n");
				
				myinstance->send_event(kIOPMDisableClamshell);// | kIOPMPreventSleep);
				break;
				
			case 0:
				if(insomniaDebug) IOLog("Insomnia: got Disable value\n");
				
				myinstance->send_event(kIOPMEnableClamshell);
				
				if(myinstance->lastLidState==0 && myinstance->lidState==1)// && myinstance->counter==3)//was closed
					{
					if(insomniaDebug) IOLog("Insomnia: lid was closed while got Disable value\n");
					
					if(insomniaDebug) IOLog("Insomina: Sending event kIOPMEnableClamshell\n");
					
					myinstance->send_event(kIOPMEnableClamshell | kIOPMAllowSleep);
				
					if(insomniaDebug) IOLog("Insomina: Sending event messageClamshellClosed\n");
					
					myinstance->send_event(myinstance->messageClamshellClosed);
					}
					
				break;
			}
				
			}
    } else if (req->newptr) {
        /* Something was wrong with the write request */
		if(insomniaDebug) IOLog("Insomnia: error setting value, error %d\n", error);
    } else {
        /* Read request.  */
        SYSCTL_OUT(req, &lidvar, sizeof(lidvar));
    }
	
	

	return error;
}
