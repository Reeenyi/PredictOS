import matplotlib.pyplot as plt
import numpy as np

with open("preemptSwtTimeLog.txt", "r") as f:
    text = f.read()
preemptTime = [int(x.strip()) for x in text.split(",") if x.strip()]
while preemptTime and preemptTime[-1] == 0:
    preemptTime.pop()

with open("yieldSwtTimeLog.txt", "r") as f:
    text = f.read()
yieldTime = [int(x.strip()) for x in text.split(",") if x.strip()]
while yieldTime and yieldTime[-1] == 0:
    yieldTime.pop()

allPreemptiveTime = preemptTime + yieldTime

with open("fullNonpreemSwtTimeLog.txt", "r") as f:
    text = f.read()
nonPreemTime = [int(x.strip()) for x in text.split(",") if x.strip()]
while nonPreemTime and nonPreemTime[-1] == 0:
    nonPreemTime.pop()

def calc_statistics(data):
    return np.mean(data), np.median(data), np.min(data), np.max(data)

for data in [nonPreemTime, allPreemptiveTime, preemptTime, yieldTime]:
    print(calc_statistics(data))

plt.grid(axis='y', linestyle='--', alpha=0.7)

plt.boxplot([nonPreemTime, allPreemptiveTime, preemptTime, yieldTime], 
            labels=["Non-preemptive", "Limited-preemptive\nOverall", "Limited-preemptive\nPreemption path", "Limited-preemptive\nCPU yield path"], 
            showfliers=False)

plt.ylim(0, 400)
plt.ylabel("Task switching time (cycles)")

plt.show()