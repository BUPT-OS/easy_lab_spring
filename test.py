import os
import subprocess
import sys


def run_tests():
    scores = [15, 15, 20, 30, 10, 10]
    total_score = 0
    # Ensure we are in the directory containing the Makefile
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    failed_tests = []

    for i in range(1, 7):
        target = f"test{i}"
        expected_msg = f"*** PHASE {i} COMPLETE! ***"

        print(f"Running {target}...", end=" ", flush=True)

        try:
            # Running make command
            result = subprocess.run(["make", target], capture_output=True, text=True)

            # Combine stdout and stderr for checking, though usually it's in stdout
            output = result.stdout + result.stderr
            print(output)
            if expected_msg in output:
                total_score += scores[i - 1]
                print("PASS")
            else:
                print("FAIL")
                # print("--- Output ---")
                # print(output)
                # print("--------------")
                failed_tests.append(target)

        except Exception as e:
            print(f"ERROR: {e}")
            failed_tests.append(target)

    if failed_tests:
        print(
            f"\nSummary: {len(failed_tests)} tests failed ({', '.join(failed_tests)})."
        )
        sys.exit(1)
    else:
        print("\nAll tests passed!")
        print(f"\nSCORE: {total_score}")
        sys.exit(0)


if __name__ == "__main__":
    run_tests()
