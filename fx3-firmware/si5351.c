//
// Copyright 2010-2023 Cypress Semiconductor Corporation
// Copyright 2024 Franco Venturi
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "si5351.h"
#include "cyu3error.h"
#include "cyu3i2c.h"
#include <math.h>

static void rational_approximation(double value, uint32_t max_denominator,
                                   uint32_t *a, uint32_t *b, uint32_t *c);

CyBool_t Si5351(double reference, double frequency)
{
    static const double SI5351_MAX_VCO_FREQ = 900e6;
    static const uint32_t SI5351_MAX_DENOMINATOR = 1048575;
    static const double FREQUENCY_TOLERANCE = 1e-8;
 
    const uint8_t SI5351_ADDR = 0x60 << 1;
    //const uint8_t SI5351_REGISTER_DEVICE_STATUS = 0;
    const uint8_t SI5351_REGISTER_CLK_BASE      = 16;
    const uint8_t SI5351_REGISTER_MSNA_BASE     = 26;
    const uint8_t SI5351_REGISTER_MS0_BASE      = 42;
    const uint8_t SI5351_REGISTER_PLL_RESET     = 177;


    /* part A - compute all the Si5351 register settings */

    /* if the requested sample rate is below 1MHz, use an R divider */
    double r_frequency = frequency;
    uint8_t rdiv = 0;
    while (r_frequency < 1e6 && rdiv <= 7) {
        r_frequency *= 2.0;
      rdiv += 1;
    }
    if (r_frequency < 1e6) {
        CyU3PDebugPrint (4, "ERROR - Si5351() - requested frequency is too low\r\n");
        return CyFalse;
    }

    /* choose an even integer for the output MS */
    uint32_t output_ms = ((uint32_t)(SI5351_MAX_VCO_FREQ / r_frequency));
    output_ms -= output_ms % 2;
    if (output_ms < 4 || output_ms > 900) {
        CyU3PDebugPrint (4, "ERROR - Si5351() - invalid output MS: %d\r\n", output_ms);
        return CyFalse;
    }
    double vco_frequency = r_frequency * output_ms;

    /* feedback MS */
    double feedback_ms = vco_frequency / reference;
    /* find a good rational approximation for feedback_ms */
    uint32_t a;
    uint32_t b;
    uint32_t c;
    rational_approximation(feedback_ms, SI5351_MAX_DENOMINATOR, &a, &b, &c);

    double actual_ratio = a + (double)b / (double)c;
    double actual_frequency = reference * actual_ratio / output_ms / (1 << rdiv);
    double frequency_diff = actual_frequency - frequency;
    if (frequency_diff <= -FREQUENCY_TOLERANCE || frequency_diff >= FREQUENCY_TOLERANCE) {
        uint32_t frequency_diff_nhz = 1000000000 * frequency_diff;
        CyU3PDebugPrint (4, "WARNING - Si5351() - frequency difference=%dnHz\r\n", frequency_diff_nhz);
    }
    CyU3PDebugPrint (4, "INFO - Si5351() - a=%d b=%d c=%d output_ms=%d rdiv=%d\r\n", a, b , c, output_ms, rdiv);

    /* configure clock input and PLL */
    uint32_t b_over_c = 128 * b / c;
    uint32_t msn_p1 = 128 * a + b_over_c - 512;
    uint32_t msn_p2 = 128 * b  - c * b_over_c;
    uint32_t msn_p3 = c;
  
    uint8_t data_clkin[] = {
      (msn_p3 & 0x0000ff00) >>  8,
      (msn_p3 & 0x000000ff) >>  0,
      (msn_p1 & 0x00030000) >> 16,
      (msn_p1 & 0x0000ff00) >>  8,
      (msn_p1 & 0x000000ff) >>  0,
      (msn_p3 & 0x000f0000) >> 12 | (msn_p2 & 0x000f0000) >> 16,
      (msn_p2 & 0x0000ff00) >>  8,
      (msn_p2 & 0x000000ff) >>  0
    };

    /* configure clock output */
    /* since the output divider is an even integer a = output_ms, b = 0, c = 1 */
    uint32_t const ms_p1 = 128 * output_ms - 512;
    uint32_t const ms_p2 = 0;
    uint32_t const ms_p3 = 1;

    uint8_t data_clkout[] = {
      (ms_p3 & 0x0000ff00) >>  8,
      (ms_p3 & 0x000000ff) >>  0,
      rdiv << 4 | (output_ms == 4 ? 0xc : 0x0) | (ms_p1 & 0x00030000) >> 16,
      (ms_p1 & 0x0000ff00) >>  8,
      (ms_p1 & 0x000000ff) >>  0,
      (ms_p3 & 0x000f0000) >> 12 | (ms_p2 & 0x000f0000) >> 16,
      (ms_p2 & 0x0000ff00) >>  8,
      (ms_p2 & 0x000000ff) >>  0
    };


    /* part B - configure the Si5351 and start the clock */
    CyU3PI2cPreamble_t preamble;
    uint8_t data[8];
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    /* 1 - clock input and PLL */
    preamble.length = 2;
    preamble.buffer[0] = SI5351_ADDR | 0x0;   /* 0 -> write (write register address) */
    preamble.buffer[1] = SI5351_REGISTER_MSNA_BASE;
    preamble.ctrlMask  = 0x0000;
    apiRetStatus = CyU3PI2cTransmitBytes (&preamble, data_clkin, sizeof(data_clkin), 0);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "I2C Transmit Bytes failed, Error code = %d\r\n", apiRetStatus);
        return CyFalse;
    }

    /* 2 - clock output */
    preamble.length = 2;
    preamble.buffer[0] = SI5351_ADDR | 0x0;   /* 0 -> write (write register address) */
    preamble.buffer[1] = SI5351_REGISTER_MS0_BASE;
    preamble.ctrlMask  = 0x0000;
    apiRetStatus = CyU3PI2cTransmitBytes (&preamble, data_clkout, sizeof(data_clkout), 0);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "I2C Transmit Bytes failed, Error code = %d\r\n", apiRetStatus);
        return CyFalse;
    }

    /* 3 - reset PLL */
    preamble.length = 2;
    preamble.buffer[0] = SI5351_ADDR | 0x0;   /* 0 -> write (write register address) */
    preamble.buffer[1] = SI5351_REGISTER_PLL_RESET;
    preamble.ctrlMask  = 0x0000;
    data[0] = 0x20;
    apiRetStatus = CyU3PI2cTransmitBytes (&preamble, data, 1, 0);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "I2C Transmit Bytes failed, Error code = %d\r\n", apiRetStatus);
        return CyFalse;
    }

    /* 4 - power on clock 0 */
    preamble.length = 2;
    preamble.buffer[0] = SI5351_ADDR | 0x0;   /* 0 -> write (write register address) */
    preamble.buffer[1] = SI5351_REGISTER_CLK_BASE;
    preamble.ctrlMask  = 0x0000;
    data[0] = 0x4f;
    apiRetStatus = CyU3PI2cTransmitBytes (&preamble, data, 1, 0);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "I2C Transmit Bytes failed, Error code = %d\r\n", apiRetStatus);
        return CyFalse;
    }

    return CyTrue;
}


/* best rational approximation:
 *
 *     value ~= a + b/c     (where c <= max_denominator)
 *
 * References:
 * - https://en.wikipedia.org/wiki/Continued_fraction#Best_rational_approximations
 */
static void rational_approximation(double value, uint32_t max_denominator,
                                   uint32_t *a, uint32_t *b, uint32_t *c)
{
  const double epsilon = 1e-5;

  double af;
  double f0 = modf(value, &af);
  *a = (uint32_t) af;
  *b = 0;
  *c = 1;
  double f = f0;
  double delta = f0;
  /* we need to take into account that the fractional part has a_0 = 0 */
  uint32_t h[] = {1, 0};
  uint32_t k[] = {0, 1};
  for(int i = 0; i < 100; ++i){
    if(f <= epsilon){
      break;
    }
    double anf;
    f = modf(1.0 / f,&anf);
    uint32_t an = (uint32_t) anf;
    for(uint32_t m = (an + 1) / 2; m <= an; ++m){
      uint32_t hm = m * h[1] + h[0];
      uint32_t km = m * k[1] + k[0];
      if(km > max_denominator){
        break;
      }
      double d = fabs((double) hm / (double) km - f0);
      if(d < delta){
        delta = d;
        *b = hm;
        *c = km;
      }
    }
    uint32_t hn = an * h[1] + h[0];
    uint32_t kn = an * k[1] + k[0];
    h[0] = h[1]; h[1] = hn;
    k[0] = k[1]; k[1] = kn;
  }
  return;
}

/* [ ] */
