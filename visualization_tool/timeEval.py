import matplotlib.pyplot as plt

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

with open("fullNonpreemSwtTimeLog.txt", "r") as f:
    text = f.read()
nonPreemTime = [int(x.strip()) for x in text.split(",") if x.strip()]
while nonPreemTime and nonPreemTime[-1] == 0:
    nonPreemTime.pop()

plt.boxplot([preemptTime, yieldTime, nonPreemTime], labels=["Preemption path", "CPU yield path", "Fully non-preemptive"], showfliers=False)

plt.ylim(0, 500)
plt.ylabel("Context switch time (cycles)")

plt.show()