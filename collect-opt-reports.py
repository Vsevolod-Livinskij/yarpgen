#!/usr/bin/python3
###############################################################################
#
# Copyright (c) 2015-2017, Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
###############################################################################
"""
Script which collects optimization reports
"""
###############################################################################

import argparse
import logging
import multiprocessing
import os
import re
import sys


import common
import run_gen

yarpgen_num = 0
csmith_num = 1


pass_stat = [{}, {}]
stmt_stat = [{}, {}]

def process_pass_in_stderr(stderr, idx):
    stderr_str = str(stderr, "utf-8")
    for i in stderr_str.split(" "):
        if "-Rpass=" in i:
            pass_name_pattern = re.compile("\[\-Rpass=.*\]")
            pass_name_match = pass_name_pattern.findall(i)
            pass_name = pass_name_match[0].split("=")[-1][:-1]
            if pass_name not in pass_stat[idx]:
                pass_stat[idx][pass_name] = 1
            else:
                pass_stat[idx][pass_name] += 1

def process_stat_in_stderr(stderr, idx):
    stderr_str = str(stderr, "utf-8")
    stderr_list = stderr_str.split("\n")
    for i in range(len(stderr_list)):
        if stderr_list[i] == "*** Stmt/Expr Stats:":
            i += 1
            while not stderr_list[i].startswith("Total bytes"):
                stat = stderr_list[i].lstrip().split()
                stat_name = stat[1][:-1]
                stat_val = int(stat[0])
                if stat_name not in stmt_stat[idx]:
                    stmt_stat[idx][stat_name] = 0
                stmt_stat[idx][stat_name] += stat_val
                i += 1

def exec_iter (gen_str, comp_str, stat_str, num, iter_num):
    ret_code, stdout, stderr, is_time_expired, elapsed_time = \
        common.run_cmd(gen_str.split(), run_gen.yarpgen_timeout, iter_num)
    full_comp_str = comp_str + " -w -O3 -Rpass=./* -c"
    ret_code, stdout, stderr, is_time_expired, elapsed_time = \
        common.run_cmd(full_comp_str.split(), run_gen.compiler_timeout, iter_num)
    if (is_time_expired):
        return False
    process_pass_in_stderr(stderr, num)
    ret_code, stdout, stderr, is_time_expired, elapsed_time = \
        common.run_cmd(stat_str.split(), run_gen.compiler_timeout, iter_num)
    if (is_time_expired):
        return False
    process_stat_in_stderr(stderr, num)
    return True

def prepare_env_and_collect_opt_reports(num_seeds, num_jobs, out_dir):
    common.check_dir_and_create(out_dir)

    # Check for binary of generator
    yarpgen_bin = os.path.abspath(common.yarpgen_home + os.sep + "yarpgen")
    common.check_and_copy(yarpgen_bin, out_dir)
    ret_code, output, err_output, time_expired, elapsed_time = common.run_cmd([yarpgen_bin, "-v"],
                                                                              run_gen.yarpgen_timeout, 0)
    common.yarpgen_version_str = str(output, "utf-8")
    # TODO: need to add some check, but I hope that it is safe
    common.log_msg(logging.DEBUG, "YARPGEN version: " + common.yarpgen_version_str)

    csmith_bin = "csmith"
    if not common.if_exec_exist(csmith_bin):
        common.print_and_exit("Can't find csmith binary")
    ret_code, output, err_output, time_expired, elapsed_time = common.run_cmd([csmith_bin, "-v"],
                                                                              run_gen.yarpgen_timeout, 0)
    csmith_version_str = str(output, "utf-8")
    # TODO: need to add some check, but I hope that it is safe
    common.log_msg(logging.DEBUG, "Csmith version: " + csmith_version_str)

    os.chdir(out_dir)

    for i in range(num_seeds):
        if not exec_iter(".." + os.sep + "yarpgen -q -std=c++11",
                         "clang++ -std=c++11 func.cpp",
                         "clang++ -cc1 -print-stats -std=c++11 -O3 -w func.cpp",
                         yarpgen_num, i):
            continue
        if not exec_iter("csmith -o func.c",
                         "clang func.c -I/home/vsevolod/workspace/csmith/runtime/ "
                                      "-I/home/vsevolod/workspace/csmith/build/runtime/",
                         "clang -cc1 -print-stats -O3 -w "
                                      "-I/home/vsevolod/workspace//csmith/runtime/ "
                                      "-I/home/vsevolod/workspace//csmith/build/runtime/ func.c",
                         csmith_num, i):
            continue

        with open("res.txt", "w") as out_file:
            for j in pass_stat:
                out_file.write("======================\n")
                for k in j:
                    out_file.write(str(k) + " = " + str(j[k]) + "\n")
            out_file.write("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
            for j in stmt_stat:
                out_file.write("======================\n")
                for k in j:
                    out_file.write(str(k) + " = " + str(j[k]) + "\n")

###############################################################################

if __name__ == '__main__':
    if os.environ.get("YARPGEN_HOME") is None:
        sys.stderr.write("\nWarning: please set YARPGEN_HOME envirnoment variable to point to test generator path, "
                         "using " + common.yarpgen_home + " for now\n")

    description = 'Script which collects opt-reports.'
    parser = argparse.ArgumentParser(description=description, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-n", dest="num_seeds", default=100, type=int,
                        help='Number of generated tests')
    parser.add_argument("-j", dest="num_jobs", default=multiprocessing.cpu_count(), type=int,
                        help='Maximum number of instances to run in parallel. By defaulti, it is set to'
                             ' number of processor in your system')
    parser.add_argument("-v", "--verbose", dest="verbose", default=False, action="store_true",
                        help="Increase output verbosity")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    common.setup_logger(None, log_level)

    common.check_python_version()
    prepare_env_and_collect_opt_reports(args.num_seeds, args.num_jobs, "out_dir")