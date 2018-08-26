import sys
import os

import argparse
import matplotlib
import json
import numpy as np
import matplotlib.pyplot as plt
import mpl_toolkits.mplot3d.axes3d as axes3d
import math

n_insns = [1000, 10000, 100000, 1000000, 10000000]
avg_insn_overshoot = [27.850000, 26.900000, 28.049999, 26.350000, 27.900000]
std_insn_overshoot = [4.881344, 7.049114, 4.964633, 7.058861, 4.011238]

avg_time_window = [402.649994, 402.549988, 473.600006, 562.349976, 2492.350098]
std_time_window = [270.816791, 259.254791, 54.293042, 40.604803, 159.422395]
avg_time_nowindow =[185.649994, 188.899994, 193.199997, 375.049988, 2294.649902]
std_time_nowindow = [112.410086, 129.036786, 43.383870, 28.050735, 140.157055]

avg_time_multistepping = [675.299988,  4592.450195, 38614.550781, 371302.562500, 3505605.500000]
std_time_multistepping = [403.022254, 2690.142004, 1578.860348, 10592.338363, 59612.281151]

fig = plt.figure(dpi=100)
ax = fig.add_subplot(111)


fsize=20
markersize = 15.0

ax.errorbar(x =n_insns, y = avg_insn_overshoot, yerr=std_insn_overshoot, marker = 'o', fmt='o',markersize=markersize)
ax.set_xlabel("Number of progress instructions", fontsize=fsize)
#ax.set_xticks(n_insns)

ax.set_ylabel("Overshoot Error", fontsize=fsize)
ax.set_title("Instruction overshoot error", fontsize=fsize)
ax.set_xscale('log')
ax.set_ylim([0,50])
ax.set_xlim([100,100000000])
plt.show()

fig = plt.figure(dpi=100)
ax = fig.add_subplot(111)

fsize=20
markersize = 15.0

ax.errorbar(x =n_insns, y = np.array(avg_time_multistepping)/1000.0, yerr=np.array(std_time_multistepping)/1000.0, marker = '^', label = "multi stepping",fmt='o',markersize=markersize)
ax.errorbar(x =n_insns, y = np.array(avg_time_window)/1000.0, yerr=np.array(std_time_window)/1000.0, marker = 'o', label = "windowed multi stepping",fmt='o',markersize=markersize)
ax.errorbar(x =n_insns, y = np.array(avg_time_nowindow)/1000.0, yerr=np.array(std_time_nowindow)/1000.0, marker = '*', label = "baseline", fmt='o', markersize=markersize)
ax.set_xlabel("Number of progress instructions", fontsize=fsize)
ax.set_ylabel("Elapsed Time (ms)", fontsize=fsize)
ax.set_title("Measure of Overhead", fontsize=fsize)
ax.set_xscale('log')
ax.set_yscale('log')
ax.legend(ncol=4, loc=9, fontsize=fsize, bbox_to_anchor=(0, 1.02, 1, .102))
ax.set_xlim([100,100000000])
plt.show()