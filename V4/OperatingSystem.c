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
void OperatingSystem_MoveToTheBLOCKState();
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
void OperatingSystem_HandleClockInterrupt();
void OperatingSystem_PrintReadyToRunQueue();
int comparePrioritys(int a, int b);
int OperatingSystem_GetExecutingProcessID();
//V4 Ej 8
void OperatingSystem_ReleaseMainMemory();

//Ejercicio 10
char *statesNames[5] = {"NEW", "READY", "EXECUTING", "BLOCKED", "EXIT"};

// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID = NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Initial PID for assignation
int initialPID = PROCESSTABLEMAXSIZE - 1;
char initialProgramName = ' ';

// Begin indes for daemons in programList
int baseDaemonsInProgramList;

// Array that contains the identifiers of the READY processes
//heapItem readyToRunQueue[PROCESSTABLEMAXSIZE];
//int numberOfReadyToRunProcesses=0;

//Ejercicio 11
heapItem readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
int numberOfReadyToRunProcesses[NUMBEROFQUEUES] = {0, 0};
char *queueNames[NUMBEROFQUEUES] = {"USER", "DAEMONS"};

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses = 0;

int numberOfClockInterrupts = 0; //V2 Ej 4

// In OperatingSystem.c Exercise 5-b of V2
// Heap with blocked processes sort by when to wakeup
heapItem sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfSleepingProcesses = 0;

// Initial set of tasks of the OS
void OperatingSystem_Initialize(int daemonsIndex)
{

	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code
	programFile = fopen("OperatingSystemCode", "r");
	if (programFile == NULL)
	{
		// Show red message "FATAL ERROR: Missing Operating System!\n"
		OperatingSystem_ShowTime(SHUTDOWN);
		ComputerSystem_DebugMessage(99, SHUTDOWN, "FATAL ERROR: Missing Operating System!\n");
		exit(1);
	}

	// Obtain the memory requirements of the program
	int processSize = OperatingSystem_ObtainProgramSize(programFile);

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);

	// Process table initialization (all entries are free)
	for (i = 0; i < PROCESSTABLEMAXSIZE; i++)
	{
		processTable[i].busy = 0;
	}
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base + 2);

	// Include in program list  all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);

	//V3 Eje 1-c 1-d
	ComputerSystem_FillInArrivalTimeQueue();
	OperatingSystem_PrintStatus();

	//V4 ej 5
	OperatingSystem_InitializePartitionTable();
	// Create all user processes from the information given in the command line
	OperatingSystem_LongTermScheduler();

	if (numberOfNotTerminatedUserProcesses <= 0 && numberOfProgramsInArrivalTimeQueue <= 0)
	{
		if (executingProcessID == sipID)
		{
			// finishing sipID, change PC to address of OS HALT instruction
			OperatingSystem_TerminatingSIP();
			OperatingSystem_ShowTime(SHUTDOWN);
			ComputerSystem_DebugMessage(99, SHUTDOWN, "The system will shut down now...\n");
			return; // Don't dispatch any process
		}
		OperatingSystem_ReadyToShutdown();
	}

	if (strcmp(programList[processTable[sipID].programListIndex]->executableName, "SystemIdleProcess"))
	{
		// Show red message "FATAL ERROR: Missing SIP program!\n"
		OperatingSystem_ShowTime(SHUTDOWN);
		ComputerSystem_DebugMessage(99, SHUTDOWN, "FATAL ERROR: Missing SIP program!\n");
		exit(1);
	}

	// At least, one user process has been created
	// Select the first process that is going to use the processor
	selectedProcess = OperatingSystem_ShortTermScheduler();

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);
}

// Daemon processes are system processes, that is, they work together with the OS.
// The System Idle Process uses the CPU whenever a user process is able to use it
int OperatingSystem_PrepareStudentsDaemons(int programListDaemonsBase)
{

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
int OperatingSystem_LongTermScheduler()
{

	int i, PID, numberOfSuccessfullyCreatedProcesses = 0;
	//V3 Ej  3
	//No se puede crear el proceso
	while (OperatingSystem_IsThereANewProgram() == YES)
	{

		//El for no funciona
		i = Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
		PID = OperatingSystem_CreateProcess(i);

		if (PID == NOFREEENTRY)
		{
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(103, ERROR, programList[i]->executableName);
		}
		if (PID == PROGRAMDOESNOTEXIST)
		{
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "it does not exist");
		}
		if (PID == PROGRAMNOTVALID)
		{
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "invalid priority or size");
		}
		if (PID == TOOBIGPROCESS)
		{
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(105, ERROR, programList[i]->executableName);
		}
		if (PID == MEMORYFULL) //V4 EJ 6
		{
			OperatingSystem_ShowTime(ERROR);
			ComputerSystem_DebugMessage(144, ERROR, programList[i]->executableName);
		}
		if (PID >= 0)
		{
			numberOfSuccessfullyCreatedProcesses++;
			if (programList[i]->type == USERPROGRAM)
				numberOfNotTerminatedUserProcesses++;
			// Move process to the ready state
			OperatingSystem_MoveToTheREADYState(PID);
		}
	}
	if (numberOfSuccessfullyCreatedProcesses > 0)
	{
		//V2 Ej 7
		OperatingSystem_PrintStatus();
	}
	else if (numberOfNotTerminatedUserProcesses <= 0 && numberOfProgramsInArrivalTimeQueue == 0)
	{
		OperatingSystem_ReadyToShutdown();
	}
	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}

// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram)
{

	int PID;
	int processSize;

	int priority;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram = programList[indexOfExecutableProgram];

	// Obtain a process ID
	PID = OperatingSystem_ObtainAnEntryInTheProcessTable();

	//if PID == NOFREEENTRY (-3)no es valido
	if (PID == NOFREEENTRY)
	{

		return NOFREEENTRY;
	}

	// Check if programFile exists
	programFile = fopen(executableProgram->executableName, "r");
	if (programFile == NULL)
	{

		return PROGRAMDOESNOTEXIST;
	}

	// Obtain the memory requirements of the program
	processSize = OperatingSystem_ObtainProgramSize(programFile);
	// Obtain the priority for the process
	priority = OperatingSystem_ObtainPriority(programFile);
	if (processSize == PROGRAMNOTVALID || priority == PROGRAMNOTVALID)
	{

		return PROGRAMNOTVALID;
	}

	//V4 ej7
	// Obtain enough memory space
	int particion = OperatingSystem_ObtainMainMemory(processSize, PID);
	if (particion < 0)
		return particion;

	//V4 ej 6 b
	OperatingSystem_ShowTime(SYSMEM);
	ComputerSystem_DebugMessage(142, SYSMEM, PID, programList[indexOfExecutableProgram]->executableName, processSize);

	//loadingPhysicalAddress=partitionsTable[particion].initAddress;

	// Load program in the allocated memory
	int errorNum = OperatingSystem_LoadProgram(programFile, partitionsTable[particion].initAddress, processSize);
	//int errorNum = OperatingSystem_LoadProgram(programFile, partitionsTable[particion].size, processSize);

	if (errorNum == TOOBIGPROCESS)
		return errorNum;

	processTable[PID].programListIndex = indexOfExecutableProgram;

	OperatingSystem_ShowPartitionTable("before allocating memory");

	partitionsTable[particion].PID = PID;

	//V4 Ej 6 -c
	OperatingSystem_ShowTime(SYSMEM);
	ComputerSystem_DebugMessage(143, SYSMEM, particion, partitionsTable[particion].initAddress, partitionsTable[particion].size, PID, executableProgram->executableName);

	// PCB initialization
	OperatingSystem_PCBInitialization(PID, partitionsTable[particion].initAddress, processSize, priority, indexOfExecutableProgram);
	//OperatingSystem_PCBInitialization(PID, partitionsTable[particion].size, processSize, priority, indexOfExecutableProgram);

	//V4 ej7
	OperatingSystem_ShowPartitionTable("after allocating memory");

	// Show message "Process [PID] created from program [executableName]\n"
	OperatingSystem_ShowTime(INIT);
	ComputerSystem_DebugMessage(70, INIT, PID, executableProgram->executableName);

	return PID;
}

// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID)
{
	int i = 0;
	int size = -1;
	//int size = MAINMEMORYSIZE;
	int bestPartition = -1;
	int partitionFound = 0;

	int asigned = 0;

	// int oldPartitionSize=PARTITIONTABLEMAXSIZE;
	for (i = 0; i < PARTITIONTABLEMAXSIZE; i++)
	{
		if (partitionsTable[i].size >= processSize)
		{
			partitionFound = 1;
			if (partitionsTable[i].PID == NOPROCESS)
			{
				//&& partitionsTable[i].size >= processSize ya se ve en lo siguiente
				if (partitionsTable[i].size < size || size == -1)
				{
					size = partitionsTable[i].size;
					bestPartition = i;

					asigned = 1;
				}
				else if (partitionsTable[i].size == size)
				{
					if (partitionsTable[i].initAddress == partitionsTable[bestPartition].initAddress)
					{
						size = partitionsTable[i].size;
						bestPartition = i;

						asigned = 1;
					}
				}
			}
		}
	}

	if (!asigned && partitionFound)
		return MEMORYFULL;
	if (!asigned && !partitionFound)
		return TOOBIGPROCESS;

	return bestPartition;
}

// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex)
{

	processTable[PID].busy = 1;
	processTable[PID].initialPhysicalAddress = initialPhysicalAddress;
	processTable[PID].processSize = processSize;
	processTable[PID].state = NEW;
	processTable[PID].priority = priority;
	processTable[PID].programListIndex = processPLIndex;
	//Ejeccicio 11 poner tipo
	processTable[PID].queueID = programList[processPLIndex]->type;
	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM)
	{
		processTable[PID].copyOfPCRegister = initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister = ((unsigned int)1) << EXECUTION_MODE_BIT;
	}
	else
	{
		processTable[PID].copyOfPCRegister = 0;
		processTable[PID].copyOfPSWRegister = 0;
		processTable[PID].copyOfAcummRegister = 0;
	}
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(111, SHORTTERMSCHEDULE, PID, programList[processTable[PID].programListIndex]->executableName, statesNames[processTable[PID].state]);
}

// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID)
{
	int queue = processTable[PID].queueID;
	if (Heap_add(PID, readyToRunQueue[queue], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[queue], PROCESSTABLEMAXSIZE) >= 0)
	{
		OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
		ComputerSystem_DebugMessage(110, SHORTTERMSCHEDULE, PID,
									programList[processTable[PID].programListIndex]->executableName,
									statesNames[processTable[PID].state],
									statesNames[READY]);

		processTable[PID].state = READY;
		//OperatingSystem_PrintReadyToRunQueue();
	}
}

// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
int OperatingSystem_ShortTermScheduler()
{

	int selectedProcess;

	selectedProcess = OperatingSystem_ExtractFromReadyToRun();

	return selectedProcess;
}

// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromReadyToRun()
{

	int selectedProcess = NOPROCESS;

	selectedProcess = Heap_poll(readyToRunQueue[USERPROCESSQUEUE], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[USERPROCESSQUEUE]);
	if (selectedProcess < 0)
		selectedProcess = Heap_poll(readyToRunQueue[DAEMONPROGRAM], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[DAEMONPROGRAM]);

	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess;
}

// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID)
{

	// The process identified by PID becomes the current executing process
	executingProcessID = PID;
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(110, SHORTTERMSCHEDULE, PID,
								programList[processTable[PID].programListIndex]->executableName,
								statesNames[processTable[PID].state],
								statesNames[EXECUTING]);
	// Change the process' state
	processTable[PID].state = EXECUTING;
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}

// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID)
{

	// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE - 1, processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE - 2, processTable[PID].copyOfPSWRegister);
	//Processor_CopyInSystemStack(MAINMEMORYSIZE-3,processTable[PID].copyOfAcummRegister);
	Processor_SetAccumulator(processTable[PID].copyOfAcummRegister);
	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
}

// Function invoked when the executing process leaves the CPU
void OperatingSystem_PreemptRunningProcess()
{
	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);
	// The processor is not assigned until the OS selects another process
	executingProcessID = NOPROCESS;
}

// Save in the process' PCB essential values stored in hardware registers and the system stack
void OperatingSystem_SaveContext(int PID)
{

	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister = Processor_CopyFromSystemStack(MAINMEMORYSIZE - 1);

	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister = Processor_CopyFromSystemStack(MAINMEMORYSIZE - 2);
	//Ejercicio 13 get acumulator

	processTable[PID].copyOfAcummRegister = Processor_GetAccumulator();
	//processTable[PID].copyOfAcummRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-3);
}

// Exception management routine
void OperatingSystem_HandleException()
{

	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	//OperatingSystem_ShowTime(SYSPROC);
	///ComputerSystem_DebugMessage(71, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);

	//V4 ej 2 y ej 4
	OperatingSystem_ShowTime(SYSPROC);
	//{DIVISIONBYZERO, INVALIDPROCESSORMODE, INVALIDADDRESS, INVALIDINSTRUCTION};
	switch (Processor_GetExceptionType())
	{

	case DIVISIONBYZERO:
		ComputerSystem_DebugMessage(140, INTERRUPT, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, "division by zero");
		break;
	case INVALIDPROCESSORMODE:
		ComputerSystem_DebugMessage(140, INTERRUPT, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, "invalid processor mode");
		break;
	case INVALIDADDRESS:
		ComputerSystem_DebugMessage(140, INTERRUPT, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, "invalid address");
		break;
	case INVALIDINSTRUCTION:
		ComputerSystem_DebugMessage(140, INTERRUPT, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, "invalid instruction");
		break;
	default:
		ComputerSystem_DebugMessage(140, INTERRUPT, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, "unknown reason");
		break;
	}

	OperatingSystem_TerminateProcess();
	//V2 Ej 7
	OperatingSystem_PrintStatus();
}

// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess()
{

	int selectedProcess;

	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(110, SHORTTERMSCHEDULE, executingProcessID,
								programList[processTable[executingProcessID].programListIndex]->executableName,
								statesNames[processTable[executingProcessID].state],
								statesNames[EXIT]);

	processTable[executingProcessID].state = EXIT;

	if (programList[processTable[executingProcessID].programListIndex]->type == USERPROGRAM)
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;
	OperatingSystem_ReleaseMainMemory(); //V4 Ej 8
	if (numberOfNotTerminatedUserProcesses == 0 && numberOfProgramsInArrivalTimeQueue <= 0)
	{
		if (executingProcessID == sipID)
		{
			// finishing sipID, change PC to address of OS HALT instruction
			OperatingSystem_TerminatingSIP();
			OperatingSystem_ShowTime(SHUTDOWN);
			ComputerSystem_DebugMessage(99, SHUTDOWN, "The system will shut down now...\n");
			return; // Don't dispatch any process
		}
		// Simulation must finish, telling sipID to finish
		OperatingSystem_ReadyToShutdown();
	}
	// Select the next process to execute (sipID if no more user processes)
	selectedProcess = OperatingSystem_ShortTermScheduler();

	// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}

// System call management routine
void OperatingSystem_HandleSystemCall()
{

	int systemCallID;

	int IDProceso, Prioridad;
	int oldProcess;
	int newProcess;

	// Register A contains the identifier of the issued system call
	systemCallID = Processor_GetRegisterA();

	switch (systemCallID)
	{
	case SYSCALL_PRINTEXECPID:
		// Show message: "Process [executingProcessID] has the processor assigned\n"
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(72, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
		break;

	case SYSCALL_END:
		// Show message: "Process [executingProcessID] has requested to terminate\n"
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(73, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
		OperatingSystem_TerminateProcess();
		//V2 ej 7
		OperatingSystem_PrintStatus();
		break;
	case SYSCALL_YIELD:

		//ProcesoActual
		oldProcess = executingProcessID;
		IDProceso = processTable[oldProcess].queueID;
		Prioridad = processTable[oldProcess].priority;

		//Siguiente Proceso
		newProcess = Heap_getFirst(readyToRunQueue[IDProceso], numberOfReadyToRunProcesses[IDProceso]);

		//Si tienen la misma prioridad
		if (numberOfReadyToRunProcesses[IDProceso] > 0)
		{
			if (Prioridad == processTable[newProcess].priority)
			{
				OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
				ComputerSystem_DebugMessage(115, SHORTTERMSCHEDULE, oldProcess, programList[processTable[oldProcess].programListIndex]->executableName, newProcess, programList[processTable[newProcess].programListIndex]->executableName);
				// Function invoked when the executing process leaves the CPU
				OperatingSystem_PreemptRunningProcess();
				// Select the next process to execute (sipID if no more user processes)
				//selectedProcess=OperatingSystem_ShortTermScheduler();
				OperatingSystem_Dispatch(OperatingSystem_ShortTermScheduler());

				//V2 ej 7
				OperatingSystem_PrintStatus();
			}
		}
		break;
	//V2 ej 5
	case SYSCALL_SLEEP:

		OperatingSystem_MoveToTheBLOCKState();
		//Pone el siguiente proceso
		OperatingSystem_Dispatch(OperatingSystem_ShortTermScheduler());

		OperatingSystem_PrintStatus();
		break;

	default: //V4 EJ 4
		ComputerSystem_ShowTime(INTERRUPT);
		//141
		ComputerSystem_DebugMessage(141, INTERRUPT, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, systemCallID);
		OperatingSystem_TerminateProcess();
		OperatingSystem_PrintStatus();
		break;
	}
}

//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint)
{
	switch (entryPoint)
	{
	case SYSCALL_BIT: // SYSCALL_BIT=2
		OperatingSystem_HandleSystemCall();
		break;
	case EXCEPTION_BIT: // EXCEPTION_BIT=6
		OperatingSystem_HandleException();
		break;
	case CLOCKINT_BIT: // CLOCKINT_BIT=9 V2 Ej 2
		OperatingSystem_HandleClockInterrupt();
		break;
	}
}

void OperatingSystem_PrintReadyToRunQueue()
{
	int queue;
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE);
	for (queue = 0; queue < NUMBEROFQUEUES; queue++)
	{
		//Nombre de la queue
		ComputerSystem_DebugMessage(109, SHORTTERMSCHEDULE, queueNames[queue]);
		int i;
		for (i = 0; i < numberOfReadyToRunProcesses[queue] - 1; i++)
		{
			//Los valores
			ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE, readyToRunQueue[queue][i].info, processTable[readyToRunQueue[queue][i].info].priority);
		}
		if (numberOfReadyToRunProcesses[queue] != 0)
			ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, readyToRunQueue[queue][i].info, processTable[readyToRunQueue[queue][i].info].priority);
		else
		{
			ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE);
		}
	}
}
// In OperatingSystem.c Exercise 2-b of V2
void OperatingSystem_HandleClockInterrupt()
{

	numberOfClockInterrupts++;
	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	OperatingSystem_ShowTime(INTERRUPT);
	ComputerSystem_DebugMessage(120, INTERRUPT, numberOfClockInterrupts);

	//V2 Eje 6
	int n = 0;
	int process = Heap_getFirst(sleepingProcessesQueue, numberOfSleepingProcesses);
	//int processAux = NOPROCESS;
	while (processTable[process].whenToWakeUp == numberOfClockInterrupts)
	{
		//6-a poner a ready procesos
		OperatingSystem_MoveToTheREADYState(Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses));
		n++;
		process = Heap_getFirst(sleepingProcessesQueue, numberOfSleepingProcesses);
		//Coprobar porceso? comprobar porcesos restantes
	}

	int newProcess = OperatingSystem_LongTermScheduler();

	// 6-b Si se ha despertado un proceso mirar
	if (n > 0 || newProcess > 0)
	{
		OperatingSystem_PrintStatus();

		for (int i = 0; (i <= processTable[executingProcessID].queueID); i++)
		{
			int proceso = Heap_getFirst(readyToRunQueue[i], numberOfReadyToRunProcesses[i]);
			if (comparePrioritys(proceso, executingProcessID))
			{
				OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
				ComputerSystem_DebugMessage(125, SHORTTERMSCHEDULE, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, proceso, programList[processTable[proceso].programListIndex]->executableName);
				//ComputerSystem_DebugMessage(110, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, statesNames[2], statesNames[3]);
				OperatingSystem_PreemptRunningProcess();
				OperatingSystem_Dispatch(OperatingSystem_ShortTermScheduler());
				OperatingSystem_PrintStatus();
				break;
			}
		}
	}
	else
	{
		if (numberOfNotTerminatedUserProcesses <= 0 && numberOfProgramsInArrivalTimeQueue <= 0)
		{
			// if (executingProcessID == sipID)
			// {
			// 	// finishing sipID, change PC to address of OS HALT instruction
			// 	OperatingSystem_TerminatingSIP();
			// 	OperatingSystem_ShowTime(SHUTDOWN);
			// 	ComputerSystem_DebugMessage(99, SHUTDOWN, "The system will shut down now...\n");
			// 	return; // Don't dispatch any process
			// }
			OperatingSystem_ReadyToShutdown();
		}
	}

	return;
}

int comparePrioritys(int a, int b)
{
	if (processTable[a].queueID == USERPROCESSQUEUE && processTable[b].queueID == DAEMONSQUEUE)
		return 1;
	if (processTable[a].queueID == processTable[b].queueID && processTable[a].priority < processTable[b].priority)
		return 1;

	return 0;
}

//Exercise 5-b of V2
// Heap with blocked processes sort by when to wakeup
//heapItem sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
//int numberOfSleepingProcesses=0;
//Se bloqueará al proceso en ejecución
//insertará por orden creciente del campo whenToWakeUp en la sleepingProcessesQueue.
void OperatingSystem_MoveToTheBLOCKState()
{
	OperatingSystem_SaveContext(executingProcessID);
	//No es necesario ya que sleepingProcessesQueue no diferencia entre daemons y programas
	//int q = processTable[executingProcessID].queueID;
	processTable[executingProcessID].whenToWakeUp = abs(Processor_GetAccumulator()) + numberOfClockInterrupts + 1;
	if (Heap_add(executingProcessID, sleepingProcessesQueue, QUEUE_WAKEUP,
				 &numberOfSleepingProcesses, PROCESSTABLEMAXSIZE) >= 0)
	{

		//Se bloqueará al proceso en ejecución
		processTable[executingProcessID].state = BLOCKED;

		//whenToWakeUp= valor absoluto del valor actual del registro acumulador, al número de interrupciones de reloj que se hayan
		//producido hasta el momento, más una unidad adicional
		processTable[executingProcessID].whenToWakeUp = abs(Processor_GetAccumulator()) + numberOfClockInterrupts + 1;
		OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
		ComputerSystem_DebugMessage(110, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, statesNames[2], statesNames[3]);
	}
	else
	{
		OperatingSystem_RestoreContext(executingProcessID);
	}
}

int OperatingSystem_GetExecutingProcessID()
{
	return executingProcessID;
}

//haciendo pid = no Process
//V4 Ej 8
void OperatingSystem_ReleaseMainMemory()
{
	for (int i = 0; i <= PARTITIONTABLEMAXSIZE; i++)
	{
		if (partitionsTable[i].PID == executingProcessID)
		{
			OperatingSystem_ShowPartitionTable("before releasing memory");

			partitionsTable[i].PID = NOPROCESS;

			OperatingSystem_ShowTime(SYSMEM);
			ComputerSystem_DebugMessage(145, SYSMEM, i, partitionsTable[i].initAddress, partitionsTable[i].size,
										executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
			OperatingSystem_ShowPartitionTable("after releasing memory");
			break;
		}
	}
}