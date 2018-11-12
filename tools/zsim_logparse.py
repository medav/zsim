
import re

core_regex = re.compile(r'c-(\d+)')

def MaxCoreCycles(outlog):
<<<<<<< HEAD
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
=======
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
>>>>>>> 98e75acb344a7db9dec7c3777bbe22cd94079d32

def AvgContCycles(outlog):
    return 0