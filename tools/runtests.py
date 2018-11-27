import os
import sys
import subprocess
import itertools
from zsim_logparse import *

def GenerateConfig(template, param_names, param_list):
    for i in range(len(param_names)):
        template = template.replace('{' + param_names[i] + '}', str(param_list[i]))

    return template

template_filename = sys.argv[2]
outdir = sys.argv[1]
template = open(template_filename, 'r').read()

params = {
    'num_cores': [1, 2, 4, 8, 16, 32, 64, 96, 128, 196, 256],
    'l1i_size': [2 * 1024], # 32KB
    'l1d_size': [2 * 1024], # 64 KB
    'l2_size': [4 * 1024], # 512 KB
    'l3_size': [8 * 1024], # 1 MB
    'peak_bw': [4],
    'mat_size': [64] # 16 M entries => 64MB matrix
}


param_names = ['num_cores', 'l1i_size', 'l1d_size', 'l2_size', 'l3_size', 'peak_bw', 'mat_size']
param_lists = list(itertools.product(*[params[key] for key in param_names]))

stats = {
    'max_core_cycles': MaxCoreCycles,
    # 'avg_cont_cycles': AvgContCycles
}

stat_names = ['max_core_cycles']

if not os.path.exists(outdir):
    os.mkdir(outdir)

procs = []

for param_list in param_lists:
    uname = 'test_' + '_'.join([str(param) for param in param_list])
    workdir = outdir + '/' + uname
    os.mkdir(workdir)

    config_text = GenerateConfig(template, param_names, param_list)
    with open(workdir + '/config.cfg', 'w') as f:
        f.write(config_text)

    procs.append(subprocess.Popen(['zsim', 'config.cfg'], cwd=workdir, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
    print(uname)
    procs[-1].wait()

print(', '.join(param_names + stat_names))
for param_list in param_lists:
    uname = 'test_' + '_'.join([str(param) for param in param_list])
    workdir = outdir + '/' + uname
    logout = workdir + '/zsim.out'

    out_data = list(param_list) + [
        stats[stat_name](logout)
        for stat_name in stat_names
    ]

    print(', '.join([str(data) for data in out_data]))
