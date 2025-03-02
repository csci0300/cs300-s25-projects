#!/usr/bin/env python3

import os
import sys
import json
import random
import argparse
import subprocess

HEADER = "\033[95m"
OKBLUE = "\033[94m"
OKCYAN = "\033[96m"
OKGREEN = "\033[92m"
WARNING = "\033[93m"
FAIL = "\033[91m"
ENDC = "\033[0m"
BOLD = "\033[1m"
UNDERLINE = "\033[4m"

MAX_LINES = 20

def log(msg):
    if '--grader' not in sys.argv:
        print(msg)


def pass_fail_str(rv):
    if rv == 0:
        return OKGREEN + "PASSED" + ENDC
    else:
        return FAIL    + "FAILED" + ENDC


def shell_return(shell_cmd, suppress=False, output_on_fail=True):
    if not suppress:
        log('-> ' + shell_cmd)
    proc = subprocess.run(
        shell_cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        shell=True
    )
    if output_on_fail:
        if proc.returncode != 0:
            output = proc.stdout.decode("utf-8").strip()
            print_test_result(proc.returncode, output)

    return proc.returncode


def print_test_result(rv, output):
    if len(output) != 0:
        output_lines = output.split("\n")
        if len(output_lines) > MAX_LINES:
            output_lines = output_lines[:MAX_LINES]
            output_lines.append("[output clipped]")
        output_fmt = "\t" + FAIL + "Test failed:\n\t" \
                + "\n\t".join(output_lines) + ENDC
        log(output_fmt)


def files_same_size(file1, file2, output_on_fail=True):
    size1 = os.path.getsize(file1)
    size2 = os.path.getsize(file2)

    if size1 != size2:
        if output_on_fail:
            output = f"File sizes differ:  expected {size1} ({file1}), got {size2} ({file2})\n"
            print_test_result(1, output)
        return False
    else:
        return True


def files_same(file1, file2, output_on_fail=True):
    same_size = files_same_size(file1, file2, output_on_fail=output_on_fail)
    if not same_size:
        return False

    same_contents = shell_return(f'diff {file1} {file2}',
                                 suppress=True, output_on_fail=output_on_fail) == 0

    return same_size and same_contents


def create_unit_test(functions, rand_seed, file_size=4096, max_file_size=8192, num_ops=8192):
    def unit_test(infile, outfile, outfile2):
        return shell_return(f'./io300_test {functions} --seed {rand_seed} -n {num_ops} --file-size {file_size} --max-size {max_file_size} ') == 0
    return unit_test

def byte_cat(infile, outfile, outfile2):
    return shell_return(f'./byte_cat {infile} {outfile}') == 0 \
        and files_same(infile, outfile)

def diabolical_byte_cat(infile, outfile, outfile2):
    return shell_return(f'./diabolical_byte_cat {infile} {outfile}') == 0 \
        and files_same(infile, outfile)

def reverse_byte_cat(infile, outfile, outfile2):
    return shell_return(f'./reverse_byte_cat {infile} {outfile}') == 0 \
        and shell_return(f'./reverse_byte_cat {outfile} {outfile2}') == 0 \
        and files_same(infile, outfile2)

def create_block_cat(block_size):
    def block_cat(infile, outfile, outfile2):
        return shell_return(f'./block_cat {block_size} {infile} {outfile}') == 0 \
            and files_same(infile, outfile)
    return block_cat

def create_reverse_block_cat(block_size):
    def reverse_block_cat(infile, outfile, outfile2):
        return shell_return(f'./reverse_block_cat {block_size} {infile} {outfile}') == 0 \
            and not files_same(infile, outfile, output_on_fail=False) \
            and shell_return(f'./reverse_block_cat {block_size} {outfile} {outfile2}') == 0 \
            and files_same(infile, outfile2)
    return reverse_block_cat

def random_block_cat(infile, outfile, outfile2):
    return shell_return(f'./random_block_cat {infile} {outfile}') == 0 \
        and files_same(infile, outfile)

def rot13(infile, outfile, outfile2):
    assert shell_return(f'cp {infile} {outfile}') == 0
    rotate1 = shell_return(f'./rot13 {outfile}') == 0
    if not rotate1:
        return False
    #import pdb; pdb.set_trace()
    files_differ = not files_same(infile, outfile, output_on_fail=False)
    if not files_differ:
        print_test_result(1, "File has not changed after one rotation")
        return False

    rotate2 = shell_return(f'./rot13 {outfile}') == 0
    same_after_rotate_twice = files_same(infile, outfile)

    return rotate2 and same_after_rotate_twice


def runtests(tests, suite_name):
    infile = '/tmp/infile'
    outfile = '/tmp/outfile'
    outfile2 = '/tmp/outfile2'
    integrity = '/tmp/integrity'
    assert shell_return(f'touch {infile}', suppress=True) == 0
    assert shell_return(f'dd if=/dev/urandom of={infile} bs=4096 count=20', suppress=True) == 0
    assert shell_return(f'touch {integrity}', suppress=True) == 0
    assert shell_return(f'cp {infile} {integrity}', suppress=True) == 0

    for (i, test) in enumerate(tests):
        # Truncate output files
        assert shell_return(f'> {outfile}', suppress=True) == 0
        assert shell_return(f'> {outfile2}', suppress=True) == 0

        log(f'{OKBLUE}{i + 1}. {test}{ENDC}')

        testfun = tests[test]
        if test == "ascii_independence":
            assert shell_return(f'touch man_nonascii.txt', suppress=True) == 0
            f = open("man_nonascii.txt", 'w')
            f.write("Make\x00sure\x00your\x00cache\x00can\x00handle\x00null\x00bytes! 가정하는 것은 안전하지 않습니다 प्रत्येकं पात्रं इति 'n ASCII-karakter.")
            f.close()
            assert shell_return(f'touch man_integrity', suppress=True) == 0
            assert shell_return(f'cp man_nonascii.txt man_integrity', suppress=True) == 0

            passed = testfun("man_nonascii.txt", outfile, outfile2)
            if not files_same("man_nonascii.txt", "man_integrity"):
                print(WARNING + 'oops, your program modified the input file' + ENDC)
                tests[test] = False
            else:
                tests[test] = True

            tests[test] = passed
            assert shell_return(f'rm -- man_nonascii.txt man_integrity', suppress=True) == 0
        else:
            passed = testfun(infile, outfile, outfile2)

            if not files_same(infile, integrity):
                print(WARNING + 'oops, your program modified the input file' + ENDC)
                tests[test] = False
            else:
                tests[test] = passed

        if tests[test]:
            log("\t" + OKGREEN + "PASSED!" + ENDC)

    log('\nYour results follow, indicating if each test passed or failed.')
    log(f'======= SUMMARY:  {suite_name} CORRECTNESS TESTS =======')

    ok = True

    if "--grader" not in sys.argv:
        for (test, passed) in tests.items():
            if passed:
              print("{}: \033[32;1mPASSED\033[0m".format(test))
            else:
              print("{}: \033[31;1mFAILED\033[0m".format(test))
              ok = False
    else:
        print(json.dumps(tests, indent=4))

    assert shell_return(f'rm -- {infile} {outfile} {outfile2} {integrity}', suppress=True) == 0
    return ok


def _msg_basic_fail():
    log('\nFor failing basic tets, you should debug with a small sample file to see how your output is different!')
    log('For example, try running the ./io300_test command for that test, and use the -i option to specify a test file.')
    log('Also, if there is sanitizer output, that\'s a good place to start.')


def _msg_e2e_fail():
    log('\nFor failing end-to-end tests, you should debug with a small sample file to see how your output is different!')
    log('For example, try running the command for the test using a small file as the input file.')
    log('To see what each test does, look at the code for the test in the `test_programs` directory')
    log('Also, if there is sanitizer output, that\'s a good place to start.')


def main(input_args):
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", help="random seed",
                        type=int, default=-1)
    parser.add_argument("test_group", type=str, default="all")

    args = parser.parse_args(input_args)

    # Random seed
    if args.seed != -1:
        # User provided a random seed
        rand_seed = args.seed
    else:
        # Otherwise, generate a random seed to use for all unit tests
        rand_seed = random.randint(0, 2 ** 32)

    all_basic = {
        'readc': create_unit_test("readc", rand_seed),
        'writec': create_unit_test("writec", rand_seed),
        'readc/writec': create_unit_test("readc writec", rand_seed),
        'read': create_unit_test("read=17", rand_seed),
        'read_random': create_unit_test("read", rand_seed, file_size=4129),
        'write': create_unit_test("write=17", rand_seed),
        'write_random': create_unit_test("write", rand_seed, file_size=4129),
        'read/write': create_unit_test("read write", rand_seed),
        'ascii_independence': create_block_cat(17),
        'read/write/seek': create_unit_test("read write seek", rand_seed),
        'seek_beyond_eof': create_unit_test("readc writec seek", rand_seed, file_size=0, max_file_size=4096, num_ops=1000),
        'all_read_write': create_unit_test("readc writec read write", rand_seed),
    }

    all_e2e = {
        'all_interaction': create_unit_test("readc writec read write seek", rand_seed),
        'byte_cat': byte_cat,
        # 'diabolical_byte_cat': diabolical_byte_cat,
        'reverse_byte_cat': reverse_byte_cat,
        'block_cat_1': create_block_cat(1),
        'block_cat_17': create_block_cat(17),
        'block_cat_334': create_block_cat(334),
        'block_cat_huge': create_block_cat(8192),
        'block_cat_gargantuan': create_block_cat(32768),
        'reverse_block_cat_1': create_reverse_block_cat(1),
        'reverse_block_cat_13': create_reverse_block_cat(13),
        'reverse_block_cat_987': create_reverse_block_cat(987),
        'reverse_block_cat_huge': create_reverse_block_cat(8192),
        'random_block_cat': random_block_cat,
        'rot13': rot13
    }

    run_all_basic = False
    run_all_e2e = False

    if args.test_group == "all":
        run_all_basic = True
        run_all_e2e = True
    elif args.test_group == "basic":
        run_all_basic = True
    elif args.test_group == "e2e":
        run_all_e2e = True
    else:
        print(f"Unrecognized test group:  {args.test_group}")
        return 1

    if run_all_basic:
        log(WARNING + 'RANDOM SEED FOR THIS RUN: ' + str(rand_seed) + ENDC)
        log('======= (1) BASIC FUNCTIONALITY TESTS =======')
        basic_ok = runtests(all_basic, "BASIC FUNCTIONALITY")

        if not basic_ok:
            _msg_basic_fail()
            log('Random seed for this test run was: ' + str(rand_seed))

    if run_all_e2e:
        log('\n======= (2) END-TO-END TESTS =======')
        end_to_end_ok = runtests(all_e2e, "END-TO-END")

        if not end_to_end_ok:
            _msg_e2e_fail()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
