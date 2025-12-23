import os
import subprocess
import sys
import argparse
from collections import defaultdict

EXAMPLES_DIR = "examples"

def run_gitmem_test(gitmem_path, file_path, should_accept):
  try:
    result = subprocess.run(
      [gitmem_path, file_path, "-e", "-o", "/dev/null"],
      capture_output=True,
      text=True
    )
    accepted = (result.returncode == 0)
  except FileNotFoundError:
    print(f"Error: '{gitmem_path}' executable not found.")
    sys.exit(1)

  status = "PASS" if accepted == should_accept else "FAIL"
  print(f"[{status}] {file_path} (exit code: {result.returncode})")
  return status == "PASS"

def main():
  parser = argparse.ArgumentParser(description="Test runner for gitmem.")
  parser.add_argument(
    "--gitmem", "-g",
    required=True,
    help="Path to the gitmem executable"
  )
  args = parser.parse_args()
  gitmem_path = args.gitmem

  # results[expectation][category][subcategory] = {"total": x, "failed": y}
  results = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: {
    "total": 0,
    "failed": 0
  })))

  total_tests = 0
  failed_tests = 0

  for expectation in ["accept", "reject"]:
    should_accept = (expectation == "accept")

    for category in ["syntax", "semantics"]:
      base_dir = os.path.join(EXAMPLES_DIR, expectation, category)
      if not os.path.isdir(base_dir):
        continue

      if category == "semantics":
        subcategories = ["branching", "linear"]
      else:
        subcategories = [None]

      for subcategory in subcategories:
        if subcategory:
          test_dir = os.path.join(base_dir, subcategory)
          if not os.path.isdir(test_dir):
            continue
        else:
          test_dir = base_dir

        for root, _, files in os.walk(test_dir):
          for file in files:
            file_path = os.path.join(root, file)

            total_tests += 1
            results[expectation][category][subcategory]["total"] += 1

            if not run_gitmem_test(gitmem_path, file_path, should_accept):
              failed_tests += 1
              results[expectation][category][subcategory]["failed"] += 1

  print("\nDetailed Summary:")
  for expectation, categories in results.items():
    print(f"\n{expectation.upper()}:")
    for category, subcats in categories.items():
      print(f"  {category}:")
      for subcategory, stats in subcats.items():
        label = subcategory if subcategory else "all"
        passed = stats["total"] - stats["failed"]
        print(
          f"    {label}: "
          f"{passed}/{stats['total']} passed "
          f"({stats['failed']} failed)"
        )

  print("\nOverall Summary:")
  print(f"Total tests run: {total_tests}")
  print(f"Tests failed:    {failed_tests}")
  print(f"Tests passed:    {total_tests - failed_tests}")

  if failed_tests > 0:
    sys.exit(1)

if __name__ == "__main__":
  main()
