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
void OperatingSystem_PCBInitialization(int, int, int, int, int, int); // Store queueID
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
void OperatingSystem_HandleClockInterrupt(); // V2 Ex 2
int OperatingSystem_ExtractFromSleepingProcessesQueue(); // V2-Ex6
void OperatingSystem_ReleaseMainMemory(); // V4-Ex8

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

// Begin indes for daemons in programList
int baseDaemonsInProgramList; 

// Array that contains the identifiers of the READY processes
//heapItem readyToRunQueue[PROCESSTABLEMAXSIZE];

// Exercise 11
heapItem readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];

//int numberOfReadyToRunProcesses=0;

// Exercise 11
int numberOfReadyToRunProcesses[NUMBEROFQUEUES]={0,0};

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses=0;

char * statesNames[5]={"NEW","READY","EXECUTING","BLOCKED","EXIT"}; 

// Exercise 11
char * queueNames [NUMBEROFQUEUES]={"USER","DAEMONS"};

// V2-Ex4 begin
int numberOfClockInterrupts = 0;
// V2-Ex4 end

// In OperatingSystem.c Exercise 5-b of V2
// Heap with blocked processes sorted by when to wakeup
heapItem sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfSleepingProcesses=0;
// V2-Ex5b

// Initial set of tasks of the OS
void OperatingSystem_Initialize(int daemonsIndex) {
	
	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code
	programFile=fopen("OperatingSystemCode", "r");
	if (programFile==NULL){
		
		// Show red message "FATAL ERROR: Missing Operating System!\n"
		ComputerSystem_ShowTime(SHUTDOWN); // V2 Ex1
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

	OperatingSystem_InitializePartitionTable(); // V4-Ex5

	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base+2);
		
	// Include in program list  all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);

	// V3-Ex1c begin
	ComputerSystem_FillInArrivalTimeQueue();
	// V3-Ex1c end

	// V3-Ex1d begin
	OperatingSystem_PrintStatus();
	// V3-Ex1d end
	
	// Create all user processes from the information given in the command line
	OperatingSystem_LongTermScheduler();

	// V1ex15
	if (numberOfNotTerminatedUserProcesses==0 && OperatingSystem_IsThereANewProgram()==EMPTYQUEUE) { // V3-Ex5a
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
int OperatingSystem_LongTermScheduler() {
  
	int PID, i,
		numberOfSuccessfullyCreatedProcesses=0;

	// V3-Ex3 begin
	while(OperatingSystem_IsThereANewProgram() == YES){ // There are more programs with valid arrivalTime

			i = Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
			// V3-Ex3 end

			PID=OperatingSystem_CreateProcess(i);
			switch (PID)
			{
			case NOFREEENTRY:
				ComputerSystem_ShowTime(SHUTDOWN); // V3 Fix
				ComputerSystem_DebugMessage(103, ERROR, programList[i]->executableName);
				break;
			case PROGRAMDOESNOTEXIST:
				ComputerSystem_ShowTime(SHUTDOWN); // V3 Fix
				ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "it does not exist");
				break;
			case PROGRAMNOTVALID:
				ComputerSystem_ShowTime(SHUTDOWN); // V3 Fix
				ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "invalid priority or size");
				break;
			case TOOBIGPROCESS:
				ComputerSystem_ShowTime(SHUTDOWN); // V3 Fix
				ComputerSystem_DebugMessage(105, ERROR, programList[i]->executableName);
				break;
			// V4-Ex6d begin
			case MEMORYFULL:
				ComputerSystem_ShowTime(SHUTDOWN);
				ComputerSystem_DebugMessage(144, ERROR, programList[i]->executableName);
				break;
			// V4-Ex6d end

			default:
				numberOfSuccessfullyCreatedProcesses++;
				if (programList[i]->type==USERPROGRAM) {
					numberOfNotTerminatedUserProcesses++;
				}

				// Move process to the ready state
				OperatingSystem_MoveToTheREADYState(PID);

				break;
			}
	}

	// V2-Ex7 begin V2 Fix
	if(numberOfSuccessfullyCreatedProcesses >= 1){ // V4 Delivery 1 FIX >= instead of >
		OperatingSystem_PrintStatus();
	}
	// V2-Ex7 end

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

	if(PID==NOFREEENTRY){
		return NOFREEENTRY;
	}

	// Check if programFile exists
	programFile=fopen(executableProgram->executableName, "r");
	if (programFile==NULL){
		return PROGRAMDOESNOTEXIST;
	}

	// Obtain the memory requirements of the program
	processSize=OperatingSystem_ObtainProgramSize(programFile);	

	if(processSize == PROGRAMNOTVALID){
		return PROGRAMNOTVALID;
	}

	// Obtain the priority for the process
	priority=OperatingSystem_ObtainPriority(programFile);

	if(priority == PROGRAMNOTVALID){
		return PROGRAMNOTVALID;
	}

	OperatingSystem_ShowPartitionTable("before allocating memory"); // V4-Ex7

	// V4-Ex6b begin
	ComputerSystem_ShowTime(SHUTDOWN);
	ComputerSystem_DebugMessage(142, SYSMEM, PID, 
	executableProgram->executableName, processSize);
	// V4-Ex6b end

	// OperatingSystem_ShowPartitionTable("before allocating memory"); // V4-Ex7
	
	// Obtain enough memory space
 	loadingPhysicalAddress=OperatingSystem_ObtainMainMemory(processSize, PID);

	if(loadingPhysicalAddress == TOOBIGPROCESS){
		return TOOBIGPROCESS;
	}  else if (loadingPhysicalAddress == MEMORYFULL){// V4-Ex6c begin
	  	return MEMORYFULL;
	} else{
		
	}
	// V4-Ex6c end

	// Load program in the allocated memory
	// V4-Ex6b begin
	int loadResult = OperatingSystem_LoadProgram(programFile, partitionsTable[loadingPhysicalAddress].initAddress, processSize);
	// V4-Ex6b end

	if(loadResult == TOOBIGPROCESS){
		return TOOBIGPROCESS;
	}

	// PCB initialization
	// V4-Ex6b begin
	OperatingSystem_PCBInitialization(PID, partitionsTable[loadingPhysicalAddress].initAddress, processSize, priority, indexOfExecutableProgram, programList[indexOfExecutableProgram]->type);
	// V4-Ex6b end
	
	ComputerSystem_ShowTime(SHUTDOWN);
		ComputerSystem_DebugMessage(143,SYSMEM,
		loadingPhysicalAddress,partitionsTable[loadingPhysicalAddress].initAddress, 
		partitionsTable[loadingPhysicalAddress].size,
		PID, executableProgram->executableName); 

	OperatingSystem_ShowPartitionTable("after allocating memory"); // V4-Ex7

	ComputerSystem_ShowTime(SHUTDOWN); // V2 Ex 1
	// Show message "Process [PID] created from program [executableName]\n"
	ComputerSystem_DebugMessage(70,INIT,PID,executableProgram->executableName);
	
	return PID;
}


// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID) {

	int difference = __INT_MAX__;
	int selectedPartition = NOFREEENTRY;
	int fits = 0;

	for(int i = 0; i < PARTITIONTABLEMAXSIZE; i++){
		if(processSize <= partitionsTable[i].size) { // Process fits
			fits = 1;
			if(partitionsTable[i].PID==NOPROCESS){ // Partition is free
				if(partitionsTable[i].size - processSize < difference){
					difference = partitionsTable[i].size - processSize;
					selectedPartition = i;
				}
			}
		}
	}

	if(selectedPartition==NOFREEENTRY){ // No valid partition has been found
		// V4-Ex6dii begin
		if(fits == 1){
			return MEMORYFULL; 
		} else if (fits == 0){
			return TOOBIGPROCESS;
		}
		// V4-Ex6dii end
	} else {
		
		partitionsTable[selectedPartition].PID = PID;
		return selectedPartition;
	}
	// V4-Ex6b end
	
 	return selectedPartition;
}


// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex, int queueID) {

	processTable[PID].busy=1;
	processTable[PID].initialPhysicalAddress=initialPhysicalAddress;
	processTable[PID].processSize=processSize;
	processTable[PID].state=NEW;
	processTable[PID].priority=priority;
	processTable[PID].programListIndex=processPLIndex;
	
	processTable[PID].queueID=queueID;

	processTable[PID].copyOfAccumulatorRegister=0;

	

	ComputerSystem_ShowTime(SHUTDOWN); // V2 Ex 1
	ComputerSystem_DebugMessage(111,SYSPROC,PID,programList[processTable[PID].programListIndex]->executableName,statesNames[0]);

	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM) {
		processTable[PID].copyOfPCRegister=initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister= ((unsigned int) 1) << EXECUTION_MODE_BIT;
	} 
	else {
		processTable[PID].copyOfPCRegister=0;
		processTable[PID].copyOfPSWRegister=0;
	}

	

}

// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID) {

	if (Heap_add(PID, readyToRunQueue[programList[processTable[PID].programListIndex]->type],QUEUE_PRIORITY ,
	&numberOfReadyToRunProcesses[programList[processTable[PID].programListIndex]->type] ,PROCESSTABLEMAXSIZE)>=0) {

		// V1-Exercise 10
		int previousState = processTable[PID].state;
		processTable[PID].state=READY;
		ComputerSystem_ShowTime(SHUTDOWN); // V2 Ex 1
		ComputerSystem_DebugMessage(110,SYSPROC,PID,programList[processTable[PID].programListIndex]->executableName,
		statesNames[previousState],statesNames[processTable[PID].state]);
		// V1-Exercise 10

	} 

	// V2-Ex8
	//OperatingSystem_PrintReadyToRunQueue();
}


// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
int OperatingSystem_ShortTermScheduler() {
	
	int selectedProcess = NOPROCESS;

	for(int i = 0; i < NUMBEROFQUEUES && selectedProcess == NOPROCESS; i++){
		selectedProcess=OperatingSystem_ExtractFromReadyToRun(i);
	}
	
	return selectedProcess;
}


// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromReadyToRun(int queueID) {
  
	int selectedProcess=NOPROCESS;

	selectedProcess=Heap_poll(readyToRunQueue[queueID],QUEUE_PRIORITY ,&numberOfReadyToRunProcesses[queueID]);
	
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess; 
}


// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID) {

	// The process identified by PID becomes the current executing process
	executingProcessID=PID;
	// Change the process' state
	// V1-Ex10
	int previousState = processTable[PID].state;
	processTable[PID].state=EXECUTING;
	ComputerSystem_ShowTime(SHUTDOWN); // V2 Ex 1
	ComputerSystem_DebugMessage(110,SYSPROC,PID,
	programList[processTable[PID].programListIndex]->executableName,
	statesNames[previousState], statesNames[processTable[PID].state]);
	//OperatingSystem_PrintStatus(SHUTDOWN); // V2 Fix 
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}


// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID) {
  
	// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE-1,processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE-2,processTable[PID].copyOfPSWRegister);
	
	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);

	// V1-Ex13
	Processor_SetAccumulator(processTable[PID].copyOfAccumulatorRegister);
	// V1-Ex13
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

	// V1-Ex13
	processTable[PID].copyOfAccumulatorRegister=Processor_GetAccumulator();
	// V1-Ex13 
	
}


// Exception management routine
void OperatingSystem_HandleException() {

	ComputerSystem_ShowTime(SHUTDOWN); // V2 Ex 1
  
	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"

	// V4-Ex2 begin
	switch (Processor_GetRegisterB())
	{
	case DIVISIONBYZERO:
		ComputerSystem_DebugMessage(140,INTERRUPT,executingProcessID,
		programList[processTable[executingProcessID].programListIndex]->executableName, "division by zero");
		break;
	
	case INVALIDPROCESSORMODE:
		ComputerSystem_DebugMessage(140,INTERRUPT,executingProcessID,
		programList[processTable[executingProcessID].programListIndex]->executableName, "invalid processor mode");
		break;
	
	case INVALIDADDRESS:
		ComputerSystem_DebugMessage(140,INTERRUPT,executingProcessID,
		programList[processTable[executingProcessID].programListIndex]->executableName, "invalid address");
		break;

	// V4-Ex3b begin
	case INVALIDINSTRUCTION:
		ComputerSystem_DebugMessage(140,INTERRUPT,executingProcessID,
		programList[processTable[executingProcessID].programListIndex]->executableName, "invalid instruction");
		break;
	// V4-Ex3b end

	default:
		break;
	}
	// V4-Ex2 end

	OperatingSystem_TerminateProcess();

	// V2-Ex7 begin
	OperatingSystem_PrintStatus();
	// V2-Ex7 end
}


// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess() {
  
	int selectedProcess;
  	
	int previousState = processTable[executingProcessID].state;
	
	processTable[executingProcessID].state=EXIT;

	ComputerSystem_ShowTime(SHUTDOWN); // V2 Ex 1
	ComputerSystem_DebugMessage(110,SYSPROC, executingProcessID,
	programList[processTable[executingProcessID].programListIndex]->executableName,
	statesNames[previousState],statesNames[processTable[executingProcessID].state]);

	if (programList[processTable[executingProcessID].programListIndex]->type==USERPROGRAM) 
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;
	
	// test fix v4-ex4
	//executingProcessID = OperatingSystem_ShortTermScheduler();

	// V4-Ex8 begin
	OperatingSystem_ReleaseMainMemory();
	// V4-Ex8 end

	if (numberOfNotTerminatedUserProcesses==0 && OperatingSystem_IsThereANewProgram()==EMPTYQUEUE) { // V3-Ex5c
		if (executingProcessID==sipID) {
			// finishing sipID, change PC to address of OS HALT instruction
			OperatingSystem_TerminatingSIP();

			ComputerSystem_ShowTime(SHUTDOWN); // V2 Ex 1
			
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

	// Register A contains the identifier of the issued system call
	systemCallID=Processor_GetRegisterA();
	
	switch (systemCallID) {
		case SYSCALL_PRINTEXECPID:

			ComputerSystem_ShowTime(SYSPROC); // V2 Ex 1

			// Show message: "Process [executingProcessID] has the processor assigned\n"
			ComputerSystem_DebugMessage(72,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			break;

		case SYSCALL_END:

			ComputerSystem_ShowTime(SYSPROC); // V2 Ex 1

			// Show message: "Process [executingProcessID] has requested to terminate\n"
			ComputerSystem_DebugMessage(73,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			OperatingSystem_TerminateProcess();

			// V2-Ex7 begin
			OperatingSystem_PrintStatus();
			// V2-Ex7 end

			break;
		
		// EX 12
		case SYSCALL_YIELD: {
		
			int currentQID = processTable[executingProcessID].queueID;
			if(numberOfReadyToRunProcesses[currentQID] > 0){
				
				int currentPriority = processTable[executingProcessID].priority;
				int firstReadyPID = readyToRunQueue[currentQID][0].info;
				if(currentPriority == processTable[firstReadyPID].priority){
					int oldPID = executingProcessID; // V2 Fix
					ComputerSystem_ShowTime(SHUTDOWN);
					ComputerSystem_DebugMessage(115, SHORTTERMSCHEDULE, executingProcessID, 
					programList[processTable[executingProcessID].programListIndex]->executableName, firstReadyPID,
					programList[processTable[firstReadyPID].programListIndex]->executableName);
					// V4 Fix prueba
					OperatingSystem_PreemptRunningProcess();
					int selectedPID = OperatingSystem_ExtractFromReadyToRun(currentQID);//OperatingSystem_ShortTermScheduler();
					// V4 Fix prueba
					OperatingSystem_Dispatch(selectedPID);

					// V2-Ex7 begin
					if (oldPID != selectedPID)
					{
						OperatingSystem_PrintStatus();
					}
					// V2-Ex7 end
				}
			}

			break;
		}

		// V2-Ex5d begin
		case SYSCALL_SLEEP: {
			
			processTable[executingProcessID].whenToWakeUp = abs(Processor_GetAccumulator()) + numberOfClockInterrupts + 1; // Calculate and set the whenToWakeUp

			if (Heap_add(executingProcessID, sleepingProcessesQueue, QUEUE_WAKEUP,
				&numberOfSleepingProcesses ,PROCESSTABLEMAXSIZE)>=0){ // Add the element to the heap


				int previousState = processTable[executingProcessID].state; // V2 Fix

				//processTable[executingProcessID].whenToWakeUp = abs(Processor_GetAccumulator()) + numberOfClockInterrupts + 1; // Calculate and set the whenToWakeUp

				OperatingSystem_SaveContext(executingProcessID); // Save the context before changing its state
				processTable[executingProcessID].state=BLOCKED; // Change the state to blocked

				// V2 Fix
				OperatingSystem_ShowTime(SHUTDOWN);
				ComputerSystem_DebugMessage(110,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,
				statesNames[previousState],statesNames[processTable[executingProcessID].state]);

				// Put another process in the processor
				int selectedPID = OperatingSystem_ShortTermScheduler();
				OperatingSystem_Dispatch(selectedPID);

				// Print the status of the system
				OperatingSystem_PrintStatus();


			}

			break;
		}
		// V2-Ex5d end

		// V4-Ex4a begin
		default: {

			ComputerSystem_ShowTime(SHUTDOWN);
			ComputerSystem_DebugMessage(141,INTERRUPT,executingProcessID,
			programList[processTable[executingProcessID].programListIndex]->executableName, systemCallID);

			// V4-Ex4b begin
			OperatingSystem_TerminateProcess();
			// V4-Ex4b end

			// V4-Ex4c begin
			OperatingSystem_PrintStatus();
			// V4-Ex4c end
		}
		// V4-Ex4a end

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
		// V2 Ex 2
		case CLOCKINT_BIT: // EXCEPTION_BIT=9
			OperatingSystem_HandleClockInterrupt();
			break;
	}

}

/* Exercise 9
void OperatingSystem_PrintReadyToRunQueue(){
	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE);

	if(numberOfReadyToRunProcesses > 0){
		ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE, readyToRunQueue[0].info, readyToRunQueue[0].insertionOrder);
	} else{
		return; // No elements in the ready to run queue
	}

	for(int i = 1; i < numberOfReadyToRunProcesses - 1; i++){
		ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, readyToRunQueue[i].info, readyToRunQueue[i].insertionOrder);
	}
	
	if(numberOfReadyToRunProcesses >= 2){
		ComputerSystem_DebugMessage(109, SHORTTERMSCHEDULE, readyToRunQueue[numberOfReadyToRunProcesses-1].info, readyToRunQueue[numberOfReadyToRunProcesses-1].insertionOrder);
	}
	
} */

void OperatingSystem_PrintReadyToRunQueue(){
	int i;

	ComputerSystem_ShowTime(SHUTDOWN); // V2 Ex 1
	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE,queueNames[USERPROCESSQUEUE]);

	for(i = 0; i < numberOfReadyToRunProcesses[USERPROCESSQUEUE] - 1; i++){
		ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE, 
		readyToRunQueue[USERPROCESSQUEUE][i], 
		processTable[readyToRunQueue[USERPROCESSQUEUE][i].info].priority);
	}

	if(i + 1 == numberOfReadyToRunProcesses[USERPROCESSQUEUE]){
		ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, 
		readyToRunQueue[USERPROCESSQUEUE][i], 
		processTable[readyToRunQueue[USERPROCESSQUEUE][i].info].priority);
	} else{
		ComputerSystem_DebugMessage(113,SHORTTERMSCHEDULE);
	}

	ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE,queueNames[DAEMONSQUEUE]);

	for(i = 0; i < numberOfReadyToRunProcesses[DAEMONSQUEUE] - 1; i++){
		ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE, 
		readyToRunQueue[DAEMONSQUEUE][i], 
		processTable[readyToRunQueue[DAEMONSQUEUE][i].info].priority);
	}

	if(i + 1 == numberOfReadyToRunProcesses[DAEMONSQUEUE]){
		ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, 
		readyToRunQueue[DAEMONSQUEUE][i], 
		processTable[readyToRunQueue[DAEMONSQUEUE][i].info].priority);
	} else{
		ComputerSystem_DebugMessage(113,SHORTTERMSCHEDULE);
	}
	
}
	// V2 Ex 2 begin
void OperatingSystem_HandleClockInterrupt(){
	
	// V2-Ex4 begin
	OperatingSystem_ShowTime(INTERRUPT);
	numberOfClockInterrupts++;
	ComputerSystem_DebugMessage(120,INTERRUPT,numberOfClockInterrupts);
	int nReady = numberOfReadyToRunProcesses[USERPROGRAM] + numberOfReadyToRunProcesses[DAEMONPROGRAM];

	// V2-Ex4 end

	// V2-Ex6 begin
	// V2-Ex6a
	int selectedProcess = NOPROCESS;

	for(int i = 0; i < numberOfSleepingProcesses; i++){ // Process the queue

		// Compare the whenToWakeUp of the first element of the heap to the number of interrupts
		if (processTable[Heap_getFirst(sleepingProcessesQueue, numberOfSleepingProcesses)].whenToWakeUp == numberOfClockInterrupts)
		{
			// If equal, extract it from the queue and put it in the ready to run queue
			selectedProcess = OperatingSystem_ExtractFromSleepingProcessesQueue();
			OperatingSystem_MoveToTheREADYState(selectedProcess);
		}
	}

	// V2-Ex6a
	OperatingSystem_LongTermScheduler();

	// V2-Ex6b begin
	if(numberOfNotTerminatedUserProcesses == 0 && OperatingSystem_IsThereANewProgram() == EMPTYQUEUE && executingProcessID == sipID){
		OperatingSystem_ReadyToShutdown(); // V3 Fix
	}
	// V2-Ex6b end

	int nReadyNew = numberOfReadyToRunProcesses[USERPROGRAM] + numberOfReadyToRunProcesses[DAEMONPROGRAM];

	// V3-Ex4 begin
	if(nReadyNew > nReady){ // At least a process has been unblocked
	// V3-Ex4 end
	
		OperatingSystem_PrintStatus(); // V2 Fix

		// V2-Ex6c	
		int currentPriority = processTable[executingProcessID].priority;
		int selectedPID = OperatingSystem_ShortTermScheduler();
		int newPriority = processTable[selectedPID].priority;

		// Check if the executing process is the one with the highest priority, if not, interchange it with the one that has the lowest
		if(newPriority < currentPriority){
			if(executingProcessID != selectedPID){
				
				OperatingSystem_ShowTime(INTERRUPT);
				ComputerSystem_DebugMessage(121, SHORTTERMSCHEDULE, executingProcessID, 
				programList[processTable[executingProcessID].programListIndex]->executableName, selectedPID,
				programList[processTable[selectedPID].programListIndex]->executableName);

				OperatingSystem_PreemptRunningProcess();
				OperatingSystem_Dispatch(selectedPID);
				// V2-Ex6d
				OperatingSystem_PrintStatus();
			}
		} else{ // In case the process taken from the short term scheduler is not used, we put it back in the queue
			Heap_add(selectedPID, readyToRunQueue[processTable[selectedPID].queueID], QUEUE_WAKEUP, // V4Final-Fix
			&numberOfReadyToRunProcesses[processTable[selectedPID].queueID], PROCESSTABLEMAXSIZE);
		}
	} 
	// V2-Ex6 end
	
	return;	
}

// V2-Ex6a begin
// Return PID of more priority process in the sleeping processes queue
int OperatingSystem_ExtractFromSleepingProcessesQueue() {
  
	int selectedProcess=NOPROCESS;

	selectedProcess=Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses);
	
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess; 
}
// V2-Ex6a end

// V3-E2 begin
int OperatingSystem_GetExecutingProcessID(){

	return executingProcessID;
}
// V3-E2 end

// V4-Ex8 begin
void OperatingSystem_ReleaseMainMemory(){

	OperatingSystem_ShowPartitionTable("before releasing memory");
	for(int i = 0; i < PARTITIONTABLEMAXSIZE; i++){
		if(partitionsTable[i].PID==executingProcessID){ 
			partitionsTable[i].PID = NOPROCESS;
			ComputerSystem_ShowTime(SHUTDOWN);
			ComputerSystem_DebugMessage(145,SYSMEM,
			i,partitionsTable[i].initAddress, partitionsTable[i].size,
			executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName); 
			break;
		}
	}
	OperatingSystem_ShowPartitionTable("after releasing memory");
	
}
// V4-Ex8 end
