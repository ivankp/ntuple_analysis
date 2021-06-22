#!/usr/bin/env python3

import sys, os, sqlite3, json, re, argparse, time
from subprocess import Popen, PIPE
from collections import defaultdict
from itertools import product

argparser = argparse.ArgumentParser(
    formatter_class=argparse.ArgumentDefaultsHelpFormatter
)
argparser.add_argument('-m',action='store_true',
    help='add "+IsMediumJob = True" to condor jobs')
argparser.add_argument('-n', type=int, default=int(25e6),
    help='maximum number of events per job')
argparser.add_argument('-r', type=float, default=0.4,
    help='jet radius')
argparser.add_argument('-t', type=str, default=f'{int(time.time()*1000)}',
    help='set run tag')
argparser.add_argument('-x',action='store_true',
    help='generate job scripts but don\'t submit')
args = argparser.parse_args()
print(args)

def mkdirs(*ds):
    for d in ds:
        if not os.path.isdir(d):
            os.makedirs(d)

exe = '../../bin/hist'

selection = [{
    'part': ['B','RS','I','V'],
    'njets': [1],
    'particle': ['H'],
    'energy': [13],
    'info': ['GGFHT pt25.0 eta4.5']
# },{
#     'part': ['B','RS','I','V'],
#     'njets': [2,3],
#     'particle': ['H'],
#     'energy': [13],
#     'info': ['ED GGFHT pt25.0 eta4.5']
}]

db = sqlite3.connect('../sql/ntuples.db')

LD_LIBRARY_PATH = os.environ['LD_LIBRARY_PATH']

subcount = defaultdict(lambda:0)

re_diag = re.compile(r' (tr|bx|pn)(?: |$)')
def check_diag():
    for s in selection:
        infos = s.get('info')
        if infos is None: continue
        for info in infos:
            if not (re_diag.search(info) is None):
                return True
    return False
do_diag = check_diag()
def diagram(info):
    m = re_diag.search(info)
    return 'all' if m is None else m.group(1)

def check_mtop():
    for s in selection:
        infos = s.get('info')
        if infos is None: continue
        for info in infos:
            if 'mtop' in info:
                return True
    return False
do_mtop = check_mtop()

def make_chunks(names,vals):
    print(dict(zip(names,vals)))
    fs = [ ( x[-1], x[0]+'/'+x[1], x[3], x[5],
        f'{x[2]}{x[3]}j{x[4]}_{x[5]:g}TeV' \
        + ('_'+('mtop' if ('mtop' in x[6]) else 'eft') if do_mtop else '') \
        + ('_'+diagram(x[6]) if do_diag else '') \
        + f'_antikt{args.r*10:g}'
    ) for x in db.execute('''
SELECT dir,file,particle,njets,part,energy,info,nentries
FROM ntuples
WHERE
'''+' and '.join(a+'=?' for a in names),vals).fetchall() ]

    if len(fs)==0: return [ ]

    pref = fs[0][-1]
    for i in range(1,len(fs)):
        if pref != fs[i][-1]:
            raise Exception(f'Incompatible selections:\n{pref}\n{fs[i][-1]}')

    chunks = [ ]
    n = 0
    for f in fs:
        if n == 0:
            subcount[pref] += 1
            chunks.append(( f'{pref}_{subcount[pref]:0>3d}', [], f[3], f[2] ))
        chunks[-1][1].append(f[1])
        n += f[0]
        if n >= args.n:
            n = 0

    return chunks

args.t = '_'+re.sub(r'\s+','-',args.t)
condor_dir = 'condor'+args.t
out_dir = 'out'+args.t
merged_dir = 'merged'+args.t
mkdirs(condor_dir,out_dir)
os.chdir(condor_dir)

def make_job(chunk):
    script = chunk[0]+'.sh'
    with open(script,'w') as f:
        f.write(f'''\
#!/bin/bash
export LD_LIBRARY_PATH={LD_LIBRARY_PATH}\n
{exe} - << CARD
''' + json.dumps({
        'input': { 'files': chunk[1] },
        'rootS': chunk[2],
        'jets': {
            'cuts': { 'pt': 30, 'eta': 4.4 },
            'algorithm': [ 'antikt', args.r ],
            'njets_min': chunk[3]
        },
        'binning': '../binning.json',
        'output': f'../{out_dir}/{chunk[0]}.root',
        'reweighting': [{
            'pdf': 'CT14nlo',
            'pdf_var': True,
            'ren_fac': [
                [1,1], [0.5,0.5], [1,0.5], [0.5,1], [2,1], [1,2], [2,2]
            ],
            'scale': 'HT2'
        }]
    }, indent=2, separators=(',',': ')) + '\nCARD\n')

    os.chmod(script,0o775)

with open('finish.sh','w') as f:
    f.write(f'''\
#!/bin/bash
export LD_LIBRARY_PATH={LD_LIBRARY_PATH}
cd ..
[ -x 'merge.sh' ] && ./merge.sh {out_dir} {merged_dir}
[ -x 'finish.sh' ] && ./finish.sh
''')
os.chmod('finish.sh',0o775)

with open('job.sub','w') as f:
    f.write('''\
Universe   = vanilla
Executable = $(name).sh
Output     = $(name).out
Error      = $(name).err
Log        = $(name).log
getenv = True
''' + ('+IsMediumJob = True\n' if args.m else '') +
'''queue
''')

with open('jobs.dag','w') as f:
    n = 0
    for s in selection:
        keys = s.keys()
        for vals in product(*s.values()):
            for chunk in make_chunks(keys,vals):
                print(chunk[0])
                make_job(chunk)
                f.write(f'JOB j{n} job.sub\nVARS j{n} name="{chunk[0]}"\n\n')
                n += 1
    f.write('''\
JOB finish job.sub
VARS finish name="finish"\n
PARENT '''+(' '.join(f'j{i}' for i in range(n)))+' CHILD finish\n')

if not args.x:
    Popen(f'rm -fv {condor_dir}/jobs.dag*',shell=True).communicate()
    Popen(('condor_submit_dag','jobs.dag')).communicate()

