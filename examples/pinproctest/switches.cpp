/*
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
#include "pinproctest.h"

extern PRMachineType machineType;

typedef struct SwitchStatus {
    PREventType state;
    uint32_t lastEventTime;
} SwitchStatus;

static SwitchStatus switches[kPRSwitchPhysicalLast + 1];

void ConfigureSwitches(PRHandle proc, YAML::Node& yamlDoc)
{
    // Configure switch controller registers (if the defaults aren't acceptable)
    PRSwitchConfig switchConfig;
    switchConfig.clear = false;
    switchConfig.use_column_8 = machineType == kPRMachineWPC;
    switchConfig.use_column_9 = false; // No WPC machines actually use this.
    switchConfig.hostEventsEnable = true;
    switchConfig.directMatrixScanLoopTime = 2; // milliseconds
    switchConfig.pulsesBeforeCheckingRX = 10;
    switchConfig.inactivePulsesAfterBurst = 12;
    switchConfig.pulsesPerBurst = 6;
    switchConfig.pulseHalfPeriodTime = 13; // milliseconds
    PRSwitchUpdateConfig(proc, &switchConfig);

    // Go through the switches array and reset the current status of each switch
    for (int i = 0; i <= kPRSwitchPhysicalLast; i++)
    {
        switches[i].state = kPREventTypeInvalid;
        switches[i].lastEventTime = 0;

        PRSwitchRule sw;
        sw.notifyHost = true;
        sw.reloadActive = false;
        PRSwitchUpdateRule(proc, i, kPREventTypeSwitchClosedDebounced, &sw, NULL, 0, false);
        PRSwitchUpdateRule(proc, i, kPREventTypeSwitchOpenDebounced, &sw, NULL, 0, false);
    }
}

void ConfigureWPCFlipperSwitchRule (PRHandle proc, int swNum, int mainCoilNum, int holdCoilNum, int pulseTime)
{
    const int numDriverRules = 2;
    PRDriverState drivers[numDriverRules];
    PRSwitchRule sw;
    
    // Flipper on rules
    PRDriverGetState(proc, mainCoilNum, &drivers[0]);
    PRDriverStatePulse(&drivers[0],pulseTime); // Pulse coil for 34ms.
    PRDriverGetState(proc, holdCoilNum, &drivers[1]);
    PRDriverStatePulse(&drivers[1],0);  // Turn on indefintely (set pulse for 0ms)
    sw.notifyHost = false;
    sw.reloadActive = false;
    PRSwitchUpdateRule(proc, swNum, kPREventTypeSwitchClosedNondebounced, &sw, drivers, numDriverRules, true);
    sw.notifyHost = true;
    sw.reloadActive = false;
    PRSwitchUpdateRule(proc, swNum, kPREventTypeSwitchClosedDebounced, &sw, NULL, 0, false);
    
    // Flipper off rules
    PRDriverGetState(proc, mainCoilNum, &drivers[0]);
    PRDriverStateDisable(&drivers[0]); // Disable main coil
    PRDriverGetState(proc, holdCoilNum, &drivers[1]);
    PRDriverStateDisable(&drivers[1]); // Disable hold coil
    sw.notifyHost = false;
    sw.reloadActive = false;
    PRSwitchUpdateRule(proc, swNum, kPREventTypeSwitchOpenNondebounced, &sw, drivers, numDriverRules, true);
    sw.notifyHost = true;
    sw.reloadActive = false;
    PRSwitchUpdateRule(proc, swNum, kPREventTypeSwitchOpenDebounced, &sw, NULL, 0, false);
}

void ConfigureSternFlipperSwitchRule (PRHandle proc, int swNum, int mainCoilNum, int pulseTime, int patterOnTime, int patterOffTime)
{
    printf("swNum: %d', coilnum: %d, pulseTime: %d, pon: %d, poff: %d\n", swNum,mainCoilNum,pulseTime,patterOnTime,patterOffTime);
    const int numDriverRules = 1;
    PRDriverState drivers[numDriverRules];
    PRSwitchRule sw;
    
    // Flipper on rules
    PRDriverGetState(proc, mainCoilNum, &drivers[0]);
    PRDriverStatePatter(&drivers[0],patterOnTime,patterOffTime,pulseTime,true); // Pulse coil for 34ms.
    sw.notifyHost = false;
    sw.reloadActive = false;
    PRSwitchUpdateRule(proc, swNum, kPREventTypeSwitchClosedNondebounced, &sw, drivers, numDriverRules, true);
    sw.notifyHost = true;
    sw.reloadActive = false;
    PRSwitchUpdateRule(proc, swNum, kPREventTypeSwitchClosedDebounced, &sw, NULL, 0, false);
    
    // Flipper off rules
    PRDriverGetState(proc, mainCoilNum, &drivers[0]);
    PRDriverStateDisable(&drivers[0]); // Disable main coil
    sw.notifyHost = false;
    sw.reloadActive = false;
    PRSwitchUpdateRule(proc, swNum, kPREventTypeSwitchOpenNondebounced, &sw, drivers, numDriverRules, true);
    sw.notifyHost = true;
    sw.reloadActive = false;
    PRSwitchUpdateRule(proc, swNum, kPREventTypeSwitchOpenDebounced, &sw, NULL, 0, false);
}

void ConfigureBumperRule (PRHandle proc, int swNum, int coilNum, int pulseTime)
{
    const int numDriverRules = 1;
    PRDriverState drivers[numDriverRules];
    PRSwitchRule sw;
    
    PRDriverGetState(proc, coilNum, &drivers[0]);
    PRDriverStatePulse(&drivers[0],pulseTime); // Pulse coil for 34ms.
    sw.reloadActive = true;
    sw.notifyHost = false;
    PRSwitchUpdateRule(proc, swNum, kPREventTypeSwitchClosedNondebounced, &sw, drivers, numDriverRules, true);
    sw.notifyHost = true;
    sw.reloadActive = false;
    PRSwitchUpdateRule(proc, swNum, kPREventTypeSwitchClosedDebounced, &sw, NULL, 0, false);
}

void ConfigureSwitchRules(PRHandle proc, YAML::Node& yamlDoc)
{
    // WPC  Flippers
    std::string numStr;
    const YAML::Node& flippers = yamlDoc[kFlippersSection];
    for (YAML::Iterator flippersIt = flippers.begin(); flippersIt != flippers.end(); ++flippersIt)
    {
        int swNum, coilMain, coilHold;
        std::string flipperName;
        *flippersIt >> flipperName;
        if (machineType == kPRMachineWPC)
        {
            yamlDoc[kSwitchesSection][flipperName][kNumberField] >> numStr; swNum = PRDecode(machineType, numStr.c_str());
            yamlDoc[kCoilsSection][flipperName + "Main"][kNumberField] >> numStr; coilMain = PRDecode(machineType, numStr.c_str());
            yamlDoc[kCoilsSection][flipperName + "Hold"][kNumberField] >> numStr; coilHold = PRDecode(machineType, numStr.c_str());
            ConfigureWPCFlipperSwitchRule (proc, swNum, coilMain, coilHold, kFlipperPulseTime);
        }
        else if (machineType == kPRMachineSternWhitestar || machineType == kPRMachineSternSAM)
        {
            printf("hi\n");
            yamlDoc[kSwitchesSection][flipperName][kNumberField] >> numStr; swNum = PRDecode(machineType, numStr.c_str());
            yamlDoc[kCoilsSection][flipperName + "Main"][kNumberField] >> numStr; coilMain = PRDecode(machineType, numStr.c_str());
            ConfigureSternFlipperSwitchRule (proc, swNum, coilMain, kFlipperPulseTime, kFlipperPatterOnTime, kFlipperPatterOffTime);
        }
    }
    
    const YAML::Node& bumpers = yamlDoc[kBumpersSection];
    for (YAML::Iterator bumpersIt = bumpers.begin(); bumpersIt != bumpers.end(); ++bumpersIt)
    {
        int swNum, coilNum;
        // WPC  Slingshots
        std::string bumperName;
        *bumpersIt >> bumperName;
        yamlDoc[kSwitchesSection][bumperName][kNumberField] >> numStr; swNum = PRDecode(machineType, numStr.c_str());
        yamlDoc[kCoilsSection][bumperName][kNumberField] >> numStr; coilNum = PRDecode(machineType, numStr.c_str());
        ConfigureBumperRule (proc, swNum, coilNum, kBumperPulseTime);
    }
}

void UpdateSwitchState( PREvent * event )
{
    switches[event->value].state = event->type;
    switches[event->value].lastEventTime = event->time;
}

void LoadSwitchStates( PRHandle proc )
{
    int i;
    PREventType procSwitchStates[kPRSwitchPhysicalLast + 1];

    // Get all of the switch states from the P-ROC.
    if (PRSwitchGetStates( proc, procSwitchStates, kPRSwitchPhysicalLast + 1 ) == kPRFailure)
    {
        fprintf(stderr, "Error: Unable to retrieve switch states\n");
    }
    else
    {
        // Copy the returning states into the local switches array.
        for (i = 0; i <= kPRSwitchPhysicalLast; i++)
        {
            switches[i].state = procSwitchStates[i];
        }

        int zero = 0;
        fprintf(stderr, "\nCurrent Switch States: %3d : ", zero);
        for (i = 0; i < kPRSwitchPhysicalLast + 1; i++)
        {
            fprintf(stderr, "%d ", switches[i].state);
            if ((i + 1) % 32 == 0) 
            {
                printf("\n");
                if (i != kPRSwitchPhysicalLast) 
                    fprintf(stderr, "Current Switch States: %3d : ", i+1);
            }
        }
        fprintf(stderr, "\n");
    }
}
