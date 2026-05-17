#!/usr/bin/env python3

# execute `dump binary memory mem.bin LOG_BUF_START_ADDR LOG_BUF_END_ADDR` in the debugger console
# and copy output file `mem.bin` to this directory

import sys
import struct
from collections import defaultdict
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

if len(sys.argv) < 2:
    raise SystemExit("usage: python vis.py mem.bin")

rawBytes = open(sys.argv[1], "rb").read()
words = [w for (w,) in struct.iter_unpack("<I", rawBytes[: len(rawBytes) // 4 * 4])]
logRecords = list(zip(words[::2], words[1::2]))

if not logRecords:
    raise SystemExit("empty log")

# find wrap index in circular log buffer
wrapIndex = max(
    (i for i in range(1, len(logRecords)) if logRecords[i][0] < logRecords[i - 1][0]),
    key=lambda i: logRecords[i - 1][0] - logRecords[i][0],
    default=0,
)
logRecords = logRecords[wrapIndex:] + logRecords[:wrapIndex]

# decode events
# log format: systime(32 bits) - currIterTime(16 bits) - priority(8 bits) - event(8 bits)
decodedEvents = []
overflowCount = 0
prevRawTimestamp = logRecords[0][0]

for rawTimestamp, eventWord in logRecords:
    if rawTimestamp < prevRawTimestamp:
        overflowCount += 1
    timestamp    = rawTimestamp + overflowCount * (1 << 32)
    currIterTime = (eventWord >> 16) & 0xFFFF
    priority     = (eventWord >>  8) & 0xFF
    eventType    =  eventWord        & 0xFF
    decodedEvents.append((timestamp, currIterTime, priority, eventType))
    prevRawTimestamp = rawTimestamp

# match intervals
openIntervals = defaultdict(list)
segments      = defaultdict(list)
releaseTimes  = defaultdict(set)

for timestamp, currIterTime, priority, eventType in decodedEvents:
    releaseTimes[priority].add(timestamp - currIterTime)
    phase      = eventType >> 1
    isEndEvent = eventType & 1
    key        = (priority, phase)
    if not isEndEvent:
        openIntervals[key].append(timestamp)
    elif openIntervals[key]:
        startTs = openIntervals[key].pop()
        segments[priority].append((startTs, timestamp, phase))

# draw graph
priorityList = sorted(segments)
priorityToY  = {p: i for i, p in enumerate(priorityList)}
phaseColors  = ["C0", "C1", "C2"]  # read / execute / write

timeMin = min(s for p in priorityList for s, e, ph in segments[p])
timeMax = max(e for p in priorityList for s, e, ph in segments[p])

fig, ax = plt.subplots()

# draw segments
for priority in priorityList:
    yPos = priorityToY[priority]
    for startTs, endTs, phase in segments[priority]:
        ax.add_patch(Rectangle((startTs, yPos - 0.35), endTs - startTs, 0.7, color=phaseColors[phase], lw=0))

# draw release time after the segments so arrows are not covered by rectangles.
for priority in priorityList:
    yPos = priorityToY[priority]
    for releaseTs in sorted(releaseTimes[priority]):
        if timeMin <= releaseTs <= timeMax:
            ax.annotate('', xy=(releaseTs, yPos + 0.5), xytext=(releaseTs, yPos + 0.3), arrowprops=dict(arrowstyle='-|>', color='0.35', lw=1.5, mutation_scale=10), zorder=3, annotation_clip=False)

for phaseIndex, phaseName in enumerate(["read", "execute", "write"]):
    ax.plot([], [], color=phaseColors[phaseIndex], label=phaseName)

ax.set_yticks(range(len(priorityList)))
ax.set_yticklabels(priorityList)
ax.set_xlabel("system time")
ax.set_ylabel("priority")
ax.set_xlim(timeMin, timeMax)
ax.set_ylim(-0.6, len(priorityList) - 0.3)
ax.legend()

plt.tight_layout()
plt.show()
