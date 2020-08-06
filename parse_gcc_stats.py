#!/usr/bin/python3
import argparse
import os


def parse_stats(input_file):
    if not os.path.exists(input_file):
        print('Error: invalid input file!')
        exit(-1)

    with open(input_file, 'r') as inp_file:
        content = inp_file.read().splitlines()

    stats_start = False
    stats_result = {}
    for line in content:
        line = line.strip()
        if stats_start and " : " in line:
            name, num = line.split(" : ")
            name = name.split(" == ")[0]
            if name not in stats_result:
                stats_result[name] = 0
            stats_result[name] += int(num)
        if line.startswith("Parsed opt_stats stats"):
            stats_start = True

    for key in stats_result:
        print(key, ":", stats_result[key])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Parse gcc statistics')
    requiredNamed = parser.add_argument_group('required named arguments')
    requiredNamed.add_argument("-i", "--input-file", dest="input_file", type=str, required=True,
                               help="Input file")
    args = parser.parse_args()
    parse_stats(args.input_file)
