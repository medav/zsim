
import re

core_regex = re.compile(r'c-(\d+)')

def MaxCoreCycles(outlog):
    try:
        cur_core = -1
        cycle_counts = []

        with open(outlog, 'r') as f:
            for line in f:
                if (not line.startswith('   ')) and (cur_core != -1):
                    cur_core = -1

                if line.startswith('  '):
                    m = core_regex.search(line)
                    if m is not None:
                        cur_core = int(m.group(1))

                if cur_core != -1:
                    if '   cycles' in line:
                        cycle_counts.append(int(line.split()[1]))

        return max(cycle_counts)
    except:
        return None

def MemRdCount(outlog):
    try:
        with open(outlog, 'r') as f:
            for line in f:
                if line.startswith('   rd:'):
                    return int(line.strip().split()[1])

    except:
        return None

def AvgContCycles(outlog):
    return 0
