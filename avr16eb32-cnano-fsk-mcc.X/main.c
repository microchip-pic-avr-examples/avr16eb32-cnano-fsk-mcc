 /*
 * MAIN Generated Driver File
 * 
 * @file main.c
 * 
 * @defgroup main MAIN
 * 
 * @brief This is the generated driver implementation file for the MAIN driver.
 *
 * @version MAIN Driver Version 1.0.0
*/

/*
© [2024] Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip 
    software and any derivatives exclusively with Microchip products. 
    You are responsible for complying with 3rd party license terms  
    applicable to your use of 3rd party software (including open source  
    software) that may accompany Microchip software. SOFTWARE IS ?AS IS.? 
    NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS 
    SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,  
    MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT 
    WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY 
    KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF 
    MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE 
    FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP?S 
    TOTAL LIABILITY ON ALL CLAIMS RELATED TO THE SOFTWARE WILL NOT 
    EXCEED AMOUNT OF FEES, IF ANY, YOU PAID DIRECTLY TO MICROCHIP FOR 
    THIS SOFTWARE.
*/
#include "mcc_generated_files/system/system.h"
#include <util/delay.h>
#include <stdbool.h>

/*
    Main application
*/

#define HZ_TO_CLOCKS(frequency)     (uint16_t)(F_CPU / (frequency))
#define US_TO_CLOCKS(period)        (uint16_t)((period) * (F_CPU / 1000000))
#define FSK_FHIGH                   4000            /* "mark" frequency (Hz) */
#define FSK_FLOW                    2600            /* "space" frequency (Hz) */
#define SAMPLING_PERIOD             1000            /* us */
#define IDLE_PERIOD                 25              /* us */
#define OFFSET                      440             /* us */

/* function used to modulate the data */
void TCE0_Sampling_Handler(void);

/* function used to distinguish between the two FSK frequencies */
void TCB0_PeriodMeasurement_Handler(void);

/* function used to demodulate the data */
void TCB1_Sampling_Handler(void);

/* function used to start the modulation process, receives as parameter the data to be modulated */
void FSK_Transmit(uint8_t data);

/* function used to return the demodulated data */
uint8_t FSK_Receive(void);

volatile uint8_t readByteFromUSART;
volatile bool isFskTxReady = true;
volatile bool isFskRxReady = false;
volatile uint8_t bufferedDemodulatedByte;
volatile bool currentLogicState;

void TCE0_Sampling_Handler(void)
{
    IO_PA6_Toggle();    /* used for debug/demonstration, removable/reassignable if needed */
    static uint8_t modulationBitCounter;
    if(!TCF0_StatusGet())   /* modulation process starts */
    {
        modulationBitCounter = 10;
        TCF0_CompareSet(TCF0_NCOFD_HZ_TO_INCREMENT(FSK_FHIGH, F_CPU, 1));   /* "mark" is sent to emulate the idle state/previous stop bit */
        TCF0_Start();
    }
    else if(modulationBitCounter == 10)
    {
        TCF0_CompareSet(TCF0_NCOFD_HZ_TO_INCREMENT(FSK_FLOW, F_CPU, 1));    /* "space" is sent to emulate the start bit */
        modulationBitCounter--;
        IO_PA2_SetLow();    /* used for debug/demonstration, removable/reassignable if needed */
    }
    else if(modulationBitCounter == 1)
    {
        TCF0_CompareSet(TCF0_NCOFD_HZ_TO_INCREMENT(FSK_FHIGH, F_CPU, 1));   /* "mark" is sent to emulate the stop bit */
        modulationBitCounter--;
        IO_PA2_SetHigh();   /* used for debug/demonstration, removable/reassignable if needed */
    }
    else if(modulationBitCounter == 0)  /* modulation process ends */
    {
        TCE0_Stop();
        TCF0_Stop();
        isFskTxReady = true;    /* new data (if available) is ready to be modulated */
    }
    else    /* data is modulated by sending either "mark" or "space" according to the value of each data bit */
    {
        if(readByteFromUSART & 0x01)
        {
            TCF0_CompareSet(TCF0_NCOFD_HZ_TO_INCREMENT(FSK_FHIGH, F_CPU, 1));   /* "mark" is sent for 1 */
            readByteFromUSART = readByteFromUSART >> 1;
            modulationBitCounter--;
            IO_PA2_SetHigh();   /* used for debug/demonstration, removable/reassignable if needed */
        }
        else
        {
            TCF0_CompareSet(TCF0_NCOFD_HZ_TO_INCREMENT(FSK_FLOW, F_CPU, 1));    /* "space" is sent for 0 */
            readByteFromUSART = readByteFromUSART >> 1;
            modulationBitCounter--;
            IO_PA2_SetLow();   /* used for debug/demonstration, removable/reassignable if needed */
        }
    }
}

void TCB0_PeriodMeasurement_Handler(void)
{   
    IO_PA3_SetHigh();   /* used for debug/demonstration, removable/reassignable if needed */
    static uint16_t currentPeriod;
    currentPeriod = TCB0_CaptureCompareRead();
    if(!TCB0_IsOvfInterruptFlagSet())   /* checks if the captured value is valid and whether it corresponds to the "mark" or "space" frequency */
    {
        if((currentPeriod > HZ_TO_CLOCKS((FSK_FHIGH + FSK_FLOW) / 2)) && (currentPeriod < HZ_TO_CLOCKS(FSK_FLOW / 2)))
        {
            IO_PA5_SetLow();    /* used for debug/demonstration, removable/reassignable if needed */
            currentLogicState = false;
        }   
        else if((currentPeriod > HZ_TO_CLOCKS(FSK_FHIGH * 2)) && (currentPeriod < HZ_TO_CLOCKS((FSK_FHIGH + FSK_FLOW) / 2)))
        {
            IO_PA5_SetHigh();   /* used for debug/demonstration, removable/reassignable if needed */
            currentLogicState = true;
        }
    }
    else
    {
        TCB0_ClearOvfInterruptFlag();
    }
    IO_PA3_SetLow();    /* used for debug/demonstration, removable/reassignable if needed */
}

void TCB1_Sampling_Handler(void)
{      
    IO_PA4_SetHigh();   /* used for debug/demonstration, removable/reassignable if needed */
    static bool previousLogicState = false;
    static bool samplingReadyToStart = false;
    static uint8_t demodulationBitCounter;
    static uint8_t demodulatedByte;
    
    if(samplingReadyToStart)    /* demodulation process starts */
    {
        if(demodulationBitCounter == 10)    /* start bit is discarded */
        {
            demodulationBitCounter--;
        }
        else if(demodulationBitCounter == 1)    /* demodulation process ends */
        {
            TCB1_Write(0);
            TCB1_CaptureCompareWrite(US_TO_CLOCKS(IDLE_PERIOD));  /* sampling rate is set to check for the next start bit */
            samplingReadyToStart = false;
            if(currentLogicState == true)   /* checks if the stop bit is valid */
            {
                bufferedDemodulatedByte = demodulatedByte;
                isFskRxReady = true;    /* data was successfully demodulated */
            }
            else
            {
                bufferedDemodulatedByte = 0;
            }
        }
        else    /* data is demodulated/reconstructed bit by bit*/
        {
            demodulatedByte = (demodulatedByte >> 1) | (currentLogicState << 7);
            demodulationBitCounter--;
        }
    }
    if((currentLogicState == false) && (previousLogicState == true) && (samplingReadyToStart == false)) /* checks if the start bit was detected */
    {
        TCB1_Write(US_TO_CLOCKS(OFFSET));   /* offset is added to accurately sample the data */
        TCB1_CaptureCompareWrite(US_TO_CLOCKS(SAMPLING_PERIOD));  /* sampling rate is set to match the one used for modulation */
        samplingReadyToStart = true;
        demodulationBitCounter = 10;
        demodulatedByte = 0;
    }
    previousLogicState = currentLogicState;
    IO_PA4_SetLow();    /* used for debug/demonstration, removable/reassignable if needed */
}

void FSK_Transmit(uint8_t data)
{
    readByteFromUSART = data;
    TCE0_Start();
}

uint8_t FSK_Receive(void)
{
    return bufferedDemodulatedByte;
}

int main(void)
{
    SYSTEM_Initialize();
    
    TCE0_OverflowCallbackRegister(TCE0_Sampling_Handler);
    TCB0_CaptureCallbackRegister(TCB0_PeriodMeasurement_Handler);
    TCB1_CaptureCallbackRegister(TCB1_Sampling_Handler);

    _delay_ms(3000);
    ENABLE_INTERRUPTS();
    
    while(1)
    {
        if(USART0_IsRxReady() && isFskTxReady)
        {
            FSK_Transmit(USART0_Read());
            isFskTxReady = false;
        }
        if(USART0_IsTxReady() && isFskRxReady)
        {
            USART0_Write(FSK_Receive());
            isFskRxReady = false;
        }
    }    
}