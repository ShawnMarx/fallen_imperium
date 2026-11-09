#!/usr/bin/env python3
"""Breakthrough statistics from simulator output (0.44.4).
usage: bt_stats.py REPO DIR [-v]
DIR holds factions.txt (unused), err*.txt (stderr of a run with TI4_ABSTATS=1) and log*.txt (TI4_SIMLOG=<file>).
Example (every faction with its breakthrough from the start - Rapid Mobilization):
  TI4_SIMFACTIONS=<all breakthrough factions> TI4_SIMMODULES=13 TI4_SIMEVENTS=65536 TI4_ABSTATS=1 \
  TI4_SIMLOG=DIR/log1.txt ./build/host/ti4 --simulate 66 6 1000 > DIR/out1.txt 2> DIR/err1.txt
Columns: gains, log lines naming the card, questions asked (Tag::Faction with its Ab), yes / yes-no questions,
component action offered / used."""
import re, sys, os, glob, collections

repo, d = sys.argv[1], sys.argv[2]
src = open(os.path.join(repo, 'src/engine/decision.h')).read()
body = src[src.index('enum class Ab'):]
body = body[body.index('{') + 1: body.index('};')]
body = re.sub(r'//[^\n]*', '', body)
names = [x.strip() for x in body.split(',') if x.strip()]
ab = {}
for i, n in enumerate(names):
    n = n.split('=')[0].strip()
    ab[i] = n

# breakthrough table: faction key, name
bt = re.findall(r'^\s*\{"([a-z0-9]+)", "((?:[^"\\]|\\.)*)"', open(os.path.join(repo, 'src/engine/breakthroughs.cpp')).read(), re.M)
bt = [(k, n.replace('\\"', '"')) for k, n in bt]

# which Ab codes belong to which breakthrough (by name)
abOf = collections.defaultdict(list)
special = {'bastion': 'BtBastion', 'ralnel': 'BtRalNel', 'deepwrought': 'BtDeepwrought', 'crimson': 'BtCrimson'}
for i, n in ab.items():
    m = re.match(r'Bt([A-Z][a-z]+)$', n)
    if m:
        abOf[m.group(1).lower()].append(i)
    m = re.match(r'Dsbt([A-Z][a-z]+)$', n)
    if m:
        abOf[m.group(1).lower()].append(i)
alias = {'freesys': 'freesystems', 'myko': 'mykomentori', 'ralnel': 'ralnel'}
for k in list(abOf):
    if k in alias:
        abOf[alias[k]] += abOf.pop(k)

stats = collections.defaultdict(lambda: collections.Counter())
for f in glob.glob(os.path.join(d, 'err*.txt')):
    for line in open(f, errors='replace'):
        m = re.match(r'ABSTATS (\w+) (-?\d+) (\d+)', line)
        if m:
            stats[m.group(1)][int(m.group(2))] += int(m.group(3))

# log mentions
fires = collections.Counter()
gains = collections.Counter()
games = 0
lines_of = collections.defaultdict(list)
for f in sorted(glob.glob(os.path.join(d, 'log*.txt'))):
    for line in open(f, errors='replace'):
        if line.startswith('=== game'):
            games += 1
            continue
        for k, n in bt:
            if n in line:
                if 'gains their breakthrough' in line:
                    gains[k] += 1
                else:
                    fires[k] += 1
                    if len(lines_of[k]) < 4:
                        lines_of[k].append(line.strip()[:200])

print(f'games {games}')
print(f'{"faction":14s} {"breakthrough":28s} gain  fire  asked yes/yn  offered used')
for k, n in bt:
    a = abOf.get(k, [])
    asked = sum(stats['asked'][i] for i in a)
    yes = sum(stats['yes'][i] for i in a)
    yn = sum(stats['yesno'][i] for i in a)
    off = sum(stats['offered'][i] for i in a)
    used = sum(stats['used'][i] for i in a)
    print(f'{k:14s} {n[:28]:28s} {gains[k]:4d} {fires[k]:5d} {asked:6d} {yes:3d}/{yn:<4d} {off:7d} {used:4d}')
if '-v' in sys.argv:
    for k, n in bt:
        print('---', k, n)
        for l in lines_of[k]:
            print('   ', l)
