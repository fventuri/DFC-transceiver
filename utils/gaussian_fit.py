#!/usr/bin/env python3
# computes the best gaussian fit
# https://stackoverflow.com/a/38431524
# Franco Venturi - Sat Nov  4 11:22:40 AM EDT 2023

import numpy as np
import matplotlib.pyplot as plt
from scipy.optimize import curve_fit
from scipy.special import erfc
import sys

xdata = []
ydata = []
with open(sys.argv[1]) as f:
  for line in f:
    if not line.strip() or line.startswith('#'):
      continue
    x, y = line.strip().split()
    xdata.append(float(x))
    ydata.append(float(y))

x = np.asarray(xdata)
y = np.asarray(ydata)

n = len(x)
x0 = x[np.argmax(y)]
a = max(y)
sigma = np.sqrt(sum(y * (x - x0) * (x - x0)) / sum(y))

def gaus(x, x0, a, sigma):
  t = (x - x0) / sigma
  return a * np.exp(-t * t / 2)

popt, pcov = curve_fit(gaus, x, y, p0=[x0, a, sigma])

print('x0:', popt[0])
print('a:', popt[1])
print('sigma:', popt[2])
print('pcov:')
print(pcov)
xmax = max(x)
print(f'erfc({xmax})={erfc((xmax - popt[0]) / popt[2])}')
print()

f, ax = plt.subplots()
plt.plot(x, y, 'b+:', label='data')
plt.plot(x, gaus(x, *popt), 'ro:', label='fit')
plt.legend()
plt.title(sys.argv[1])
plt.xlabel('Sample value')
plt.ylabel('# samples')
text = f'$x_0$={popt[0]:.2f}\n$a$={popt[1]:.4g}\n$\\sigma$={popt[2]:.2f}'
plt.text(0.05, 0.75, text, transform=ax.transAxes, fontsize=14)
plt.show()
