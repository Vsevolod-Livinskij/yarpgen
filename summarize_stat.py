#!/usr/bin/python3


import argparse
import collections
import logging
import math
import os
import sys

import common
import run_gen


class ProbStat:
    def __init__(self, name):
        self.name = name
        self.num = 0
        self.values = []

    def add(self, value):
        self.values.append(value)
        self.num += 1

    def dump(self):
        average = 0.0
        for i in self.values:
            average += i
        average /= self.num
        deviation = 0.0
        for i in self.values:
            deviation += (average - i)**2
        deviation /= self.num
        deviation = math.sqrt(deviation)
        print("{:30s}: {:1.5f} / {:1.5f}".format(self.name, average, deviation))


def start_summarization(iter_num):
        yarpgen_bin = os.path.abspath(common.yarpgen_home + os.sep + "yarpgen")
        if not common.if_exec_exist(yarpgen_bin):
            common.print_and_exit("Can't find yarpgen binary")
        stat = collections.OrderedDict()
        for i in range(iter_num):
            ret_code, output, err_output, time_expired, elapsed_time = \
                common.run_cmd([yarpgen_bin, "-d", "/tmp"], run_gen.yarpgen_timeout, 0)
            raw_prob_block = str(output, "utf-8").split("@@@@@@@@@@")
            for j in range(1, len(raw_prob_block)):
                raw_data = raw_prob_block[j].split("\n")
                if len(raw_data) != 6:
                    continue
                if raw_data[1] not in stat:
                    stat[raw_data[1]] = ProbStat(raw_data[1])
                value = float(raw_data[3].split(":")[-1])
                if not math.isnan(value):
                    stat[raw_data[1]].add(value)

        for i in stat:
            stat[i].dump()

if __name__ == '__main__':
    if os.environ.get("YARPGEN_HOME") is None:
        sys.stderr.write("\nWarning: please set YARPGEN_HOME envirnoment variable to point to test generator path, "
                         "using " + common.yarpgen_home + " for now\n")
    description = 'Script for summarization of generator probability statistics'
    parser = argparse.ArgumentParser(description=description, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-n", dest="iter_num", type=int, default=2,
                        help="Number of iterations")
    parser.add_argument("-v", "--verbose", dest="verbose", default=False, action="store_true",
                        help="Increase output verbosity")
    parser.add_argument("--log-file", dest="log_file", type=str,
                        help="Logfile")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    common.setup_logger(args.log_file, log_level)

    common.check_python_version()
    start_summarization(args.iter_num)
