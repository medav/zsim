import os
import sys
import subprocess
import itertools
from multiprocessing.dummy import Pool as ThreadPool
from zsim_logparse import *
import shutil

def GenerateConfig(template, param_names, param_list):
    for i in range(len(param_names)):
        template = template.replace('{' + param_names[i] + '}', str(param_list[i]))

    return template


template_filename = sys.argv[2]
outdir = sys.argv[1]
template = open(template_filename, 'r').read()

params = {
    'num_cores': [1, 2, 4, 8, 16, 32, 64, 96, 128, 196, 256],
    'l1i_size': [16 * 1024], # 32KB
    'l1d_size': [16 * 1024], # 64 KB
    'l2_size': [512 * 1024], # 512 KB
    'l3_size': [1024 * 1024], # 1 MB
    'peak_bw': [1024],
    'mat_size': [64] # 16 M entries => 64MB matrix
}

min_cores = min(params['num_cores'])

param_names = ['num_cores', 'l1i_size', 'l1d_size', 'l2_size', 'l3_size', 'peak_bw', 'mat_size']
param_skip = {'l1i_size', 'l1d_size', 'l2_size', 'l3_size', 'mat_size'}
param_lists = list(itertools.product(*[params[key] for key in param_names]))

stats = {
    'max_core_cycles': MaxCoreCycles,
    'read_count': MemRdCount,
    # 'avg_cont_cycles': AvgContCycles
}

stat_names = ['max_core_cycles', 'read_count']

def RunTest(params):
    (param_list, workdir) = params
    proc = subprocess.Popen(['zsim', 'config.cfg'], cwd=workdir, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    proc.wait()

    logout = workdir + '/zsim.out'
    param_list = list(param_list)
    out_params = []

    for i in range(len(param_list)):
        if param_names[i] not in param_skip:
            out_params.append(param_list[i])

    out_data = out_params + [
        stats[stat_name](logout)
        for stat_name in stat_names
    ]

    return out_data

if not os.path.exists(outdir):
    os.mkdir(outdir)

params = []


out_param_names = []
for i in range(len(param_names)):
    if param_names[i] not in param_skip:
        out_param_names.append(param_names[i])

print(', '.join(out_param_names + stat_names + ['speedup']))

tp = ThreadPool(8)

for param_list in param_lists:
    uname = 'test_' + '_'.join([str(param) for param in param_list])
    workdir = outdir + '/' + uname
    os.mkdir(workdir)

    config_text = GenerateConfig(template, param_names, param_list)
    with open(workdir + '/config.cfg', 'w') as f:
        f.write(config_text)

    params.append((param_list, workdir))

results = tp.map(RunTest, params)
ref_cycle_count = 1

for result in results:
    if result[0] == min_cores:
        ref_cycle_count = result[-2]

for result in results:
    result += [ref_cycle_count / result[-2]]
    print(','.join([str(r) for r in result]))

shutil.rmtree(outdir)