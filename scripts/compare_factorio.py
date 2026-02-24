#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
import random
import numpy as np
import cv2


def run_command(cmd, cwd=None):
    print("Running:", cmd)
    res = subprocess.run(cmd, shell=True, cwd=cwd)
    return res.returncode


def images_identical(path_a: Path, path_b: Path) -> bool:
    a = cv2.imread(path_a)
    b = cv2.imread(path_b)

    difference = cv2.subtract(a, b)    
    return not np.any(difference)

def save_diff(path_a: Path, path_b: Path, path_out: Path):
    a = cv2.imread(path_a)
    b = cv2.imread(path_b)

    difference = cv2.absdiff(a, b)
    cv2.imwrite(path_out, difference)

def main():
    parser = argparse.ArgumentParser(description="Run C++ renderer and Factorio for seeds and compare images.")
    parser.add_argument('--cpp-bin', required=True, help='Path to the C++ renderer binary (must write output to {cpp_out} or default patches.ppm in cwd)')
    parser.add_argument('--factorio-cmd', required=True,
                        help=('Command template to run Factorio that accepts {seed} and {out} placeholders. ' 
                              'Example: "factorio --create tmp.zip --seed {seed} --screenshot {out}"'))
    parser.add_argument('--start-seed', type=int, default=None, help='Start from this seed; if omitted a random seed is chosen first')
    parser.add_argument('--max-iter', type=int, default=0, help='Maximum seeds to test (0 = infinite until mismatch)')
    parser.add_argument('--out-dir', default='compare_out', help='Directory to save failing pairs')
    parser.add_argument('--factorio-outname', default='factorio.png', help='Filename Factorio should write when using {out} (default factorio.png)')
    parser.add_argument('--seed-random-bits', type=int, default=32, help='Number of bits for random seeds')
    args = parser.parse_args()

    cpp_bin = Path(args.cpp_bin).resolve()
    factorio_template = args.factorio_cmd
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    rng = random.SystemRandom()

    iter_count = 0
    seed = args.start_seed

    while True:
        if seed is None:
            seed = rng.getrandbits(args.seed_random_bits)
        iter_count += 1

        tmp = Path(tempfile.mkdtemp(prefix=f"cmp_seed_{seed}_"))
        try:
            # run C++ renderer in tmpdir; it is expected to write args.cpp_outname in cwd
            rc = run_command(f'"{cpp_bin}" {seed}', cwd=tmp)
            if rc != 0:
                print(f"C++ renderer exited with code {rc} for seed {seed}")
                shutil.rmtree(tmp)
                return 2

            cpp_out = Path("/home/ness056/CodeProjets/factorio-reverse-engineering/build/patches.ppm")
            if not cpp_out.exists():
                print(f"C++ output not found: {cpp_out}")
                shutil.rmtree(tmp)
                return 3

            # prepare factorio command; user must include {seed} and {out}
            factorio_out = tmp / args.factorio_outname
            cmd = factorio_template.format(seed=seed, out=str(factorio_out))
            rc = run_command(cmd, cwd=tmp)
            if rc != 0:
                print(f"Factorio command exited with code {rc} for seed {seed}")
                shutil.rmtree(tmp)
                return 4

            if not factorio_out.exists():
                print(f"Factorio output not found: {factorio_out}")
                shutil.rmtree(tmp)
                return 5

            identical = images_identical(cpp_out, factorio_out)
            if identical:
                print(f"Seed {seed}: images match")
                # continue to next seed unless max reached
                if args.max_iter and iter_count >= args.max_iter:
                    print("Reached max iterations; exiting")
                    shutil.rmtree(tmp)
                    return 0

                # prepare next random seed
                seed = None
                shutil.rmtree(tmp)
                continue
            else:
                # save both images to out_dir with seed in filename
                dst_cpp = out_dir / f"{seed}_cpp.ppm"
                dst_fact = out_dir / f"{seed}_factorio.png"
                shutil.copy(cpp_out, dst_cpp)
                shutil.copy(factorio_out, dst_fact)
                save_diff(dst_cpp, dst_fact, out_dir / f"{seed}_diff.png")
                print(f"Seed {seed}: IMAGES DID NOT MATCH -- saved to {dst_cpp} and {dst_fact}")
                shutil.rmtree(tmp)
                return 1

        finally:
            # tmp cleaned up above on continue/return; ensure not lingering
            if tmp.exists():
                try:
                    shutil.rmtree(tmp)
                except Exception:
                    pass


if __name__ == '__main__':
    main()
