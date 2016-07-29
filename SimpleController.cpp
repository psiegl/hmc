/*********************************************************************************
*  Copyright (c) 2010-2011, Elliott Cooper-Balis
*                             Paul Rosenfeld
*                             Bruce Jacob
*                             University of Maryland 
*                             dramninjas [at] gmail [dot] com
*  All rights reserved.
*  
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*  
*     * Redistributions of source code must retain the above copyright notice,
*        this list of conditions and the following disclaimer.
*  
*     * Redistributions in binary form must reproduce the above copyright notice,
*        this list of conditions and the following disclaimer in the documentation
*        and/or other materials provided with the distribution.
*  
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
*  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
*  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
*  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************************************************************/

//Simple Controller source

#include "SimpleController.h"
#include "DRAMChannel.h"
#include "BusPacket.h"
#include <math.h>

using namespace std;
using namespace BOBSim;

SimpleController::SimpleController(DRAMChannel *parent) :
	refreshCounter(0),
	readCounter(0),
	writeCounter(0),
	commandQueueMax(0),
	commandQueueAverage(0),
	numIdleBanksAverage(0),
	numActBanksAverage(0),
	numPreBanksAverage(0),
	numRefBanksAverage(0),
	RRQFull(0),
	waitingACTS(0),
	idd2nCount(0),
	rankBitWidth(log2(NUM_RANKS)),
	bankBitWidth(log2(NUM_BANKS)),
	rowBitWidth(log2(NUM_ROWS)),
	colBitWidth(log2(NUM_COLS)),
	busOffsetBitWidth(log2(BUS_ALIGNMENT_SIZE)),
	channelBitWidth(log2(NUM_CHANNELS)),
	cacheOffset(log2(CACHE_LINE_SIZE)),
    outstandingReads(0),
    currentClockCycle(0)

{
	//Registers the parent channel object
    channel = parent;

	//Make the bank state objects
	bankStates = (BankState**) malloc(sizeof(BankState*)*(NUM_RANKS));
	for(unsigned i=0; i<NUM_RANKS; i++)
	{
		bankStates[i]= (BankState*) calloc(sizeof(BankState), NUM_BANKS);
	}

	//Used to keep track of refreshes 
	refreshCounters = vector<unsigned>(NUM_RANKS,0);

	//make tFAW sliding window - one per rank
	tFAWWindow.reserve(NUM_RANKS);
	for(unsigned i=0; i<NUM_RANKS; i++)
	{
		tFAWWindow.push_back(vector<unsigned>());
	}


	//init power fields
	backgroundEnergy = vector<uint64_t>(NUM_RANKS,0);
	burstEnergy = vector<uint64_t>(NUM_RANKS,0);
	actpreEnergy = vector<uint64_t>(NUM_RANKS,0);
	refreshEnergy = vector<uint64_t>(NUM_RANKS,0);
	idd2nCount = vector<unsigned>(NUM_RANKS,0);


	//init refresh counters
	for(unsigned i=0; i<NUM_RANKS; i++)
	{
		refreshCounters[i] = ((7800/tCK)/NUM_RANKS)*(i+1);
    }
}

//Updates the state of everything 
void SimpleController::Update()
{
	//
	//Stats
	//
	unsigned currentCount = 0;
	//count all the ACTIVATES waiting in the queue
	for(unsigned i=0; i<commandQueue.size(); i++)
	{
        currentCount += (commandQueue[i]->busPacketType==ACTIVATE);
	}
	if(currentCount>commandQueueMax) commandQueueMax = currentCount;

	//cumulative rolling average
	commandQueueAverage += currentCount;

	for(unsigned r=0; r<NUM_RANKS; r++)
	{
		for(unsigned b=0; b<NUM_BANKS; b++)
		{
            switch(bankStates[r][b].currentBankState)
			{
            //count the number of idle banks
            case IDLE:
              numIdleBanksAverage++;
              break;

            //count the number of active banks
            case ROW_ACTIVE:
              numActBanksAverage++;
              break;

            //count the number of precharging banks
            case PRECHARGING:
              numPreBanksAverage++;
              break;

            //count the number of refreshing banks
            case REFRESHING:
              numRefBanksAverage++;
              break;
			}
        }

        //
        //Power
        //
        bool bankOpen;
		for(unsigned b=0; b<NUM_RANKS; b++)
		{
          if( (bankOpen = bankStates[r][b].isBankOpen()) )
            break;
		}

		if(bankOpen)
		{
			//DRAM_BUS_WIDTH/2 because value accounts for DDR
			backgroundEnergy[r] += IDD3N * ((DRAM_BUS_WIDTH/2 * 8) / DEVICE_WIDTH);
		}
		else
		{
			//DRAM_BUS_WIDTH/2 because value accounts for DDR
			idd2nCount[r]++;
			backgroundEnergy[r] += IDD2N * ((DRAM_BUS_WIDTH/2 * 8) / DEVICE_WIDTH);
        }

        //
        //Update
        //
        //Updates the sliding window for tFAW
		for(unsigned i=0; i<tFAWWindow[r].size(); i++)
		{
			tFAWWindow[r][i]--;
			if(tFAWWindow[r][i]==0) tFAWWindow[r].erase(tFAWWindow[r].begin());
		}

        //Updates the bank states for each rank
		for(unsigned b=0; b<NUM_BANKS; b++)
		{
			bankStates[r][b].UpdateStateChange();
		}

        //Handle refresh counters
        if(refreshCounters[r]>0)
        {
            refreshCounters[r]--;
        }
    }

	//Send write data to data bus
	for(unsigned i=0; i<writeBurstCountdown.size(); i++)
	{
		writeBurstCountdown[i]--;
	}
	if(writeBurstCountdown.size()>0&&writeBurstCountdown[0]==0)
	{
        if(DEBUG_CHANNEL) DEBUG("     == Sending Write Data : ");
        channel->ReceiveOnDataBus(writeBurstQueue[0],0);
		writeBurstQueue.erase(writeBurstQueue.begin());
		writeBurstCountdown.erase(writeBurstCountdown.begin());
	}

	bool issuingRefresh = false;
	bool canIssueRefresh = true;

	//Figure out if everyone who needs a refresh can actually receive one
	for(unsigned r=0; r<NUM_RANKS; r++)
	{
		if(refreshCounters[r]==0)
		{
			if(DEBUG_CHANNEL) DEBUG("      !! -- Rank "<<r<<" needs refresh");
			//Check to be sure we can issue a refresh
			for(unsigned b=0; b<NUM_BANKS; b++)
			{
				if(bankStates[r][b].nextActivate > currentClockCycle ||
                   bankStates[r][b].currentBankState != IDLE)
				{
					canIssueRefresh = false;
                    break;
				}
			}

			//Once all counters have reached 0 and everyone is either idle or ready to accept refresh-CAS
			if(canIssueRefresh)
			{
				if(DEBUG_CHANNEL) DEBUGN("-- !! Refresh is issuable - Sending : ");

				//BusPacketType packtype, unsigned transactionID, unsigned col, unsigned rw, unsigned r, unsigned b, unsigned prt, unsigned bl
				BusPacket *refreshPacket = new BusPacket(REFRESH, -1, 0, 0, r, 0, 0, 0, 0, 0, false);
				refreshPacket->channel = channel->channelID;

				//Send to command bus
                channel->ReceiveOnCmdBus(refreshPacket,0);

				//make sure we don't send anythign else
				issuingRefresh = true;

				refreshEnergy[r] += (IDD5B-IDD3N) * tRFC * ((DRAM_BUS_WIDTH/2 * 8) / DEVICE_WIDTH);

				for(unsigned b=0; b<NUM_BANKS; b++)
				{
                    bankStates[r][b].refresh(currentClockCycle);
				}

				//reset refresh counters
				for(unsigned i=0; i<NUM_RANKS; i++)
				{
					if(refreshCounters[r]==0)
					{
						refreshCounters[r] = 7800/tCK;
					}
				}

				//only issue one
				break;
			}
		}
	}

	//If no refresh is being issued then do this block
	if(!issuingRefresh)
	{
		for(unsigned i=0; i<commandQueue.size(); i++)
		{
			//Checks to see if this particular request can be issued
			if(IsIssuable(commandQueue[i]))
            {
				//make sure we don't send a command ahead of its own ACTIVATE
				if(i>0 && commandQueue[i]->transactionID == commandQueue[i-1]->transactionID)
                    continue;

				//send to channel
                channel->ReceiveOnCmdBus(commandQueue[i],0);

				//update channel controllers bank state bookkeeping
				unsigned rank = commandQueue[i]->rank;
				unsigned bank = commandQueue[i]->bank;


				//
				//Main block for determining what to do with each type of command
				//
				switch(commandQueue[i]->busPacketType)
				{
                case READ_P:
					outstandingReads++;
					waitingACTS--;
					if(waitingACTS<0)
					{
						ERROR("#@)($J@)#(RJ");
						exit(0);
					}

					//keep track of energy
					burstEnergy[rank] += (IDD4R - IDD3N) * BL/2 * ((DRAM_BUS_WIDTH/2 * 8) / DEVICE_WIDTH);

					bankStates[rank][bank].lastCommand = READ_P;
					bankStates[rank][bank].stateChangeCountdown = (4*tCK>7.5)?tRTP:ceil(7.5/tCK); //4 clk or 7.5ns
					bankStates[rank][bank].nextActivate = max(bankStates[rank][bank].nextActivate, currentClockCycle + tRTP + tRP);
//					bankStates[rank][bank].nextRefresh = currentClockCycle + tRTP + tRP;

					for(unsigned r=0; r<NUM_RANKS; r++)
                    {
						if(r==rank)
						{
							for(unsigned b=0; b<NUM_BANKS; b++)
                            {
								bankStates[r][b].nextRead = max(bankStates[r][b].nextRead,
                                                                currentClockCycle + max((uint)tCCD, commandQueue[i]->burstLength)); // commandQueue[i]->burstLength == TRANSACTION_SIZE/DRAM_BUS_WIDTH
								bankStates[r][b].nextWrite = max(bankStates[r][b].nextWrite,
                                                                 currentClockCycle + (tCL + commandQueue[i]->burstLength + tRTRS - tCWL)); // commandQueue[i]->burstLength == TRANSACTION_SIZE/DRAM_BUS_WIDTH
							}
						}
						else
						{
							for(unsigned b=0; b<NUM_BANKS; b++)
							{
								bankStates[r][b].nextRead = max(bankStates[r][b].nextRead,
                                                                currentClockCycle + commandQueue[i]->burstLength  + tRTRS); // commandQueue[i]->burstLength == TRANSACTION_SIZE/DRAM_BUS_WIDTH
								bankStates[r][b].nextWrite = max(bankStates[r][b].nextWrite,
                                                                 currentClockCycle + (tCL + commandQueue[i]->burstLength + tRTRS - tCWL));// commandQueue[i]->burstLength == TRANSACTION_SIZE/DRAM_BUS_WIDTH
							}
						}
					}

					//prevents read or write being issued while waiting for auto-precharge to close page
					bankStates[rank][bank].nextRead = bankStates[rank][bank].nextActivate;
					bankStates[rank][bank].nextWrite = bankStates[rank][bank].nextActivate;

					break;
				case WRITE_P:
                {
					waitingACTS--;
					if(waitingACTS<0)
					{
						ERROR(")(JWE)(FJEWF");
						exit(0);
					}

					//keep track of energy
					burstEnergy[rank] += (IDD4W - IDD3N) * BL/2 * ((DRAM_BUS_WIDTH/2 * 8) / DEVICE_WIDTH);

                    BusPacket *writeData;
					writeData = new BusPacket(*commandQueue[i]);
					writeData->busPacketType = WRITE_DATA;
					writeBurstQueue.push_back(writeData);
					writeBurstCountdown.push_back(tCWL);
					if(DEBUG_CHANNEL) DEBUG("     !!! After Issuing WRITE_P, burstQueue is :"<<writeBurstQueue.size()<<" "<<writeBurstCountdown.size()<<" with head : "<<writeBurstCountdown[0]);

					bankStates[rank][bank].lastCommand = WRITE_P;
                    bankStates[rank][bank].stateChangeCountdown = tCWL + commandQueue[i]->burstLength + tWR; // commandQueue[i]->burstLength == TRANSACTION_SIZE/DRAM_BUS_WIDTH
                    bankStates[rank][bank].nextActivate = currentClockCycle + tCWL + commandQueue[i]->burstLength + tWR + tRP; // commandQueue[i]->burstLength == TRANSACTION_SIZE/DRAM_BUS_WIDTH
//					bankStates[rank][bank].nextRefresh = currentClockCycle + tCWL + commandQueue[i]->burstLength + tWR + tRP; // commandQueue[i]->burstLength == TRANSACTION_SIZE/DRAM_BUS_WIDTH

					for(unsigned r=0; r<NUM_RANKS; r++)
					{
						if(r==rank)
						{
							for(unsigned b=0; b<NUM_BANKS; b++)
							{
                                bankStates[r][b].nextRead = max(bankStates[r][b].nextRead,
                                                                currentClockCycle + tCWL + commandQueue[i]->burstLength + tWTR); // commandQueue[i]->burstLength == TRANSACTION_SIZE/DRAM_BUS_WIDTH
                                bankStates[r][b].nextWrite = max(bankStates[r][b].nextWrite,
                                                                 currentClockCycle+(uint64_t)max((uint)tCCD, commandQueue[i]->burstLength)); // commandQueue[i]->burstLength == TRANSACTION_SIZE/DRAM_BUS_WIDTH
							}
						}
						else
						{
							for(unsigned b=0; b<NUM_BANKS; b++)
							{
                                bankStates[r][b].nextRead = max(bankStates[r][b].nextRead,
                                                                currentClockCycle + tCWL + commandQueue[i]->burstLength + tRTRS - tCL); // commandQueue[i]->burstLength == TRANSACTION_SIZE/DRAM_BUS_WIDTH
                                bankStates[r][b].nextWrite = max(bankStates[r][b].nextWrite,
                                                                 currentClockCycle + commandQueue[i]->burstLength + tRTRS); // commandQueue[i]->burstLength == TRANSACTION_SIZE/DRAM_BUS_WIDTH
							}
						}
					}

					//prevents read or write being issued while waiting for auto-precharge to close page
					bankStates[rank][bank].nextRead = bankStates[rank][bank].nextActivate;
					bankStates[rank][bank].nextWrite = bankStates[rank][bank].nextActivate;

					break;
                }
                case ACTIVATE:
					for(unsigned b=0; b<NUM_BANKS; b++)
					{
						if(b!=bank)
						{
							bankStates[rank][b].nextActivate = max(currentClockCycle + tRRD, bankStates[rank][b].nextActivate);
						}
					}

					actpreEnergy[rank] += ((IDD0 * tRC) - ((IDD3N * tRAS) + (IDD2N * (tRC - tRAS)))) * ((DRAM_BUS_WIDTH/2 * 8) / DEVICE_WIDTH);

					bankStates[rank][bank].lastCommand = ACTIVATE;
					bankStates[rank][bank].currentBankState = ROW_ACTIVE;
					bankStates[rank][bank].openRowAddress = commandQueue[i]->row;
					bankStates[rank][bank].nextActivate = currentClockCycle + tRC;
					bankStates[rank][bank].nextRead = max(currentClockCycle + tRCD, bankStates[rank][bank].nextRead);
					bankStates[rank][bank].nextWrite = max(currentClockCycle + tRCD, bankStates[rank][bank].nextWrite);

					//keep track of sliding window
					tFAWWindow[rank].push_back(tFAW);

					break;
				default:
                    ERROR("Unexpected packet type");
					abort();
				}

				//erase
				commandQueue.erase(commandQueue.begin()+i);

				break;
			}
		}
    }

	//increment clock cycle
	currentClockCycle++;
}


bool SimpleController::IsIssuable(BusPacket *busPacket)
{
	unsigned rank = busPacket->rank;
	unsigned bank = busPacket->bank;


	//if((channel->readReturnQueue.size()+outstandingReads) * TRANSACTION_SIZE >= CHANNEL_RETURN_Q_MAX)
	//if((channel->readReturnQueue.size()) * TRANSACTION_SIZE >= CHANNEL_RETURN_Q_MAX)
	//	{
	//RRQFull++;
	//DEBUG("!!!!!!!!!"<<*busPacket)
	//exit(0);
	//return false;
	//}


	switch(busPacket->busPacketType)
	{
    case READ_P:
		if(bankStates[rank][bank].currentBankState == ROW_ACTIVE &&
		        bankStates[rank][bank].openRowAddress == busPacket->row &&
		        currentClockCycle >= bankStates[rank][bank].nextRead &&
                (channel->readReturnQueue.size()+outstandingReads) * (busPacket->burstLength * DRAM_BUS_WIDTH) < CHANNEL_RETURN_Q_MAX) // busPacket->burstLength * DRAM_BUS_WIDTH == TRANSACTION_SIZE
        {
			return true;
		}
		else
		{
            if((channel->readReturnQueue.size()+outstandingReads) * (busPacket->burstLength * DRAM_BUS_WIDTH) >= CHANNEL_RETURN_Q_MAX) // busPacket->burstLength * DRAM_BUS_WIDTH == TRANSACTION_SIZE
			{
				RRQFull++;
			}

			return false;
		}

        break;
    case WRITE_P:
		if(bankStates[rank][bank].currentBankState == ROW_ACTIVE &&
		        bankStates[rank][bank].openRowAddress == busPacket->row &&
		        currentClockCycle >= bankStates[rank][bank].nextWrite &&
                (channel->readReturnQueue.size()+outstandingReads) * (busPacket->burstLength * DRAM_BUS_WIDTH) < CHANNEL_RETURN_Q_MAX) // busPacket->burstLength * DRAM_BUS_WIDTH == TRANSACTION_SIZE
        {
			return true;
		}
		else
		{
            if((channel->readReturnQueue.size()+outstandingReads) * (busPacket->burstLength * DRAM_BUS_WIDTH) >= CHANNEL_RETURN_Q_MAX) // busPacket->burstLength * DRAM_BUS_WIDTH == TRANSACTION_SIZE
			{
				RRQFull++;
			}
			return false;
		}
		break;
    case ACTIVATE:
		if(bankStates[rank][bank].currentBankState == IDLE &&
		        currentClockCycle >= bankStates[rank][bank].nextActivate &&
		        refreshCounters[rank]>0 &&
		        tFAWWindow[rank].size()<4)
        {
			return true;
		}
		else return false;
		break;
	default:
        ERROR("== Error - Checking issuability on unknown packet type");
		exit(0);
	}
}

void SimpleController::AddTransaction(Transaction *trans)
{
	//map physical address to rank/bank/row/col
	AddressMapping(trans->address,mappedRank,mappedBank,mappedRow,mappedCol);

	if(GIVE_LOGIC_PRIORITY && trans->originatedFromLogicOp)
	{
		//if requests from logic ops have priority, put them at the front so they go first
        if(DEBUG_LOGIC) DEBUG("  == Simple Controller received transaction from logic op");
		switch(trans->transactionType)
		{
		case DATA_READ:
			readCounter++;
			//create column read bus packet and add it to command queue
			commandQueue.push_front(new BusPacket(READ_P,trans->transactionID,mappedCol,mappedRow,mappedRank,mappedBank,trans->portID,trans->transactionSize/DRAM_BUS_WIDTH,trans->mappedChannel,trans->address,trans->originatedFromLogicOp));
			break;
		case DATA_WRITE:
			writeCounter++;
			//create column write bus packet and add it to command queue
			commandQueue.push_front(new BusPacket(WRITE_P,trans->transactionID,mappedCol,mappedRow,mappedRank,mappedBank,trans->portID,trans->transactionSize/DRAM_BUS_WIDTH,trans->mappedChannel,trans->address,trans->originatedFromLogicOp));
			delete trans;
			break;
		default:
            ERROR("== ERROR - Adding wrong transaction to simple controller");
			abort();
			break;
		}
		//since we're pushing front, add the ACT after so it ends up being first
		commandQueue.push_front(new BusPacket(ACTIVATE, trans->transactionID,mappedCol,mappedRow,mappedRank,mappedBank,trans->portID,0,trans->mappedChannel,trans->address,trans->originatedFromLogicOp));
	}
	else
	{
		//create the row activate bus packet and add it to command queue
		commandQueue.push_back(new BusPacket(ACTIVATE, trans->transactionID,mappedCol,mappedRow,mappedRank,mappedBank,trans->portID,0,trans->mappedChannel,trans->address,trans->originatedFromLogicOp));

		switch(trans->transactionType)
		{
		case DATA_READ:
			readCounter++;
			//create column read bus packet and add it to command queue
			commandQueue.push_back(new BusPacket(READ_P,trans->transactionID,mappedCol,mappedRow,mappedRank,mappedBank,trans->portID,trans->transactionSize/DRAM_BUS_WIDTH,trans->mappedChannel,trans->address,trans->originatedFromLogicOp));
			break;
		case DATA_WRITE:
			writeCounter++;
			//create column write bus packet and add it to command queue
			commandQueue.push_back(new BusPacket(WRITE_P,trans->transactionID,mappedCol,mappedRow,mappedRank,mappedBank,trans->portID,trans->transactionSize/DRAM_BUS_WIDTH,trans->mappedChannel,trans->address,trans->originatedFromLogicOp));
			delete trans;
			break;
		default:
            ERROR("== ERROR - Adding wrong transaction to simple controller");
			abort();
			break;
		}
	}

	waitingACTS++;
}

void SimpleController::AddressMapping(uint64_t physicalAddress, unsigned &rank, unsigned &bank, unsigned &row, unsigned &col)
{
	uint64_t tempA, tempB;

	if(DEBUG_CHANNEL)DEBUGN("     == Mapped 0x"<<hex<<physicalAddress);

    switch(MAPPINGSCHEME)
	{
	case BK_CLH_RW_RK_CH_CLL_BY://bank:col_high:row:rank:chan:col_low:by
		//remove low order bits
		//this includes:
		// - byte offset
		// - low order bits of column address (assumed to be 0 since it is cache aligned)
		// - channel bits (already used)
		//
		//log2(CACHE_LINE_SIZE) == (log2(Low order column bits) + log2(BUS_ALIGNMENT_SIZE))
		physicalAddress = physicalAddress >> (channelBitWidth + cacheOffset);

		//rank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		mappedRank = tempA ^ tempB;

		//row bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		mappedRow = tempA ^ tempB;

		//column bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> (colBitWidth - (cacheOffset-busOffsetBitWidth));
		tempB = physicalAddress << (colBitWidth - (cacheOffset-busOffsetBitWidth));
		mappedCol = tempA ^ tempB;

		//account for low order column bits
		mappedCol = mappedCol << (cacheOffset-busOffsetBitWidth);

		//bank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		mappedBank = tempA ^ tempB;

		break;
	case CLH_RW_RK_BK_CH_CLL_BY://col_high:row:rank:bank:chan:col_low:by
		//remove low order bits
		//this includes:
		// - byte offset
		// - low order bits of column address (assumed to be 0 since it is cache aligned)
		// - channel bits (already used)
		//
		//log2(CACHE_LINE_SIZE) == (log2(Low order column bits) + log2(BUS_ALIGNMENT_SIZE))
		physicalAddress = physicalAddress >> (channelBitWidth + cacheOffset);

		//bank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		mappedBank = tempA ^ tempB;

		//rank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		mappedRank = tempA ^ tempB;

		//row bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		mappedRow = tempA ^ tempB;

		//column bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> (colBitWidth - (cacheOffset-busOffsetBitWidth));
		tempB = physicalAddress << (colBitWidth - (cacheOffset-busOffsetBitWidth));
		mappedCol = tempA ^ tempB;

		//account for low order column bits
		mappedCol = mappedCol << (cacheOffset-busOffsetBitWidth);

		break;
	case RK_BK_RW_CLH_CH_CLL_BY://rank:bank:row:colhigh:chan:collow:by
		//remove low order bits
		// - byte offset
		// - low order bits of column address (assumed to be 0 since it is cache aligned)
		// - channel bits (already used)
		//
		//log2(CACHE_LINE_SIZE) == (log2(Low order column bits) + log2(BUS_ALIGNMENT_SIZE))
		physicalAddress = physicalAddress >> (channelBitWidth + cacheOffset);

		//column bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> (colBitWidth - (cacheOffset-busOffsetBitWidth));
		tempB = physicalAddress << (colBitWidth - (cacheOffset-busOffsetBitWidth));
		mappedCol = tempA ^ tempB;

		//account for low order column bits
		mappedCol = mappedCol << (cacheOffset-busOffsetBitWidth);

		//row bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		mappedRow = tempA ^ tempB;

		//bank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		mappedBank = tempA ^ tempB;

		//rank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		mappedRank = tempA ^ tempB;

		break;
	case RW_CH_BK_RK_CL_BY://row:chan:bank:rank:col:by
		//remove low order bits which account for the amount of data received on the bus (8 bytes)
		physicalAddress = physicalAddress >> busOffsetBitWidth;

		//column bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> colBitWidth;
		tempB = physicalAddress << colBitWidth;
		mappedCol = tempA ^ tempB;

		//rank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		mappedRank = tempA ^ tempB;

		//bank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		mappedBank = tempA ^ tempB;

		//channel has already been mapped so just shift off the bits
		physicalAddress = physicalAddress >> channelBitWidth;

		//row bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		mappedRow = tempA ^ tempB;

		break;
	case RW_BK_RK_CH_CL_BY://row:bank:rank:chan:col:byte
		//remove low order bits which account for the amount of data received on the bus (8 bytes)
		physicalAddress = physicalAddress >> busOffsetBitWidth;

		//column bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> colBitWidth;
		tempB = physicalAddress << colBitWidth;
		mappedCol = tempA ^ tempB;

		//channel has already been mapped so just shift off the bits
		physicalAddress = physicalAddress >> channelBitWidth;

		//rank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		mappedRank = tempA ^ tempB;

		//bank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		mappedBank = tempA ^ tempB;

		//row bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		mappedRow = tempA ^ tempB;

		break;
	case RW_BK_RK_CLH_CH_CLL_BY://row:bank:rank:col_high:chan:col_low:byte
		//remove low order bits
		//this includes:
		// - byte offset
		// - low order bits of column address (assumed to be 0 since it is cache aligned)
		// - channel bits (already used)
		//
		//log2(CACHE_LINE_SIZE) == (log2(Low order column bits) + log2(BUS_ALIGNMENT_SIZE))
		physicalAddress = physicalAddress >> (channelBitWidth + cacheOffset);

		//column bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> (colBitWidth - (cacheOffset-busOffsetBitWidth));
		tempB = physicalAddress << (colBitWidth - (cacheOffset-busOffsetBitWidth));
		mappedCol = tempA ^ tempB;

		//account for low order column bits
		mappedCol = mappedCol << (cacheOffset-busOffsetBitWidth);

		//rank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		mappedRank = tempA ^ tempB;

		//bank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		mappedBank = tempA ^ tempB;

		//row bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		mappedRow = tempA ^ tempB;

		break;
	case RW_CLH_BK_RK_CH_CLL_BY://row:col_high:bank:rank:chan:col_low:byte
		//remove low order bits
		//this includes:
		// - byte offset
		// - low order bits of column address (assumed to be 0 since it is cache aligned)
		// - channel bits (already used)
		//
		//log2(CACHE_LINE_SIZE) == (log2(Low order column bits) + log2(BUS_ALIGNMENT_SIZE))
		physicalAddress = physicalAddress >> (channelBitWidth + cacheOffset);

		//rank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		mappedRank = tempA ^ tempB;

		//bank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		mappedBank = tempA ^ tempB;

		//column bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> (colBitWidth - (cacheOffset-busOffsetBitWidth));
		tempB = physicalAddress << (colBitWidth - (cacheOffset-busOffsetBitWidth));
		mappedCol = tempA ^ tempB;

		//account for low order column bits
		mappedCol = mappedCol << (cacheOffset-busOffsetBitWidth);

		//row bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		mappedRow = tempA ^ tempB;

		break;
	case CH_RW_BK_RK_CL_BY:
		//remove bits which would address the amount of data received on a request
		physicalAddress = physicalAddress >> busOffsetBitWidth;

		//column bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> colBitWidth;
		tempB = physicalAddress << colBitWidth;
		mappedCol = tempA ^ tempB;

		//rank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rankBitWidth;
		tempB = physicalAddress << rankBitWidth;
		mappedRank = tempA ^ tempB;

		//bank bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> bankBitWidth;
		tempB = physicalAddress << bankBitWidth;
		mappedBank = tempA ^ tempB;

		//row bits
		tempA = physicalAddress;
		physicalAddress = physicalAddress >> rowBitWidth;
		tempB = physicalAddress << rowBitWidth;
		mappedRow = tempA ^ tempB;

		break;
	default:
		ERROR("== ERROR - Unknown address mapping???");
		exit(1);
		break;
	};

	if(DEBUG_CHANNEL) DEBUG(" to RK:"<<hex<<mappedRank<<" BK:"<<mappedBank<<" RW:"<<mappedRow<<" CL:"<<mappedCol<<dec);
}
