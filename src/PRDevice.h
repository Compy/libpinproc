/*
 * The MIT License
 * Copyright (c) 2009 Gerry Stellenberg, Adam Preble
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 *  PRDevice.h
 *  libpinproc
 */

#include "../include/pinproc.h"
#include "PRCommon.h"
#include "PRHardware.h"
#include <queue>

using namespace std;

#define maxDriverGroups (26)
#define maxDrivers (256)
#define maxSwitchRules (256<<2) // 8 bits of switchNum indicies plus bits for debounced and state.
#define maxWriteWords (1536) // Hardware supports 2048 word bursts, but restrict to 1536 for margin.

class PRDevice
{
public:
    static PRDevice *Create(PRMachineType machineType);
    ~PRDevice();
    PRResult Reset(uint32_t resetFlags);
protected:
    PRDevice(PRMachineType machineType);

public:
    // public libpinproc API:
    int GetEvents(PREvent *events, int maxEvents);

    PRResult DriverUpdateGlobalConfig(PRDriverGlobalConfig *driverGlobalConfig);
    PRResult DriverGetGroupConfig(uint8_t groupNum, PRDriverGroupConfig *driverGroupConfig);
    PRResult DriverUpdateGroupConfig(PRDriverGroupConfig *driverGroupConfig);
    PRResult DriverGetState(uint8_t driverNum, PRDriverState *driverState);
    PRResult DriverUpdateState(PRDriverState *driverState);

    PRResult SwitchUpdateConfig(PRSwitchConfig *switchConfig);
    PRResult SwitchUpdateRule(uint8_t switchNum, PREventType eventType, PRSwitchRule *rule, PRDriverState *linkedDrivers, int numDrivers);

    PRResult DriverWatchdogTickle();

    PRResult DMDUpdateConfig(PRDMDConfig *dmdConfig);
    PRResult DMDDraw(uint8_t * dots);

protected:

    // Device I/O

    PRResult Open();
    PRResult Close();

    PRResult VerifyChipID();

    // Raw write and read methods
    //
    
    /** Schedules data to be written to the P-ROC.  */
    PRResult PrepareWriteData(uint32_t * buffer, int32_t numWords);

    /** Initiates a burst write of all data scheduled to be written to the P-ROC.  */
    PRResult FlushWriteData();

    /** Writes data to P-ROC.
     * Returns #kPFailure if the number of words read does not match the number requested.
     */
    PRResult WriteData(uint32_t * buffer, int32_t numWords);

    /**
     * Reads data from the buffer that was previously collected by CollectReadData().
     * Returns the number of bytes read.
     */
    int32_t ReadData(uint32_t *buffer, int32_t maxWords);

    // Collection of methods to get data returning from the P-ROC
    /**
     * Request a block of data from the P-ROC.
     */
    PRResult RequestData(uint32_t module_select, uint32_t start_addr, int32_t num_words);
    /**
     * Actually reads the data off of the FTDI chip.
     * This is called by SortReturningData() in order to get some data to process.
     */
    int32_t CollectReadData();
    /**
     * Processes data into unrequestedDataQueue and requestedDataQueue.
     * Calls CollectReadData() to obtain the data and then uses ReadData() to read it out.
     */
    PRResult SortReturningData();
    /**
     * Empties out the read buffer.
     * Calls CollectReadData() and then ReadData() until it's empty.
     */
    PRResult FlushReadBuffer();

    queue<uint32_t> unrequestedDataQueue; /**< Queue of words received from the device that were not requested via RequestData().  Usually switch events. */
    queue<uint32_t> requestedDataQueue; /**< Queue of words received from the device as the result of a call to RequestData(). */

    uint32_t preparedWriteWords[maxWriteWords];
    int32_t numPreparedWriteWords;

    uint8_t collected_bytes_fifo[FTDI_BUFFER_SIZE];
    int32_t collected_bytes_rd_addr;
    int32_t collected_bytes_wr_addr;
    int32_t num_collected_bytes;

    uint8_t wr_buffer[16384];
    uint8_t collect_buffer[FTDI_BUFFER_SIZE];


    // Local Device State
    PRMachineType machineType;
    PRDriverGlobalConfig driverGlobalConfig;
    PRDriverGroupConfig driverGroups[maxDriverGroups];
    PRDriverState drivers[maxDrivers];
    PRDMDConfig dmdConfig;
	
    PRSwitchConfig switchConfig;
    PRSwitchRuleInternal switchRules[maxSwitchRules];
	queue<uint32_t> freeSwitchRuleIndexes; /**< Indexes of available switch rules. */
    PRSwitchRuleInternal *GetSwitchRuleByIndex(uint16_t index);
};
