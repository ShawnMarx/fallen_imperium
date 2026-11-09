#!/usr/bin/env python3
"""Technology statistics from simulator output (after 0.44.4).
usage: tech_stats.py REPO DIR [colour]   colour: Blue / Green / Red / Yellow (default Yellow)
DIR holds log*.txt (TI4_SIMLOG=<file>) and err*.txt (stderr of a run with TI4_ABSTATS=1: TECHSTATS lines).
Prints per technology of that colour: games played, times researched (and by which factions for generic ones),
average round researched, component action offered / used, log lines naming it (uses), and the "use it?" questions."""
import collections
import glob
import os
import re
import sys

repo, d = sys.argv[1], sys.argv[2]
colour = sys.argv[3] if len(sys.argv) > 3 else 'Yellow'
gd = open(os.path.join(repo, 'src/engine/gen/gamedata.inc'), errors='replace').read()
fac_block = gd[gd.index('const FactionDef kFactions'):]
factions = re.findall(r'^  \{"([a-z0-9]+)", "([^"]+)"', fac_block, re.M)
tech_block = gd[gd.index('TechDef kTechs['):]
techs = re.findall(r'^  \{"([^"]+)", "([^"]+)", TechColor::(\w+), (true|false), \{[^}]*\}, (-?\d+),', tech_block, re.M)
names = {k: n for k, n, c, u, f in techs}
mine = [(k, n, int(f)) for k, n, c, u, f in techs if c == colour and u == 'false']

researched = collections.Counter()
by = collections.defaultdict(collections.Counter)
rounds = collections.defaultdict(list)
mentions = collections.Counter()
games = 0
for f in sorted(glob.glob(os.path.join(d, 'log*.txt'))):
    rnd = 0
    for line in open(f, errors='replace'):
        if line.startswith('=== game'):
            games += 1
            rnd = 0
            continue
        m = re.match(r'--- Round (\d+) ---', line)
        if m:
            rnd = int(m.group(1))
            continue
        m = re.match(r'(.+?) researches (.+?)( \(.*\))?\.$', line.strip())
        if m:
            who, t = m.group(1), m.group(2)
            for k, n, fi in mine:
                if t == n:
                    researched[k] += 1
                    by[k][who] += 1
                    rounds[k].append(rnd)
            continue
        for k, n, fi in mine:
            if n in line and 'researches' not in line:
                mentions[k] += 1

offered, used = collections.Counter(), collections.Counter()
usetech = []
for f in glob.glob(os.path.join(d, 'err*.txt')):
    for line in open(f, errors='replace'):
        m = re.match(r'TECHSTATS (offered|used) (\S+) (\d+)', line)
        if m:
            (offered if m.group(1) == 'offered' else used)[m.group(2)] += int(m.group(3))
        m = re.match(r'TECHSTATS usetech (\d+) (\d+) (.*)', line)
        if m:
            usetech.append((int(m.group(1)), int(m.group(2)), m.group(3).strip()))

print(f'games {games} - {colour} technologies (not unit upgrades)')
print(f'{"key":20s} {"name":28s} {"owner":12s} res  avgR  offer used  logs  top researchers')
for k, n, fi in mine:
    owner = factions[fi][0] if 0 <= fi < len(factions) else '-'
    avg = sum(rounds[k]) / len(rounds[k]) if rounds[k] else 0
    top = ', '.join(f'{w} {c}' for w, c in by[k].most_common(4))
    print(f'{k:20s} {n[:28]:28s} {owner:12s} {researched[k]:4d} {avg:5.1f} {offered[k]:5d} {used[k]:4d} {mentions[k]:5d}  {top}')
print('\n"use it?" questions (asked, yes, prompt):')
agg = collections.defaultdict(lambda: [0, 0])
for a, y, p in usetech:
    agg[p][0] += a
    agg[p][1] += y
for p, (a, y) in sorted(agg.items(), key=lambda x: -x[1][0]):
    print(f'  {a:6d} {y:6d}  {p}')
