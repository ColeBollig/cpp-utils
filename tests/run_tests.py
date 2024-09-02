#!/usr/bin/env python3

import subprocess
import shutil
import os

TEST_DIR = os.getcwd()
EXECUTE_DIR = os.path.join(TEST_DIR, "executions")

TESTS = [file for file in os.listdir() if file[:5] == "test_" and file[-3:] == ".py"]

if not os.path.exists(EXECUTE_DIR):
    os.mkdir(EXECUTE_DIR)

os.chdir(EXECUTE_DIR)

for test in TESTS:
    name = test[:-3]
    sub_dir = os.path.join(EXECUTE_DIR, name)
    if os.path.exists(sub_dir):
        shutil.rmtree(sub_dir)
    os.mkdir(sub_dir)
    os.chdir(sub_dir)

    test_file = os.path.join(TEST_DIR, test)
    shutil.copy(test_file, ".")

    p = subprocess.run([f"./{test}"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120)
    print(f"----------{name}----------")
    print("stdout:")
    for line in p.stdout.strip().decode().split("\n"):
        print(f"\t{line}")
    print("stderr:")                              
    for line in p.stderr.strip().decode().split("\n"): 
        print(f"\t{line}")

    os.chdir(EXECUTE_DIR)

os.chdir(TEST_DIR)
