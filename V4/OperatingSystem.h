#ifndef OPERATINGSYSTEM_H
#define OPERATINGSYSTEM_H

#include "ComputerSystem.h"
#include <stdio.h>



#define SUCCESS 1
#define PROGRAMDOESNOTEXIST -1
#define PROGRAMNOTVALID -2

// In this version, every process occupies a 60 positions main memory chunk 
// so we can use 60 positions for OS code and the system stack
#define MAINMEMORYSECTIONSIZE (MAINMEMORYSIZE / (PROCESSTABLEMAXSIZE+1))

#define NOFREEENTRY -3
#define TOOBIGPROCESS -4

#define NOPROCESS -1

//Ejercicio 11
#define NUMBEROFQUEUES 2

//V2 ej 5
#define SLEEPINGQUEUE

//V4 ej 4
// Partitions configuration file name definition
#define MEMCONFIG "MemConfig" // in OperatingSystem.h 
//V4 eJ 6
#define MEMORYFULL -5 // In OperatingSystem.h


enum TypeOfReadyToRunProcessQueues { USERPROCESSQUEUE, DAEMONSQUEUE};

// Contains the possible type of programs
enum ProgramTypes { USERPROGRAM, DAEMONPROGRAM }; 

// Enumerated type containing all the possible process states
enum ProcessStates { NEW, READY, EXECUTING, BLOCKED, EXIT};

// Enumerated type containing the list of system calls and their numeric identifiers
enum SystemCallIdentifiers { SYSCALL_END=3,SYSCALL_YIELD=4, SYSCALL_PRINTEXECPID=5, SYSCALL_SLEEP=7};

// A PCB contains all of the information about a process that is needed by the OS
typedef struct {
	int busy;
	int initialPhysicalAddress;
	int processSize;
	int state;
	int priority;
	int copyOfPCRegister;
	unsigned int copyOfPSWRegister;
	int copyOfAcummRegister; //Eje 13
	int programListIndex;
	int queueID;
	int whenToWakeUp; // Exercise 5-a of V2
} PCB;

// These "extern" declaration enables other source code files to gain access
// to the variable listed
extern PCB processTable[];
extern int OS_address_base;
extern int sipID;

// Functions prototypes
void OperatingSystem_Initialize();
void OperatingSystem_InterruptLogic(int);
// void OperatingSystem_PrepareDaemons();
int OperatingSystem_PrepareStudentsDaemons(int);

int OperatingSystem_GetExecutingProcessID();


#endif
