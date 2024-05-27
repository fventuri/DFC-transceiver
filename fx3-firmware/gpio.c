//
// Copyright 2010-2023 Cypress Semiconductor Corporation
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "cyu3system.h"
#include "cyu3error.h"
#include "cyu3gpio.h"

#include "app.h"
#include "gpio.h"

void GpioInit()
{
    const uint8_t ADC0_SHDN_GPIO = 17;
    const uint8_t DAC_SHDN_GPIO = 18;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    CyU3PGpioClock_t        gpioClock;
    CyU3PGpioSimpleConfig_t gpioConf = {CyFalse, CyTrue, CyTrue, CyFalse, CY_U3P_GPIO_NO_INTR};

    /* Initialize the GPIO block. */
    gpioClock.fastClkDiv = 2;
    gpioClock.slowClkDiv = 32;
    gpioClock.simpleDiv  = CY_U3P_GPIO_SIMPLE_DIV_BY_16;
    gpioClock.clkSrc     = CY_U3P_SYS_CLK_BY_2;
    gpioClock.halfDiv    = 0;
 
    apiRetStatus = CyU3PGpioInit (&gpioClock, NULL);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PGpioInit failed, error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
 
    /* Override PIN 17 (ADC0 SHDN) as a simple GPIO. */
    apiRetStatus = CyU3PDeviceGpioOverride (ADC0_SHDN_GPIO, CyTrue);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PDeviceGpioOverride failed, error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Set PIN 17 (ADC0 SHDN) to low for normal ADC operation. */
    apiRetStatus = CyU3PGpioSetSimpleConfig (ADC0_SHDN_GPIO, &gpioConf);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PGpioSetSimpleConfig failed, error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
 
    /* Override PIN 18 (DAC SHDN) as a simple GPIO. */
    apiRetStatus = CyU3PDeviceGpioOverride (DAC_SHDN_GPIO, CyTrue);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PDeviceGpioOverride failed, error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Set PIN 18 (DAC SHDN) to low for normal DAC operation. */
    apiRetStatus = CyU3PGpioSetSimpleConfig (DAC_SHDN_GPIO, &gpioConf);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PGpioSetSimpleConfig failed, error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
}

void GpioShutdownAdc(CyBool_t shutdown)
{
    const uint8_t ADC0_SHDN_GPIO = 17;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    CyU3PGpioSimpleConfig_t gpioConf = {shutdown, CyTrue, CyTrue, CyFalse, CY_U3P_GPIO_NO_INTR};

    /* Set PIN 17 (ADC0 SHDN) to high/low to shutdown/wakeup ADC0. */
    apiRetStatus = CyU3PGpioSetSimpleConfig (ADC0_SHDN_GPIO, &gpioConf);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PGpioSetSimpleConfig failed, error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
}

void GpioShutdownDac(CyBool_t shutdown)
{
    const uint8_t DAC_SHDN_GPIO = 18;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    CyU3PGpioSimpleConfig_t gpioConf = {shutdown, CyTrue, CyTrue, CyFalse, CY_U3P_GPIO_NO_INTR};

    /* Set PIN 18 (DAC SHDN) to high/low to shutdown/wakeup DAC. */
    apiRetStatus = CyU3PGpioSetSimpleConfig (DAC_SHDN_GPIO, &gpioConf);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PGpioSetSimpleConfig failed, error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
}

/* [ ] */

