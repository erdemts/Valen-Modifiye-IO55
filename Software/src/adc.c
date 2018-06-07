/*******************************************************************************
 *
 * FileName     : adc.c
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
#include "HardwareProfile.h"

/* DEFINE LOCAL CONSTANTS HERE -----------------------------------------------*/
/* DEFINE LOCAL TYPES HERE ---------------------------------------------------*/
/* DEFINE LOCAL MACROS HERE --------------------------------------------------*/
/* DEFINE LOCAL VARIABLES HERE -----------------------------------------------*/
// Uart Variables
/* DECLARE EXTERNAL VARIABLES HERE -------------------------------------------*/
/* DECLARE LOCAL FUNCTIONS HERE ----------------------------------------------*/
/* DEFINE FUNCTIONS HERE -----------------------------------------------------*/
/*******************************************************************************
 *
 * Function     : void vInitializeADC( void )
 * PreCondition : None
 * Input        : None
 * Output       : None
 * Side Effects : None
 * Overview     :
 * Note         : None
 *
 ******************************************************************************/
void vInitializeADC( void )
{
    LM94022_TEMP_SENS_TRIS = 1;

    ANSELHbits.ANS13 = 1;

    ADCON0bits.ADCS1 = 1;   //
    ADCON0bits.ADCS0 = 0;   //FOSC/32

    ADCON1bits.ADFM = 1;  //sa?a dayal?
    ADCON1bits.VCFG1 = 0; //Voltaj referans VSS
    ADCON1bits.VCFG0 = 0; //Voltaj referans VDD
    ADCON0bits.ADON = 1;    //Enable ADC
}

/*******************************************************************************
 *
 * Function     : unsigned char read_ADC( void )
 * PreCondition : None
 * Input        : None
 * Output       : None
 * Side Effects : None
 * Overview     :
 * Note         : None
 *
 ******************************************************************************/
unsigned int read_ADC( unsigned char analogChannel )
{
    unsigned int adcResult = 0;

    ADCON0bits.CHS = analogChannel;
    ADCON0bits.GO_DONE = 1;
    while(ADCON0bits.GO_DONE) continue;
    adcResult = ADRESH;
    adcResult <<= 8;
    adcResult |= ADRESL;

    return adcResult;
}





/* DEFINE LOCAL FUNCTIONS HERE -----------------------------------------------*/

