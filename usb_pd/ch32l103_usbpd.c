/********************************** (C) COPYRIGHT  *******************************
 * File Name          : ch32l103_usbpd.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2024/10/25
 * Description        : This file provides all the USBPD peripheral firmware functions.
 *********************************************************************************
 * Copyright (c) 2023 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "ch32l103_usbpd.h"

/* PD Receive and Transmit Buffers - defined in application layer, extern here */
extern __attribute__ ((aligned(4))) uint8_t PD_Rx_Buf[ 34 ];
extern __attribute__ ((aligned(4))) uint8_t PD_Tx_Buf[ 34 ];
extern UINT8 PD_Ack_Buf[ 2 ];

/* PD Control Structure - defined in application layer, extern here */
extern PD_CONTROL PD_Ctl;

/*********************************************************************
 * @fn      USBPD_Init
 *
 * @brief   Initializes the USBPD peripheral according to the specified
 *          parameters in the USBPD_InitStruct.
 *
 * @param   USBPD_InitStruct: pointer to a USBPD_InitTypeDef structure that
 *          contains the configuration information for the USBPD peripheral.
 *
 * @return  none
 */
void USBPD_Init(USBPD_InitTypeDef* USBPD_InitStruct)
{
    /* Enable USBPD clock */
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBPD, ENABLE);

    /* Configure CONFIG register */
    USBPD->CONFIG = (USBPD_InitStruct->USBPD_FiltEn << 0) |
                    (USBPD_InitStruct->USBPD_DMAEn << 3) |
                    (USBPD_InitStruct->USBPD_RstEn << 4) |
                    (USBPD_InitStruct->USBPD_WakePolar << 5);

    /* Clear pending interrupts */
    USBPD->STATUS = BUF_ERR | IF_RX_BIT | IF_RX_BYTE | IF_RX_ACT | IF_RX_RESET | IF_TX_END;
}

/*********************************************************************
 * @fn      USBPD_StructInit
 *
 * @brief   Fills each USBPD_InitStruct member with its default value.
 *
 * @param   USBPD_InitStruct: pointer to a USBPD_InitTypeDef structure which
 *          will be initialized.
 *
 * @return  none
 */
void USBPD_StructInit(USBPD_InitTypeDef* USBPD_InitStruct)
{
    USBPD_InitStruct->USBPD_FiltEn = 0;
    USBPD_InitStruct->USBPD_DMAEn = 0;
    USBPD_InitStruct->USBPD_RstEn = 0;
    USBPD_InitStruct->USBPD_WakePolar = 0;
}

/*********************************************************************
 * @fn      USBPD_CCCmdConfig
 *
 * @brief   Configures the CC port (CC1 or CC2) with specified settings.
 *
 * @param   CCx: CC port to configure (USBPD_PORT_CC1 or USBPD_PORT_CC2).
 *          NewState: ENABLE or DISABLE the specific configuration.
 *          CC_Cmd: The command/configuration to apply (e.g., CC_PU_330, CC_PD).
 *
 * @return  none
 */
void USBPD_CCCmdConfig(uint8_t CCx, FunctionalState NewState, uint32_t CC_Cmd)
{
    volatile uint32_t *CC_Reg;

    if (CCx == USBPD_PORT_CC1)
    {
        CC_Reg = &USBPD->PORT_CC1;
    }
    else
    {
        CC_Reg = &USBPD->PORT_CC2;
    }

    if (NewState != DISABLE)
    {
        *CC_Reg |= CC_Cmd;
    }
    else
    {
        *CC_Reg &= ~CC_Cmd;
    }
}

/*********************************************************************
 * @fn      USBPD_SetCCComparator
 *
 * @brief   Sets the voltage comparator threshold for the specified CC port.
 *
 * @param   CCx: CC port (USBPD_PORT_CC1 or USBPD_PORT_CC2).
 *          Comparator: Comparator threshold value (e.g., CC_CMP_66).
 *
 * @return  none
 */
void USBPD_SetCCComparator(uint8_t CCx, uint32_t Comparator)
{
    volatile uint32_t *CC_Reg;

    if (CCx == USBPD_PORT_CC1)
    {
        CC_Reg = &USBPD->PORT_CC1;
    }
    else
    {
        CC_Reg = &USBPD->PORT_CC2;
    }

    *CC_Reg = (*CC_Reg & ~CC_CE) | Comparator;
}

/*********************************************************************
 * @fn      USBPD_SetCCPullUp
 *
 * @brief   Sets the pull-up current for the specified CC port (for Source mode).
 *
 * @param   CCx: CC port (USBPD_PORT_CC1 or USBPD_PORT_CC2).
 *          PullUp: Pull-up current value (e.g., CC_PU_330).
 *
 * @return  none
 */
void USBPD_SetCCPullUp(uint8_t CCx, uint32_t PullUp)
{
    volatile uint32_t *CC_Reg;

    if (CCx == USBPD_PORT_CC1)
    {
        CC_Reg = &USBPD->PORT_CC1;
    }
    else
    {
        CC_Reg = &USBPD->PORT_CC2;
    }

    *CC_Reg = (*CC_Reg & ~CC_PU_Mask) | PullUp;
}

/*********************************************************************
 * @fn      USBPD_SetCCPullDown
 *
 * @brief   Enables or disables the pull-down resistor for the specified CC port (for Sink mode).
 *
 * @param   CCx: CC port (USBPD_PORT_CC1 or USBPD_PORT_CC2).
 *          NewState: ENABLE or DISABLE the pull-down resistor.
 *
 * @return  none
 */
void USBPD_SetCCPullDown(uint8_t CCx, FunctionalState NewState)
{
    volatile uint32_t *CC_Reg;

    if (CCx == USBPD_PORT_CC1)
    {
        CC_Reg = &USBPD->PORT_CC1;
    }
    else
    {
        CC_Reg = &USBPD->PORT_CC2;
    }

    if (NewState != DISABLE)
    {
        *CC_Reg |= CC_PD;
    }
    else
    {
        *CC_Reg &= ~CC_PD;
    }
}

/*********************************************************************
 * @fn      USBPD_PHY_SendPack
 *
 * @brief   Sends a PD packet via the PHY layer.
 *
 * @param   mode: 0 - do not wait for transmission complete; 1 - wait for complete.
 *          pbuf: pointer to the data buffer.
 *          len: length of the data to send (in bytes).
 *          sop: SOP sequence type (e.g., UPD_SOP0, UPD_HARD_RESET).
 *
 * @return  none
 */
void USBPD_PHY_SendPack( UINT8 mode, UINT8 *pbuf, UINT8 len, UINT8 sop )
{
    /* Select the appropriate CC line for low voltage output */
    if ((USBPD->CONFIG & CC_SEL) == CC_SEL )
    {
        USBPD->PORT_CC2 |= CC_LVE;
    }
    else
    {
        USBPD->PORT_CC1 |= CC_LVE;
    }

    /* Set transmit timer based on system clock (assuming 96MHz) */
    USBPD->BMC_CLK_CNT = UPD_TMR_TX_96M;

    /* Set DMA address */
    USBPD->DMA = (UINT32)(UINT8 *)pbuf;

    /* Set SOP type */
    USBPD->TX_SEL = sop;

    /* Set transmit size */
    USBPD->BMC_TX_SZ = len;

    /* Enable transmitter and start BMC */
    USBPD->CONTROL |= PD_TX_EN;
    USBPD->STATUS &= BMC_AUX_INVALID;
    USBPD->CONTROL |= BMC_START;

    /* If mode is set to wait for completion */
    if( mode )
    {
        /* Wait for transmission to complete */
        while( (USBPD->STATUS & IF_TX_END) == 0 );
        USBPD->STATUS |= IF_TX_END;

        /* Disable low voltage output on the active CC line */
        if((USBPD->CONFIG & CC_SEL) == CC_SEL )
        {
            USBPD->PORT_CC2 &= ~CC_LVE;
        }
        else
        {
            USBPD->PORT_CC1 &= ~CC_LVE;
        }

        /* Switch back to receive mode */
        USBPD->CONFIG |=  PD_ALL_CLR ;
        USBPD->CONFIG &= ~( PD_ALL_CLR );
        USBPD->CONTROL &= ~ ( PD_TX_EN );
        USBPD->DMA = (UINT32)(UINT8 *)PD_Rx_Buf;
        USBPD->BMC_CLK_CNT = UPD_TMR_RX_96M;
        USBPD->CONTROL |= BMC_START;
    }
}

/*********************************************************************
 * @fn      USBPD_EnterRxMode
 *
 * @brief   Configures the USBPD peripheral to enter receive mode.
 *
 * @return  none
 */
void USBPD_EnterRxMode( void )
{
    /* Clear all interrupts and flags */
    USBPD->CONFIG |= PD_ALL_CLR;
    USBPD->CONFIG &= ~PD_ALL_CLR;

    /* Enable receive and reset interrupts, enable DMA */
    USBPD->CONFIG |= IE_RX_ACT | IE_RX_RESET|PD_DMA_EN;

    /* Set DMA address to the receive buffer */
    USBPD->DMA = (UINT32)(UINT8 *)PD_Rx_Buf;

    /* Disable transmitter */
    USBPD->CONTROL &= ~PD_TX_EN;

    /* Set receive timer (assuming 96MHz system clock) */
    USBPD->BMC_CLK_CNT = UPD_TMR_RX_96M;

    /* Start BMC for receiving */
    USBPD->CONTROL |= BMC_START ;

    /* Enable USBPD interrupt */
    NVIC_EnableIRQ( USBPD_IRQn );
}

/*********************************************************************
 * @fn      USBPD_SRC_Init
 *
 * @brief   Initializes the USBPD peripheral for Source (SRC) mode.
 *
 * @return  none
 */
void USBPD_SRC_Init( void )
{
    /* Set PR_Role to Source (1) */
    PD_Ctl.Flag.Bit.PR_Role = 1;
    /* Set auto-responder role to Source */
    PD_Ctl.Flag.Bit.Auto_Ack_PRRole = 1;

    /* Configure CC1 and CC2: 0.66V comparator, 330uA pull-up */
    USBPD->PORT_CC1 = CC_CMP_66 | CC_PU_330;
    USBPD->PORT_CC2 = CC_CMP_66 | CC_PU_330;
}

/*********************************************************************
 * @fn      USBPD_SINK_Init
 *
 * @brief   Initializes the USBPD peripheral for Sink (SNK) mode.
 *
 * @return  none
 */
void USBPD_SINK_Init( void )
{
    /* Set PR_Role to Sink (0) */
    PD_Ctl.Flag.Bit.PR_Role = 0;
    /* Set auto-responder role to Sink */
    PD_Ctl.Flag.Bit.Auto_Ack_PRRole = 0;

    /* Configure CC1 and CC2: 0.66V comparator, pull-down enabled */
    USBPD->PORT_CC1 = CC_CMP_66 | CC_PD;
    USBPD->PORT_CC2 = CC_CMP_66 | CC_PD;
}

/*********************************************************************
 * @fn      USBPD_GetCCAnalogInput
 *
 * @brief   Gets the analog input state of the specified CC port.
 *
 * @param   CCx: CC port (USBPD_PORT_CC1 or USBPD_PORT_CC2).
 *
 * @return  The analog input state (SET or RESET).
 */
FlagStatus USBPD_GetCCAnalogInput(uint8_t CCx)
{
    volatile uint32_t *CC_Reg;

    if (CCx == USBPD_PORT_CC1)
    {
        CC_Reg = &USBPD->PORT_CC1;
    }
    else
    {
        CC_Reg = &USBPD->PORT_CC2;
    }

    if (*CC_Reg & PA_CC_AI)
    {
        return SET;
    }
    else
    {
        return RESET;
    }
}

/*********************************************************************
 * @fn      USBPD_GetStatusFlag
 *
 * @brief   Gets the specified USBPD status flag.
 *
 * @param   USBPD_FLAG: specifies the flag to check.
 *          This parameter can be one of the following values:
 *            BUF_ERR: BUFFER or DMA error interrupt flag.
 *            IF_RX_BIT: Receive bit or 5bit interrupt flag.
 *            IF_RX_BYTE: Receive byte or SOP interrupt flag.
 *            IF_RX_ACT: Receive completion interrupt flag.
 *            IF_RX_RESET: Receive reset interrupt flag.
 *            IF_TX_END: Transfer completion interrupt flag.
 *
 * @return  The new state of USBPD_FLAG (SET or RESET).
 */
FlagStatus USBPD_GetStatusFlag(uint16_t USBPD_FLAG)
{
    if (USBPD->STATUS & USBPD_FLAG)
    {
        return SET;
    }
    else
    {
        return RESET;
    }
}

/*********************************************************************
 * @fn      USBPD_ClearStatusFlag
 *
 * @brief   Clears the specified USBPD status flag.
 *
 * @param   USBPD_FLAG: specifies the flag to clear.
 *          This parameter can be any combination of the following values:
 *            BUF_ERR, IF_RX_BIT, IF_RX_BYTE, IF_RX_ACT, IF_RX_RESET, IF_TX_END.
 *
 * @return  none
 */
void USBPD_ClearStatusFlag(uint16_t USBPD_FLAG)
{
    USBPD->STATUS |= USBPD_FLAG;
}

/*********************************************************************
 * @fn      USBPD_GetReceiveStatus
 *
 * @brief   Gets the current receive status (SOP type received).
 *
 * @return  The receive status (PD_RX_SOP0, PD_RX_SOP1_HRST, PD_RX_SOP2_CRST).
 */
uint8_t USBPD_GetReceiveStatus(void)
{
    return (USBPD->STATUS & MASK_PD_STAT);
}

/*********************************************************************
 * @fn      USBPD_ITConfig
 *
 * @brief   Enables or disables the specified USBPD interrupts.
 *
 * @param   USBPD_IT: specifies the USBPD interrupt sources to be enabled or disabled.
 *          This parameter can be any combination of the following values:
 *            IE_PD_IO, IE_RX_BIT, IE_RX_BYTE, IE_RX_ACT, IE_RX_RESET, IE_TX_END.
 *          NewState: new state of the specified USBPD interrupts.
 *
 * @return  none
 */
void USBPD_ITConfig(uint16_t USBPD_IT, FunctionalState NewState)
{
    if (NewState != DISABLE)
    {
        USBPD->CONFIG |= USBPD_IT;
    }
    else
    {
        USBPD->CONFIG &= ~USBPD_IT;
    }
}

/*********************************************************************
 * @fn      USBPD_GetITStatus
 *
 * @brief   Checks whether the specified USBPD interrupt has occurred.
 *          (This is an alias for checking the corresponding status flag).
 *
 * @param   USBPD_IT: specifies the USBPD interrupt source to check.
 *
 * @return  The new state of USBPD_IT (SET or RESET).
 */
ITStatus USBPD_GetITStatus(uint16_t USBPD_IT)
{
    /* The interrupt flags are in the STATUS register */
    if (USBPD->STATUS & USBPD_IT)
    {
        return SET;
    }
    else
    {
        return RESET;
    }
}

/*********************************************************************
 * @fn      USBPD_ClearITPendingBit
 *
 * @brief   Clears the USBPD's interrupt pending bits.
 *          (This is an alias for clearing the corresponding status flag).
 *
 * @param   USBPD_IT: specifies the interrupt pending bit to clear.
 *
 * @return  none
 */
void USBPD_ClearITPendingBit(uint16_t USBPD_IT)
{
    USBPD->STATUS |= USBPD_IT;
}
