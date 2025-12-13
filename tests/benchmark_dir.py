#!/usr/bin/env python3
import os
import sys
import subprocess
import argparse
import statistics
import re
import shlex

def is_executable(filepath):
    """Checks if a file exists, is a file (not dir), and is executable."""
    return (os.path.isfile(filepath) and 
            os.access(filepath, os.X_OK) and 
            not filepath.endswith('.ll'))

def run_benchmark(directory, num_runs):
    try:
        files = [f for f in os.listdir(directory) if is_executable(os.path.join(directory, f))]
    except FileNotFoundError:
        print(f"Error: Directory '{directory}' not found.")
        sys.exit(1)

    files.sort()

    if not files:
        print(f"No executable files found in {directory}")
        return

    print(f"{'='*90}")
    print(f"Benchmarking (User CPU Time) executables in: {directory}")
    print(f"Method: Bash built-in 'time'")
    print(f"Iterations per file: {num_runs}")
    print(f"{'='*90}")
    print(f"{'Filename':<35} | {'Avg User(s)':<12} | {'Min (s)':<10} | {'Max (s)':<10} | {'Stdev':<10}")
    print(f"{'-'*90}")

    # user <minutes>m<seconds>s
    user_time_pattern = re.compile(r'user\s+(\d+)m(\d+\.\d+)s')

    for filename in files:
        filepath = os.path.join(directory, filename)
        quoted_filepath = shlex.quote(filepath)
        
        try:
            # warmup run (discarded)
            subprocess.run([filepath], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)

            # measurement Loop
            times = []
            for _ in range(num_runs):
                cmd = ['bash', '-c', f'time {quoted_filepath} >/dev/null 2>&1']
                
                result = subprocess.run(
                    cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    check=False
                )
                
                match = user_time_pattern.search(result.stderr)
                if match:
                    minutes = float(match.group(1))
                    seconds = float(match.group(2))
                    total_seconds = (minutes * 60) + seconds
                    times.append(total_seconds)
                else:
                    pass

            if not times:
                print(f"{filename:<35} | {'Error/No Data':<12} | {'-':<10} | {'-':<10} | {'-':<10}")
                continue

            avg_time = statistics.mean(times)
            min_time = min(times)
            max_time = max(times)
            stdev = statistics.stdev(times) if len(times) > 1 else 0.0

            print(f"{filename:<35} | {avg_time:<12.6f} | {min_time:<10.6f} | {max_time:<10.6f} | {stdev:<10.6f}")

        except Exception as e:
            print(f"{filename:<35} | Error: {e}")

    print(f"{'-'*90}")
    print("Done.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Benchmark executables in a directory using bash time builtin.")
    parser.add_argument("directory", help="Directory containing executables")
    parser.add_argument("runs", nargs="?", type=int, default=100, help="Number of runs per executable (default: 100)")
    
    args = parser.parse_args()
    
    run_benchmark(args.directory, args.runs)