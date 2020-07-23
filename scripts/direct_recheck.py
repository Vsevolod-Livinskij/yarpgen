#!/usr/bin/python3

import argparse
import logging
import multiprocessing
import os
import queue
import sys

import common

targets = ["clang_icx_no_opt",
           "clang_icx_opt",
           "clang_no_opt",
           "clang_opt",
           "clang_skx_no_opt",
           "clang_skx_opt",
           "clang_tgl_no_opt",
           "clang_tgl_opt",
           "gcc_icx_no_opt",
           "gcc_icx_opt",
           "gcc_no_opt",
           "gcc_opt",
           "gcc_skx_no_opt",
           "gcc_skx_opt",
           "gcc_tgl_no_opt",
           "gcc_tgl_opt",
           "ubsan_clang_o0"]


def process_dir(directory, task_queue):
    common.log_msg(logging.DEBUG, "Searching for test directories in " + str(directory))
    for root, dirs, files in os.walk(directory):
        for name in dirs:
            if name.startswith("S_"):
                common.log_msg(logging.DEBUG, "Adding " + str(os.path.join(root, name)))
                task_queue.put(os.path.join(root, name))
            else:
                process_dir(os.path.join(root, name), task_queue)
    return task_queue

def start_recheck(inp_dir, out_dir, num_jobs):
    if not common.check_if_dir_exists(inp_dir):
        common.print_and_exit("Can't use input directory")
    common.check_dir_and_create(out_dir)

    lock = multiprocessing.Lock()

    task_queue = multiprocessing.JoinableQueue()
    process_dir(inp_dir, task_queue)
#    failed_queue = multiprocessing.SimpleQueue()
#    passed_queue = multiprocessing.SimpleQueue()

    task_threads = [0] * num_jobs
    for num in range(num_jobs):
        task_threads[num] = \
            multiprocessing.Process(target=recheck,
                                    args=(num, lock, task_queue, None, None, out_dir))
        task_threads[num].start()

    task_queue.join()
    task_queue.close()

    for num in range(num_jobs):
        task_threads[num].join()

def recheck(num, lock, task_queue, failed_queue, passed_queue, out_dir):
    common.log_msg(logging.DEBUG, "Started recheck. Process #" + str(num))
    root_dir = os.getcwd()
    abs_out_dir = os.path.join(root_dir, out_dir)
    job_finished = False
    while not job_finished:
        try:
            test_dir = task_queue.get_nowait()
            task_queue.task_done()
            common.log_msg(logging.DEBUG, "#" + str(num) + " test directory: " + str(test_dir))
            abs_test_dir = os.path.join(root_dir, test_dir)
            os.chdir(abs_test_dir)

            out_res = set()
            for target in targets:
                common.log_msg(logging.DEBUG, "Re-checking target " + target)
                ret_code, output, err_output, time_expired, elapsed_time = \
                    common.run_cmd(["make", "-f", "Test_Makefile", target],
                                   60, num)
                if time_expired or ret_code != 0:
                    #failed_queue.put(test_dir)
                    common.log_msg(logging.DEBUG, "#" + str(num) + " Compilation failed")
                    common.copy_test_to_out(abs_test_dir, os.path.join(abs_out_dir, test_dir), lock)
                    break

                ret_code, output, err_output, time_expired, elapsed_time = \
                    common.run_cmd(["make", "-f", "Test_Makefile", "run_" + target],
                                   60, num)
                if time_expired or ret_code != 0:
                    #failed_queue.put(test_dir)
                    common.log_msg(logging.DEBUG, "#" + str(num) + " Execution failed")
                    common.copy_test_to_out(abs_test_dir, os.path.join(abs_out_dir, test_dir), lock)
                    break

                out_res.add(output)
                if len(out_res) > 1:
                    #failed_queue.put(test_dir)
                    common.log_msg(logging.DEBUG, "#" + str(num) + " Out differs")
                    common.copy_test_to_out(abs_test_dir, os.path.join(abs_out_dir, test_dir), lock)
                    break

            #passed_queue.put(test_dir)
            os.chdir(root_dir)

        except queue.Empty:
            job_finished = True


if __name__ == '__main__':
    if os.environ.get("YARPGEN_HOME") is None:
        sys.stderr.write("\nWarning: please set YARPGEN_HOME environment variable to point to yarpgen's directory,"
                         " using " + common.yarpgen_home + " for now\n")
    description = 'Script for rechecking of compiler errors'
    parser = argparse.ArgumentParser(description=description, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    requiredNamed = parser.add_argument_group('required named arguments')
    requiredNamed.add_argument("-i", "--input-dir", dest="inp_dir", type=str, required=True,
                               help="Input directory for re-checking")
    parser.add_argument("-o", "--output-dir", dest="out_dir", default="rechecked", type=str,
                        help="Output directory with relevant fails")
    parser.add_argument("-j", dest="num_jobs", default=multiprocessing.cpu_count(), type=int,
                        help='Maximum number of instances to run in parallel. By default, '
                             'it is set to number of processor in your system')
    parser.add_argument("-v", "--verbose", dest="verbose", default=False, action="store_true",
                        help="Increase output verbosity")
    parser.add_argument("--log-file", dest="log_file", type=str,
                        help="Logfile")
    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    common.setup_logger(args.log_file, log_level)

    start_recheck(args.inp_dir, args.out_dir, args.num_jobs)
