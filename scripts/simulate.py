#!/usr/bin/python

import os
import sys
import math
import subprocess

def dna_path():
    return 'DNA_Simulation/'

def rna_path():
    return 'RNA_Simulation/'

def seq_path(base):
    return base + 'seqs/'

def read_path(base):
    return base + 'reads/'

def r_sequins():
    return '../data/rna/RNA.tab.fa'

def d_sequins():
    return '../data/dna/DNA.tab.fa'

def r_mixtures():
    return '../data/rna/RNA_Standards_Analysis.txt'

def d_mixtures():
    return '../data/dna/DNA_Standards_Analysis.txt'

# Split a file of sequin into individual sequins
def split_sequins(file, seq_path):
    os.system('mkdir -p ' + seq_path)

    with open(file) as f:
        while True:
            l1 = f.readline()
            l2 = f.readline()            
            if (not l2):
                break

            file = l1.replace(">","")
            file = file.replace("?","")
            file = file.replace("\n","")

            w = open(seq_path + file + '.fa', 'w')
            w.write(l1)
            w.write(l2)

def read_mixture(file, mix):
    r = {}

    with open(file) as f:
        l = f.readline()

        while True:
            l = f.readline()
            if (not l):
                break

            tokens = l.strip().split('\t')

            #
            # ID  DNA_ID  subgroup  mix-A  mix-B  expected-fold-change  log2(Mix 1/Mix 2)
            #

            # Create data-structure for the sequin            
            r[tokens[1]] = { 'id':     tokens[1],
                             'group':  tokens[2],
                             'con_a':  float(tokens[3]),
                             'con_b':  float(tokens[4]),
                             'fold' :  float(tokens[5]),
                             'l_fold': float(tokens[6])
                           }

    return r

# Generate simulated reads for each sequin for a given mixture
def simulate_reads(file, seq_path, read_path, min_, max_, mix):
    os.system('mkdir -p ' + read_path)
    d = read_mixture(file, mix)

    min_c = 1000000;

    # Let's calculate a constant such that all concentration will be at least 10
    for key in d:
        if (d[key]['con_a'] < min_c):
            min_c = d[key]['con_a'];
        if (d[key]['con_b'] < min_c):
            min_c = d[key]['con_b'];
    
    print 'Minimum concentration: ' + str(min_c)

    for f in os.listdir(seq_path):
        key = f.split('.')[0]

        if key in d:
            if mix == 'A':
                con = d[key]['con_a']
            else:
                con = d[key]['con_b']
                
            # This can be changed to simulate sequencing depth and coverage...

            s = 1
            c = 0

            con = c + (s * con)
            con = max(min_, con)
            con = min(max_, con)
            
            print '\n------------------ ' + key + ' ------------------'
            
            # Command: wgsim -d 400 -N 5151 -1 101 -2 101 ${X} ${X}.R1.fastq ${X}.R2.fastq
            
            i  = seq_path  + key + '.fa'
            o1 = read_path + key + '.R1.fastq'
            o2 = read_path + key + '.R2.fastq'

            # Don't bother if the abundance is too low
            if (con > 1):
                print 'Generating: ' + str(con)

                # Simulate reads from a given sequin
                cmd = 'wgsim -d 400 -N ' + str(int(con)) + ' -1 101 -2 101 ' + i + ' ' + o1 + ' ' + o2

                print cmd
                os.system(cmd)
        else:
            raise key + ' not found!'

    print('Merging the individual simulations...')
    os.system('cat ' + read_path + '*R1.fastq > ' + read_path + 'simulated_1.fastq')
    os.system('cat ' + read_path + '*R2.fastq > ' + read_path + 'simulated_2.fastq')

def print_usage():
    print 'Usage: python simulate.py RNA_A|RNA_B|DNA_A|DNA_B'

if __name__ == '__main__':
    if (len(sys.argv) != 2):
        print_usage()
    elif (sys.argv[1] == 'RNA_A'):
        split_sequins(r_sequins(), seq_path(rna_path()))
        simulate_reads(r_mixtures(), seq_path(rna_path()), read_path(rna_path()), 0, sys.maxint, 'A')
    elif (sys.argv[1] == 'RNA_B'):
        split_sequins(r_sequins(), seq_path(rna_path()))
        simulate_reads(r_mixtures(), seq_path(rna_path()), read_path(rna_path()), 0, sys.maxint, 'B')
    elif (sys.argv[1] == 'DNA_A'):
        split_sequins(d_sequins(), seq_path(dna_path()))
        simulate_reads(d_mixtures(), seq_path(dna_path()), read_path(dna_path()), 0, sys.maxint, 'A')
    elif (sys.argv[1] == 'DNA_B'):
        split_sequins(d_sequins(), seq_path(dna_path()))
        simulate_reads(d_mixtures(), seq_path(dna_path()), read_path(dna_path()), 0, sys.maxint, 'B')
    else:
        print_usage()