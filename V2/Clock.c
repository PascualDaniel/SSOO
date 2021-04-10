#include "Clock.h"
#include "Processor.h"
#include "ComputerSystemBase.h"

int tics=0;


void Clock_Update() {

	tics++;
	//intervalBetweenInterrupts
	if(tics%intervalBetweenInterrupts==0){
		Processor_RaiseInterrupt(CLOCKINT_BIT);

		
	}
    // ComputerSystem_DebugMessage(97,CLOCK,tics);

}


int Clock_GetTime() {

	return tics;
}
