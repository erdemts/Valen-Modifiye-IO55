/*******************************************************************************
 *
 * FileName     : BusIdDongle main.c
 * Dependencies :
 * Description  :
 * Processor    : PIC16F887
 * Compiler     : Hi-tech Picc 9.83
 * Linker       :
 * Company      : Inno Technology Incorporated
 *
 * Software License Agreement
 * Copyright (C) 2011 - 2012 Inno Technology Inc.  All rights
 * reserved.
 *
 * Version    Author                     Date             Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * 1.0        Erdem Tayfun Salman       11/14/2014       Initial Version
 ******************************************************************************/


#include <htc.h>
#include <stdlib.h>
#include <stdio.h>
#include "HardwareProfile.h"
#include "uart.h"
#include "adc.h"
#include "StringSerialProtocol.h"
#include "main.h"

__CONFIG(FOSC_INTRC_NOCLKOUT & WDTE_ON & PWRTE_ON & MCLRE_ON & CP_ON & CPD_ON & BOREN_OFF & IESO_OFF & FCMEN_OFF & LVP_OFF & DEBUG_OFF & BOR4V_BOR21V);
//__CONFIG(FOSC_INTRC_NOCLKOUT & WDTE_OFF & PWRTE_OFF & MCLRE_ON & CP_OFF & CPD_OFF & BOREN_OFF & IESO_OFF & FCMEN_OFF & LVP_ON& DEBUG_OFF & BOR4V_BOR21V );

//#define DEBUG


#define UPPER_TEMP_LIMIT        35
#define LOWER_TEMP_LIMIT        30



// Üretim Konfigurasyonu
__EEPROM_DATA('1', '0', 0, 0, 0, DEFAULT_TEMPERATURE_OFFSET_0, DEFAULT_TEMPERATURE_OFFSET_1, DEFAULT_TEMPERATURE_OFFSET_2);


/* DEFINE LOCAL TYPES HERE ---------------------------------------------------*/
/* DEFINE LOCAL MACROS HERE --------------------------------------------------*/
#define bit_set(var,bitno)          ((var) |= 1 << (bitno))
#define bit_clear(var,bitno)        ((var) &= ~(1 << (bitno)))
#define bit_test(data,bitno)        ((data>>bitno)&0x01)
#define bit_test_int(data,bitno) 	((data>>bitno)&0x0001)
#define hibyte(x)                   (unsigned char)	((x>>8) & 0xFF)
#define lobyte(x)                   (unsigned char)	(x & 0xFF)

/* DEFINE LOCAL VARIABLES HERE -----------------------------------------------*/
//Timer variables
volatile unsigned char tim10Hz, tim2Hz, tim1sec, tim1min;
volatile unsigned char RELAY_1_Timer;
bit RELAY_1_Timer_En;
volatile unsigned char RELAY_2_Timer;
bit RELAY_2_Timer_En;
volatile unsigned char RELAY_3_Timer;
bit RELAY_3_Timer_En;
volatile unsigned char RELAY_4_Timer;
bit RELAY_4_Timer_En;
volatile unsigned char RELAY_5_Timer;
bit RELAY_5_Timer_En;

volatile unsigned int communicationControlTimer;
bit communicationControlTimer_En;
unsigned char inputPort;

volatile unsigned char timer = 0;

const float a = 3300.0 / 1024.0;
volatile signed int temperature_mV;
char currentTemperatureBuffer[6];
char printBuffer[6];
volatile signed int currentTemperature;
bit timeoutFlag;

const signed int V1[11] = {1299, 1247, 1141, 1034, 925, 816, 704, 591, 476, 361, 243};
const signed int V2[11] = {1252, 1146, 1039, 931, 821, 710, 596, 482, 367, 249, 183};
const signed int T1[11] = {-50, -40, -20, 0, 20, 40, 60, 80, 100, 120, 140};
const signed int T2[11] = {-41, -21, -1, 19, 39, 59, 79, 99, 119, 139, 150};
signed int calgulateTemp(signed int milliVolt);


/* DECLARE EXTERNAL VARIABLES HERE -------------------------------------------*/
extern volatile unsigned char uartTransmitBufferR;
extern volatile unsigned char uartTransmitBufferW;
/* DECLARE LOCAL FUNCTIONS HERE ----------------------------------------------*/
void vInitializeBoard(void);
void vPulseRelay(unsigned char relay, unsigned char timerValue);
void vSetRelay(unsigned char relay, unsigned char value);
void setCommunicationTimeOut(unsigned int value);
void restartUSR_K2(void);
unsigned int getTemperature(void);
void vResetPrintBuffer(void);
void vSendTemperatureToBuffer(int convData);

/* DEFINE FUNCTIONS HERE -----------------------------------------------------*/
/* DEFINE LOCAL FUNCTIONS HERE -----------------------------------------------*/

/*******************************************************************************
 *
 * Function     :
 * PreCondition : None<
 * Input        : None
 * Output       : None
 * Side Effects : None
 * Overview     :
 * Note         : None
 *
 ******************************************************************************/
main(void) {
    vInitializeBoard();
    vInitializeUart();
    vInitializeADC();
    temperature_mV = 0;
    PEIE = 1;
    GIE = 1;
    ei();

    inputPort = (PORTB & 0b00110111);
    setCommunicationTimeOut(COMMUNICATION_TIMEOUT_VALUE); //Cihaza module 60 sn boyunca bir komut gelmezse ethernet modul resetlenir.

    while (1) {
        CLRWDT();

        if (timeoutFlag) {
            timeoutFlag = 0;
            temperature_mV = getTemperature();
            currentTemperature = calgulateTemp(temperature_mV);
            currentTemperature += temperatureOffset();
            
        }
        if (currentTemperature >= UPPER_TEMP_LIMIT) {
            RELAY_1 = 1;
        } else if (currentTemperature <= LOWER_TEMP_LIMIT) {
            RELAY_1 = 0;
        }

        if (inputPort != (PORTB & 0b00010111)) {
            __delay_ms(40);
            if (inputPort != (PORTB & 0b00010111)) {
                inputPort = (PORTB & 0b00010111);

                vAddToUartTransmitBuffer('<');
                vAddToUartTransmitBuffer('D');
                vAddToUartTransmitBuffer('U');
                vAddToUartTransmitBuffer('R');
                vAddToUartTransmitBuffer('U');
                vAddToUartTransmitBuffer('M');
                vAddToUartTransmitBuffer(':');
                vAddToUartTransmitBuffer(INPUT_1 + 48);
                vAddToUartTransmitBuffer(INPUT_2 + 48);
                vAddToUartTransmitBuffer(INPUT_3 + 48);
                vAddToUartTransmitBuffer(INPUT_4 + 48);
                vAddToUartTransmitBuffer('0');
                vAddToUartTransmitBuffer('0');
                vAddToUartTransmitBuffer('>');
                vSendDataToUart();
            }
        }

        StringSerialProtocolService();
    }
}

void interrupt erdem(void) {
    unsigned char temp;
    //    unsigned int i;

    if (T0IE && T0IF) {
        TMR0 = 190; //  (1/2MHz)*256*66*12 = 100uS

        if (--tim10Hz == 0) //100 mS
        {
            tim10Hz = 12;

            if ((--RELAY_1_Timer == 0) && RELAY_1_Timer_En) {
                RELAY_1 = 0;
                RELAY_1_Timer_En = 0;
            }
            if ((--RELAY_2_Timer == 0) && RELAY_2_Timer_En) {
                RELAY_2 = 0;
                RELAY_2_Timer_En = 0;
            }
            if ((--RELAY_3_Timer == 0) && RELAY_3_Timer_En) {
                RELAY_3 = 0;
                RELAY_3_Timer_En = 0;
            }
            if ((--RELAY_4_Timer == 0) && RELAY_4_Timer_En) {
                RELAY_4 = 0;
                RELAY_4_Timer_En = 0;
            }
            if ((--RELAY_5_Timer == 0) && RELAY_5_Timer_En) {
                RELAY_5 = 0;
                RELAY_5_Timer_En = 0;
            }
            if ((--communicationControlTimer == 0) && communicationControlTimer_En) {
                restartUSR_K2();
                communicationControlTimer = COMMUNICATION_TIMEOUT_VALUE;
            }

            if (--tim2Hz == 0) //500 mS
            {
                tim2Hz = 5;
                if (--tim1sec == 0) // 1 second
                {
                    tim1sec = 2;
                    LED_BLUE ^= 1;
                    timeoutFlag = 1;

                }//--------end period 1 sec.-----------------------------
            }//--------end period 2 Hz.-----------------------------
        }//--------end period 10 Hz.-----------------------------

        T0IF = 0;
        return;
    }

    if (RCIE && RCIF) {
        if (OERR) {
            CREN = 0;
            CREN = 1;
        }
        if (FERR) {
            temp = RCREG;
        }
        while (!RCIF) {
            vCheckError();
        }

        temp = RCREG;
        vAddToUartReceiveBuffer(temp);
        return;
    }

    if (TXIE && TXIF) {
        if (uartTransmitBufferR != uartTransmitBufferW) {
            temp = cGetFromTxBuffer();
            TXREG = temp;
        } else {
            TXIE = 0;
            TXIF = 0;
        }
        return;
    }
    return;
}

/*******************************************************************************
 *
 * Function     : void vPulseRelay(unsigned char relay, unsigned char timerValue)
 * PreCondition : None
 * Input        : relay: Relay No
 *              : timerValue : Relay pulse time x 100ms
 * Output       : None
 * Side Effects : None
 * Overview     :
 * Note         : None
 *
 ******************************************************************************/
void vPulseRelay(unsigned char relay, unsigned char timerValue) {
    switch (relay) {
        case 1:
            RELAY_1 = 1;
            RELAY_1_Timer = timerValue;
            RELAY_1_Timer_En = 1;
            break;
        case 2:
            RELAY_2 = 1;
            RELAY_2_Timer = timerValue;
            RELAY_2_Timer_En = 1;
            break;
        case 3:
            RELAY_3 = 1;
            RELAY_3_Timer = timerValue;
            RELAY_3_Timer_En = 1;
            break;
        case 4:
            RELAY_4 = 1;
            RELAY_4_Timer = timerValue;
            RELAY_4_Timer_En = 1;
            break;
        case 5:
            RELAY_5 = 1;
            RELAY_5_Timer = timerValue;
            RELAY_5_Timer_En = 1;
            break;
        default:
            break;
    }
}

/*******************************************************************************
 *
 * Function     : void vPulseRelay(unsigned char relay, unsigned char timerValue)
 * PreCondition : None
 * Input        : relay: Relay No
 *              : timerValue : Relay pulse time x 100ms
 * Output       : None
 * Side Effects : None
 * Overview     :
 * Note         : None
 *
 ******************************************************************************/
void vSetRelay(unsigned char relay, unsigned char value) {
    switch (relay) {
        case 1:
            RELAY_1 = value;
            break;
        case 2:
            RELAY_2 = value;
            break;
        case 3:
            RELAY_3 = value;
            break;
        case 4:
            RELAY_4 = value;
            break;
        case 5:
            RELAY_5 = value;
            break;
        default:
            break;
    }
}

/*******************************************************************************
 *
 * Function     : void vInitializeBoard(void)
 * PreCondition : None
 * Input        : None
 * Output       : None
 * Side Effects : None
 * Overview     :
 * Note         : None
 *
 ******************************************************************************/
void vInitializeBoard(void) {
    PORTA = 0;
    PORTB = 0;
    PORTC = 0;
    PORTD = 0;
    PORTE = 0;

    OSCCON = 0b01110101;

    CM1CON0 = 0;
    CM2CON0 = 0;
    C1ON = 0;
    C2ON = 0;

    WDTCON = 0b00010110;
    OPTION_REG = 0b10000111; //pull ups disable TMR0 prescaler 1:256
    INTCON = 0;
    PIE1 = 0;
    PIE2 = 0;

    ANSELH = 0;
    ANSEL = 0;
    WPUB = 0;

    T0IF = 0;
    T0IE = 1;

    TRISA = 0b00000000; //
    TRISB = 0b00110111; //
    TRISC = 0b11000000; //
    TRISD = 0b00000000; //
    TRISE = 0b00000000; //
    ETH_RESET_TRIS = 0;
    ETH_RESET = 0;

    PORTA = 0;
    PORTB = 0;
    PORTC = 0;
    PORTD = 0;
    PORTE = 0;

    tim10Hz = 5;
    tim2Hz = 5;
    tim1sec = 2;
}

void setCommunicationTimeOut(unsigned int value) {
    communicationControlTimer = value;
    communicationControlTimer_En = 1;
}

void restartUSR_K2(void) {
    ETH_RESET = 1;
#ifdef DEBUG
    RELAY_5 = 1;
#endif
    __delay_ms(40);
    __delay_ms(40);
    __delay_ms(40);
    __delay_ms(40);
    __delay_ms(40);
    __delay_ms(40);
    __delay_ms(40);
    ETH_RESET = 0;

#ifdef DEBUG
    RELAY_5 = 0;
#endif
    setCommunicationTimeOut(COMMUNICATION_TIMEOUT_VALUE); //Cihaza module x sn boyunca bir komut gelmezse ethernet modul resetlenir.
}

/*******************************************************************************
 *
 * Function     : unsigned char getTemperature(void)
 * PreCondition : None
 * Input        : None
 * Output       : None
 * Side Effects : None
 * Overview     :
 * Note         : None
 *
 ******************************************************************************/
unsigned int getTemperature(void) {
    unsigned int iTempP;
    float milliVolt;
    unsigned int m;

    iTempP = 0;
    for (m = 0; m < 16; m++) {
        iTempP += read_ADC(LM9402);
    }
    iTempP /= 16;


    milliVolt = iTempP * a;
    m = (int) milliVolt;
    return m;
}

/*******************************************************************************
 *
 * Function     : void vResetPrintBuffer(void);
 * PreCondition : None
 * Input        : None
 * Output       : None
 * Side Effects : None
 * Overview     :
 * Note         : None
 *
 ******************************************************************************/
void vResetPrintBuffer(void) {
    unsigned char i;

    for (i = 0; i<sizeof (printBuffer); i++) {
        printBuffer[i] = 0;
    }
}

void vSendTemperatureToBuffer(int convData) {
    itoa(currentTemperatureBuffer, convData, 10);

    printBuffer[0] = '+';
    printBuffer[1] = currentTemperatureBuffer[0];
    printBuffer[2] = currentTemperatureBuffer[1];
    printBuffer[3] = currentTemperatureBuffer[2];
    printBuffer[4] = 'C';
    printBuffer[5] = '\0';

    if (convData < 100) {
        printBuffer[0] = '+';
        printBuffer[1] = '0';
        printBuffer[2] = currentTemperatureBuffer[0];
        printBuffer[3] = currentTemperatureBuffer[1];
        printBuffer[4] = 'C';
        printBuffer[5] = '\0';
    }
    if (convData < 10) {
        printBuffer[0] = '+';
        printBuffer[1] = '0';
        printBuffer[2] = '0';
        printBuffer[3] = currentTemperatureBuffer[0];
        printBuffer[4] = 'C';
        printBuffer[5] = '\0';
    }
    if (convData == 0) {
        printBuffer[0] = '0';
        printBuffer[1] = '0';
        printBuffer[2] = '0';
        printBuffer[3] = currentTemperatureBuffer[0];
        printBuffer[4] = 'C';
        printBuffer[5] = '\0';
    }
    if ((convData > -10) && (convData < 0)) {
        printBuffer[0] = currentTemperatureBuffer[0];
        printBuffer[1] = '0';
        printBuffer[2] = '0';
        printBuffer[3] = currentTemperatureBuffer[1];
        printBuffer[4] = 'C';
        printBuffer[5] = '\0';
    }
    if ((convData > -51) && (convData <= -10)) {
        printBuffer[0] = currentTemperatureBuffer[0];
        printBuffer[1] = '0';
        printBuffer[2] = currentTemperatureBuffer[1];
        printBuffer[3] = currentTemperatureBuffer[2];
        printBuffer[4] = 'C';
        printBuffer[5] = '\0';
    }
}

signed int calgulateTemp(signed int milliVolt) {
    signed int V1_, V2_, T1_, T2_;
    signed int temperature;
    if (milliVolt > 1247) {
        V1_ = V1[0];
        V2_ = V2[0];
        T1_ = T1[0];
        T2_ = T2[0];
    } else if (milliVolt > 1141) {
        V1_ = V1[1];
        V2_ = V2[1];
        T1_ = T1[1];
        T2_ = T2[1];
    } else if (milliVolt > 1034) {
        V1_ = V1[2];
        V2_ = V2[2];
        T1_ = T1[2];
        T2_ = T2[2];
    } else if (milliVolt > 925) {
        V1_ = V1[3];
        V2_ = V2[3];
        T1_ = T1[3];
        T2_ = T2[3];
    } else if (milliVolt > 816) {
        V1_ = V1[4];
        V2_ = V2[4];
        T1_ = T1[4];
        T2_ = T2[4];
    } else if (milliVolt > 704) {
        V1_ = V1[5];
        V2_ = V2[5];
        T1_ = T1[5];
        T2_ = T2[5];
    } else if (milliVolt > 591) {
        V1_ = V1[6];
        V2_ = V2[6];
        T1_ = T1[6];
        T2_ = T2[6];
    } else if (milliVolt > 476) {
        V1_ = V1[7];
        V2_ = V2[7];
        T1_ = T1[7];
        T2_ = T2[7];
    } else if (milliVolt > 361) {
        V1_ = V1[8];
        V2_ = V2[8];
        T1_ = T1[8];
        T2_ = T2[8];
    } else if (milliVolt > 243) {
        V1_ = V1[9];
        V2_ = V2[9];
        T1_ = T1[9];
        T2_ = T2[9];
    } else if (milliVolt > 182) {
        V1_ = V1[10];
        V2_ = V2[10];
        T1_ = T1[10];
        T2_ = T2[10];
    }

    temperature = (int) ((milliVolt - V1_)*((float) (T2_ - T1_) / (float) (V2_ - V1_)) + T1_);
    if ((milliVolt < 183) || (milliVolt > 1299)) {
        temperature = 255;
    }
    return temperature;
}

signed int temperatureOffset(void)
{
    signed int offset = 0;
    
    offset = 10 * (EEPROM_READ(TEMPERATURE_OFFSET_EEADDR_1) - 48);
    offset += (EEPROM_READ(TEMPERATURE_OFFSET_EEADDR_2) - 48);
    
    if(EEPROM_READ(TEMPERATURE_OFFSET_EEADDR_0) == '-')
    {
        offset = offset * (-1);
    }
    return offset;
}