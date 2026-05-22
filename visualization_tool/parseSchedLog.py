#!/usr/bin/env python3

# execute `dump binary memory mem.bin LOG_BUF_START_ADDR LOG_BUF_END_ADDR` in the debugger console
# and copy output file `mem.bin` to this directory

import sys
import struct
from collections import defaultdict
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Patch

if len(sys.argv) != 2 and len(sys.argv) != 3:
    raise SystemExit("usage: python parseSchedLog.py mem.bin [mem2.bin]")

plt.rcParams.update({"font.size": 12})

def parse_log(logFileName, sourceId):
    rawBytes = open(logFileName, "rb").read()
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
            segments[priority].append((startTs, timestamp, phase, sourceId))

    return segments, releaseTimes


if len(sys.argv) == 2:
    segments, releaseTimes = parse_log(sys.argv[1], 0)

elif len(sys.argv) == 3:
    segments1, releaseTimes1 = parse_log(sys.argv[1], 0)
    segments2, releaseTimes2 = parse_log(sys.argv[2], 1)

    segments = defaultdict(list)
    releaseTimes = defaultdict(set)

    # merge segments
    for d in (segments1, segments2):
        for k, v in d.items():
            segments[k].extend(v)

    # merge release times
    for d in (releaseTimes1, releaseTimes2):
        for k, v in d.items():
            releaseTimes[k].update(v)

priorityList = sorted(segments)
priorityToY  = {p: i for i, p in enumerate(priorityList)}

phaseColors = [
    ["C0", "C1", "C2"],
    ["C9", "C3", "C8"],
]

timeMin = min(s for p in priorityList for s, e, ph, src in segments[p])
timeMax = max(e for p in priorityList for s, e, ph, src in segments[p])

# timeMin = 1000
# timeMax = 1400

fig, ax = plt.subplots(figsize=(14, 4))

# draw segments
for priority in priorityList:
    yPos = priorityToY[priority]
    for startTs, endTs, phase, sourceId in segments[priority]:
        ax.add_patch(Rectangle((startTs, yPos - 0.25), endTs - startTs, 0.5, color=phaseColors[sourceId][phase], lw=0))

# draw release time
for priority in priorityList:
    yPos = priorityToY[priority]
    for releaseTs in sorted(releaseTimes[priority]):
        if timeMin <= releaseTs <= timeMax:
            ax.annotate('', xy=(releaseTs, yPos + 0.5), xytext=(releaseTs, yPos + 0.2), 
                        arrowprops=dict(arrowstyle='-|>', color='0.35', lw=1.5, mutation_scale=10),
                        zorder=3, annotation_clip=False)

if len(sys.argv) == 2:
    legendPatches = [
        Patch(color="C0", label="read"),
        Patch(color="C1", label="execute"),
        Patch(color="C2", label="write")
    ]
elif len(sys.argv) == 3:
    legendPatches = [
        Patch(color="C0", label="core1 read"),
        Patch(color="C1", label="core1 execute"),
        Patch(color="C2", label="core1 write"),
        Patch(color="C9", label="core2 read"),
        Patch(color="C3", label="core2 execute"),
        Patch(color="C8", label="core2 write")
    ]

ax.legend(handles=legendPatches, fontsize=10, handlelength=0.5, handleheight=0.5, loc="lower right")

ax.set_yticks(range(len(priorityList)))
ax.set_yticklabels(priorityList)

ax.set_xlabel("system time")
ax.set_ylabel("priority")

ax.set_xlim(timeMin, timeMax)
ax.set_ylim(-0.6, len(priorityList) - 0.3)

# plt.tight_layout()
plt.subplots_adjust(left=0.08, right=0.98, top=0.90, bottom=0.3)

plt.show()