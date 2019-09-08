#!/usr/bin/env python3
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

myFontSize = 15

no_miss_hit = np.array([0.1704, 0.1834, 0.1870, 0.1993, 0.2084])
with_miss_hit = np.array([0.1867, 0.2051, 0.2199, 0.2328, 0.2479])
data = np.stack((no_miss_hit, with_miss_hit), axis=1)

fig = plt.figure()
fig.set_size_inches(3.2, 3.2)
ax = fig.add_subplot(111)

ax.boxplot(data)
plt.xlabel("Cache Hit Ratio", fontsize=myFontSize)
plt.ylabel("Recovery Time (x1000ms)", fontsize=myFontSize)
plt.xticks(np.arange(3), (' ', '1.0', '0.5'))
plt.tight_layout()
plt.tick_params(labelsize=myFontSize-1)
plt.rcParams.update({'font.size': myFontSize})
fig.savefig('performance.pdf', format='pdf', dpi=1000)

plt.show()
