//
// Copyright 2010-2023 Cypress Semiconductor Corporation
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "cyu3system.h"
#include "cyu3error.h"
#include "cyu3pib.h"
#include "cyu3pib.h"

#include "app.h"
#include "gpif.h"

void GpifInit()
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    // fv
    //CyU3PPibClock_t pibClk = {4, CyFalse, CyFalse, CY_U3P_SYS_CLK};
    CyU3PPibClock_t pibClk = {2, CyFalse, CyFalse, CY_U3P_SYS_CLK};

    /* Initialize the PIB block. */
    apiRetStatus = CyU3PPibInit (CyTrue, &pibClk);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "PIB Init failed, error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
}

/* [ ] */

