/*******************************************************************************
* File Name:   main.c
*
* Description: This code example demonstrates the implementation of an I2C 
*              master and an I2C slave using the Universal Serial Interface
*              Channel (USIC) module available in PSOC™ control C1 MCUs. 
*              It configures one USIC module as I2C master and another one as 
*              I2C slave on the same PSOC™ control C1 MCU using the device configurator. 
*              I2C master module sends commands to the I2C slave module to 
*              toggle the LEDs present on PSOC™ control C1 evaluation kit. User has 
*              to make the connection between I2C master and I2C slave externally.
*
* Related Document: See README.md
*
********************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*****************************************************************************/

#include "cybsp.h"
#include "cy_utils.h"

/*******************************************************************************
* Defines
*******************************************************************************/
/* SysTick timer frequency in Hz  */
#define TICKS_PER_SECOND           1000

/* I2C Master sends every 500ms a command to toggle LED on slave */
#define I2C_MASTER_SEND_TASK_MS    500

/* 8-bit command patterns to set LED port on slave to high/low */
#define CMD_LED_HIGH               0xaa
#define CMD_LED_LOW                0x55

/* I2C receive event interrupt priority */
#define I2C_RECEIVE_EVENT_PRIORITY 63

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Variable for keeping track of time */
static volatile uint32_t ticks = 0;

/*******************************************************************************
* Function Name: SysTick_Handler
********************************************************************************
* Summary:
* This is the interrupt handler function for the SysTick timer interrupt.
* It counts the time elapsed in milliseconds since the timer started.
* Every I2C_MASTER_SEND_TASK_MS milliseconds, an I2C-command is sent from
* I2C-master to I2C-slave to toggle a LED.
*
* Parameters:
*  none
*
* Return:
*  none
*
*******************************************************************************/
void SysTick_Handler(void)
{
    static uint8_t tx_master_send = CMD_LED_LOW;
    static uint32_t ticks = 0;

    ticks++;
    if (ticks == I2C_MASTER_SEND_TASK_MS)
    {
        /* Prepare command to toggle LED on I2C slave */
        switch (tx_master_send)
        {
            case CMD_LED_LOW:
            {
                tx_master_send = CMD_LED_HIGH;
            }
            break;
            case CMD_LED_HIGH:
            {
                tx_master_send = CMD_LED_LOW;
            }
            break;
        }

        /* Send START conditon */
        Cy_I2C_CH_MasterStart(I2C_MASTER_HW, I2C_SLAVE_SLAVE_ADDRESS, CY_I2C_CH_CMD_WRITE);

        /* Wait for acknowledge and reset status */
        while((Cy_I2C_CH_GetStatusFlag(I2C_MASTER_HW) & CY_I2C_CH_STATUS_FLAG_ACK_RECEIVED) == 0U)
        {
            /* wait for ACK from slave */
        }

        Cy_I2C_CH_ClearStatusFlag(I2C_MASTER_HW,(uint32_t)CY_I2C_CH_STATUS_FLAG_ACK_RECEIVED);

        /* Transmit next command from I2C master to I2C slave */
        Cy_I2C_CH_MasterTransmit(I2C_MASTER_HW, tx_master_send);

        /* Wait for acknowledge and reset status */
        while((Cy_I2C_CH_GetStatusFlag(I2C_MASTER_HW) & CY_I2C_CH_STATUS_FLAG_ACK_RECEIVED) == 0U)
        {
            /* wait for ACK from slave */
        }

        Cy_I2C_CH_ClearStatusFlag(I2C_MASTER_HW,(uint32_t)CY_I2C_CH_STATUS_FLAG_ACK_RECEIVED);

        /* Wait until TX FIFO is empty */
        while (!Cy_USIC_CH_TXFIFO_IsEmpty(I2C_MASTER_HW))
        {
            /* wait until all data is sent by HW */
        }

        /* Send STOP conditon */
        Cy_I2C_CH_MasterStop(I2C_MASTER_HW);

        /* Reset ticks counter */
        ticks = 0;
    }
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function.
* The I2C master and slave interface are initialized using personalities.
* Interrupt priority is set for the receive event of the I2C slave.
* Systick timer is initialized to call SysTick_Handler every 1 ms.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Set NVIC priority */
    NVIC_SetPriority(I2C_SLAVE_RECEIVE_EVENT_IRQN, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), I2C_RECEIVE_EVENT_PRIORITY, 0));

    /* Enable IRQ */
    NVIC_EnableIRQ(I2C_SLAVE_RECEIVE_EVENT_IRQN);

    /* System timer configuration */
    SysTick_Config(SystemCoreClock / TICKS_PER_SECOND);
    while(1);
}

/*******************************************************************************
* Function Name: I2C_SLAVE_RECEIVE_EVENT_HANDLER
********************************************************************************
* Summary:
* This is the interrupt handler function for the I2C slave receive handler.
* It is called every time a message is received by the I2C slave peripheral.
* Based on the received command, the LED is turned on/off.
*
* Parameters:
*  none
*
* Return:
*  none
*
*******************************************************************************/
void I2C_SLAVE_RECEIVE_EVENT_HANDLER()
{
    /* Read received data from I2C module */
    uint8_t rx_slave_receive = Cy_I2C_CH_GetReceivedData(I2C_SLAVE_HW);

    /* Interpret command and set LED state accordingly */
    switch(rx_slave_receive)
    {
        case CMD_LED_HIGH:
        {
            Cy_GPIO_SetOutputHigh(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
        }
        break;
        case CMD_LED_LOW:
        {
            Cy_GPIO_SetOutputLow(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
        }
        break;
    }
}

/* [] END OF FILE */
