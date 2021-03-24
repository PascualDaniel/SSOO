#include "OperatingSystem.h"
#include "OperatingSystemBase.h"
#include "MMU.h"
#include "Processor.h"
#include "Buses.h"
#include "Heap.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// Functions prototypes
void OperatingSystem_PCBInitialization(int, int, int, int, int);
void OperatingSystem_MoveToTheREADYState(int);
void OperatingSystem_Dispatch(int);
void OperatingSystem_RestoreContext(int);
void OperatingSystem_SaveContext(int);
void OperatingSystem_TerminateProcess();
int OperatingSystem_LongTermScheduler();
void OperatingSystem_PreemptRunningProcess();
int OperatingSystem_CreateProcess(int);
int OperatingSystem_ObtainMainMemory(int, int);
int OperatingSystem_ShortTermScheduler();
int OperatingSystem_ExtractFromReadyToRun();
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();
void OperatingSystem_PrintReadyToRunQueue();

//Ejercicio 10
char * statesNames [5]={"NEW","READY","EXECUTING","BLOCKED","EXIT"}; 



// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID=NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Initial PID for assignation
int initialPID=PROCESSTABLEMAXSIZE-1;
char initialProgramName=' ';

// Begin indes for daemons in programList
int baseDaemonsInProgramList; 

// Array that contains the identifiers of the READY processes
//heapItem readyToRunQueue[PROCESSTABLEMAXSIZE];
//int numberOfReadyToRunProcesses=0;

//Ejercicio 11
heapItem readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
int numberOfReadyToRunProcesses[NUMBEROFQUEUES]={0,0};
char * queueNames[NUMBEROFQUEUES]={"USER","DAEMONS"};



// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses=0;

// Initial set of tasks of the OS
void OperatingSystem_Initialize(int daemonsIndex) {
	
	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code
	programFile=fopen("OperatingSystemCode", "r");
	if (programFile==NULL){
		// Show red message "FATAL ERROR: Missing Operating System!\n"
		ComputerSystem_DebugMessage(99,SHUTDOWN,"FATAL ERROR: Missing Operating System!\n");
		exit(1);		
	}

	// Obtain the memory requirements of the program
	int processSize=OperatingSystem_ObtainProgramSize(programFile);

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);
	
	// Process table initialization (all entries are free)
	for (i=0; i<PROCESSTABLEMAXSIZE;i++){
		processTable[i].busy=0;
	}
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base+2);
		
	// Include in program list  all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);
	
	// Create all user processes from the information given in the command line
	int processCreated = OperatingSystem_LongTermScheduler(initialPID,initialProgramName);
	if(processCreated==0){
		OperatingSystem_ReadyToShutdown();
		
	}
		
	


	if (strcmp(programList[processTable[sipID].programListIndex]->executableName,"SystemIdleProcess")) {
		// Show red message "FATAL ERROR: Missing SIP program!\n"
		ComputerSystem_DebugMessage(99,SHUTDOWN,"FATAL ERROR: Missing SIP program!\n");
		exit(1);		
	}

	// At least, one user process has been created
	// Select the first process that is going to use the processor
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);
}

// Daemon processes are system processes, that is, they work together with the OS.
// The System Idle Process uses the CPU whenever a user process is able to use it
int OperatingSystem_PrepareStudentsDaemons(int programListDaemonsBase) {

	// Prepare aditionals daemons here
	// index for aditionals daemons program in programList
	// programList[programListDaemonsBase]=(PROGRAMS_DATA *) malloc(sizeof(PROGRAMS_DATA));
	// programList[programListDaemonsBase]->executableName="studentsDaemonNameProgram";
	// programList[programListDaemonsBase]->arrivalTime=0;
	// programList[programListDaemonsBase]->type=DAEMONPROGRAM; // daemon program
	// programListDaemonsBase++

	return programListDaemonsBase;
};


// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the 
// 			command line and daemons programs
int OperatingSystem_LongTermScheduler(int PID,char *name) {

	int  i,numberOfSuccessfullyCreatedProcesses=0;

	//No se puede crear el proceso
    if(PID == NOFREEENTRY){
		ComputerSystem_DebugMessage(103,ERROR,name); 
	}else
	if(PID==PROGRAMDOESNOTEXIST ){
		ComputerSystem_DebugMessage(104,ERROR,name,"it does not exist"); 
	}else
	if(PID==PROGRAMNOTVALID ){
		ComputerSystem_DebugMessage(104,ERROR,name,"invalid priority or size"); 
	}else 
	if(PID==TOOBIGPROCESS ){
		ComputerSystem_DebugMessage(105,ERROR,name); 
	}
	else{
		for (i=0; programList[i]!=NULL && i<PROGRAMSMAXNUMBER ; i++) {
			PID=OperatingSystem_CreateProcess(i);		
			numberOfSuccessfullyCreatedProcesses++;
			if (programList[i]->type==USERPROGRAM) 
				numberOfNotTerminatedUserProcesses++;
			// Move process to the ready state
			OperatingSystem_MoveToTheREADYState(PID);
		}
	}
	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}


// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram) {
  
	int PID;
	int processSize;
	int loadingPhysicalAddress;
	int priority;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram=programList[indexOfExecutableProgram];

	// Obtain a process ID
	PID=OperatingSystem_ObtainAnEntryInTheProcessTable();

	//if PID == NOFREEENTRY (-3)no es valido
	if(PID == NOFREEENTRY){
		OperatingSystem_LongTermScheduler(NOFREEENTRY,executableProgram->executableName);
		return NOFREEENTRY;
	}


	// Check if programFile exists
	programFile=fopen(executableProgram->executableName, "r");
	if (programFile==NULL){
		OperatingSystem_LongTermScheduler(PROGRAMDOESNOTEXIST,executableProgram->executableName);
		return PROGRAMDOESNOTEXIST;
	}

	// Obtain the memory requirements of the program
	processSize=OperatingSystem_ObtainProgramSize(programFile);	
	// Obtain the priority for the process
	priority=OperatingSystem_ObtainPriority(programFile);
	if (processSize==PROGRAMNOTVALID || priority == PROGRAMNOTVALID){
		OperatingSystem_LongTermScheduler(PROGRAMNOTVALID,executableProgram->executableName);
	    return PROGRAMNOTVALID;
	}
	
	// Obtain enough memory space
 	loadingPhysicalAddress=OperatingSystem_ObtainMainMemory(processSize, PID);
	if (loadingPhysicalAddress==TOOBIGPROCESS){
		OperatingSystem_LongTermScheduler(TOOBIGPROCESS,executableProgram->executableName);
	    return TOOBIGPROCESS;
	}
	int errorNum;
	// Load program in the allocated memory
	errorNum = OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize);
	if (errorNum==TOOBIGPROCESS){
		OperatingSystem_LongTermScheduler(TOOBIGPROCESS,executableProgram->executableName);
	    return TOOBIGPROCESS;
	}
	
	// PCB initialization
	OperatingSystem_PCBInitialization(PID, loadingPhysicalAddress, processSize, priority, indexOfExecutableProgram);
	
	// Show message "Process [PID] created from program [executableName]\n"
	ComputerSystem_DebugMessage(70,INIT,PID,executableProgram->executableName);
	
	return PID;
}


// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID) {

 	if (processSize>MAINMEMORYSECTIONSIZE)
		return TOOBIGPROCESS;
	
 	return PID*MAINMEMORYSECTIONSIZE;
}


// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex) {

	processTable[PID].busy=1;
	processTable[PID].initialPhysicalAddress=initialPhysicalAddress;
	processTable[PID].processSize=processSize;
	processTable[PID].state=NEW; 
	processTable[PID].priority=priority;
	processTable[PID].programListIndex=processPLIndex;
	//Ejeccicio 11 poner tipo
	processTable[PID].queueID=programList[processPLIndex]->type;
	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM) {
		processTable[PID].copyOfPCRegister=initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister= ((unsigned int) 1) << EXECUTION_MODE_BIT;

		
		
	} 
	else {
		processTable[PID].copyOfPCRegister=0;
		processTable[PID].copyOfPSWRegister=0;
		processTable[PID].copyOfAcummRegister=0;
	}
	
	ComputerSystem_DebugMessage(111,SHORTTERMSCHEDULE,PID,programList[processTable[PID].programListIndex]->executableName, statesNames[processTable[PID].state]);
}


// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID) {
	int queue = processTable[PID].queueID;
	if (Heap_add(PID, readyToRunQueue[queue ],QUEUE_PRIORITY 
					,&numberOfReadyToRunProcesses[queue] ,PROCESSTABLEMAXSIZE)>=0) {

		ComputerSystem_DebugMessage(110,SHORTTERMSCHEDULE,PID,
			programList[processTable[PID].programListIndex]->executableName, 
			statesNames[processTable[PID].state],
			statesNames[READY]);

		processTable[PID].state=READY;
		OperatingSystem_PrintReadyToRunQueue();
	} 
	
}


// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
int OperatingSystem_ShortTermScheduler() {
	
	int selectedProcess;

	selectedProcess=OperatingSystem_ExtractFromReadyToRun();
	
	return selectedProcess;
}


// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromReadyToRun() {
  
	int selectedProcess=NOPROCESS;

	selectedProcess=Heap_poll(readyToRunQueue[USERPROCESSQUEUE ],QUEUE_PRIORITY ,&numberOfReadyToRunProcesses[USERPROCESSQUEUE ]);
	if(selectedProcess==-1)
		selectedProcess=Heap_poll(readyToRunQueue[DAEMONPROGRAM ],QUEUE_PRIORITY ,&numberOfReadyToRunProcesses[DAEMONPROGRAM ]);
	
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess; 
}


// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID) {

	// The process identified by PID becomes the current executing process
	executingProcessID=PID;
	ComputerSystem_DebugMessage(110,SHORTTERMSCHEDULE,PID,
			programList[processTable[PID].programListIndex]->executableName, 
			statesNames[processTable[PID].state],
			statesNames[EXECUTING]);
	// Change the process' state
	processTable[PID].state=EXECUTING;
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}


// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID) {
  
	// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE-1,processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE-2,processTable[PID].copyOfPSWRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE-3,processTable[PID].copyOfAcummRegister);
	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
}


// Function invoked when the executing process leaves the CPU 
void OperatingSystem_PreemptRunningProcess() {

	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);
	// The processor is not assigned until the OS selects another process
	executingProcessID=NOPROCESS;
}


// Save in the process' PCB essential values stored in hardware registers and the system stack
void OperatingSystem_SaveContext(int PID) {
	
	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-1);
	
	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-2);

	//Ejercicio 13
	processTable[PID].copyOfAcummRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-3);
	
}


// Exception management routine
void OperatingSystem_HandleException() {
  
	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	ComputerSystem_DebugMessage(71,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
	
	OperatingSystem_TerminateProcess();
}


// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess() {
  
	int selectedProcess;
  	ComputerSystem_DebugMessage(110,SHORTTERMSCHEDULE,executingProcessID,
			programList[processTable[executingProcessID].programListIndex]->executableName, 
			statesNames[processTable[executingProcessID].state],
			statesNames[EXIT]);
	processTable[executingProcessID].state=EXIT;
	
	
	if (programList[processTable[executingProcessID].programListIndex]->type==USERPROGRAM) 
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;
	
	if (numberOfNotTerminatedUserProcesses==0) {
		if (executingProcessID==sipID) {
			// finishing sipID, change PC to address of OS HALT instruction
			OperatingSystem_TerminatingSIP();
			ComputerSystem_DebugMessage(99,SHUTDOWN,"The system will shut down now...\n");
			return; // Don't dispatch any process
		}
		// Simulation must finish, telling sipID to finish
		OperatingSystem_ReadyToShutdown();
	}
	// Select the next process to execute (sipID if no more user processes)
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}

// System call management routine
void OperatingSystem_HandleSystemCall() {
  
	int systemCallID;


	int	oldProcess ;
	int newProcess ;
	//ProcesoActual	
	oldProcess= executingProcessID;
	//Siguiente Proceso
	newProcess = readyToRunQueue[processTable[executingProcessID].queueID][0].info;
	


	// Register A contains the identifier of the issued system call
	systemCallID=Processor_GetRegisterA();
	
	switch (systemCallID) {
		case SYSCALL_PRINTEXECPID:
			// Show message: "Process [executingProcessID] has the processor assigned\n"
			ComputerSystem_DebugMessage(72,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			break;

		case SYSCALL_END:
			// Show message: "Process [executingProcessID] has requested to terminate\n"
			ComputerSystem_DebugMessage(73,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			OperatingSystem_TerminateProcess();
			break;
		case SYSCALL_YIELD:
			//Si tienen la misma prioridad
			if(processTable[oldProcess].priority==processTable[newProcess].priority){
				// Function invoked when the executing process leaves the CPU
				OperatingSystem_PreemptRunningProcess();
				// Select the next process to execute (sipID if no more user processes)
				//selectedProcess=OperatingSystem_ShortTermScheduler();
				OperatingSystem_Dispatch(OperatingSystem_ShortTermScheduler());
				ComputerSystem_DebugMessage(115,SHORTTERMSCHEDULE,oldProcess,programList[processTable[oldProcess].programListIndex]->executableName
																 ,newProcess,programList[processTable[newProcess].programListIndex]->executableName);

			}

			break;		
	}
}
	
//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint){
	switch (entryPoint){
		case SYSCALL_BIT: // SYSCALL_BIT=2
			OperatingSystem_HandleSystemCall();
			break;
		case EXCEPTION_BIT: // EXCEPTION_BIT=6
			OperatingSystem_HandleException();
			break;
	}

}
//Cambiar insertion Order Esta maaaaaaaaalllll
//mallllll
//==========================MAAAAAALLL

void OperatingSystem_PrintReadyToRunQueue(){
	int queue;
	ComputerSystem_DebugMessage(106,SHORTTERMSCHEDULE);	
	for(queue=0; queue<NUMBEROFQUEUES ; queue++){
		//Nombre de la queue
		ComputerSystem_DebugMessage(109,SHORTTERMSCHEDULE,queueNames[queue]);
		int i;
		for(i=0; i<numberOfReadyToRunProcesses[queue]-1 ; i++){
			//Los valores
			ComputerSystem_DebugMessage(107,SHORTTERMSCHEDULE,readyToRunQueue[queue][i].info, processTable[readyToRunQueue[queue][i].info].priority);
		}
		if(numberOfReadyToRunProcesses[queue] !=0)
			ComputerSystem_DebugMessage(108,SHORTTERMSCHEDULE,readyToRunQueue[queue][i].info,processTable[readyToRunQueue[queue][i].info].priority);
		else{
			ComputerSystem_DebugMessage(112,SHORTTERMSCHEDULE);	
		}
	}
}

