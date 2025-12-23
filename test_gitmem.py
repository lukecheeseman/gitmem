import os
import subprocess
import sys
import argparse
from collections import defaultdict

EXAMPLES_DIR = "examples"

def supports_color():
  return sys.stdout.isatty() and os.getenv("NO_COLOR") is None

def color(text, code):
  if not supports_color():
    return text
  return f"\033[{code}m{text}\033[0m"

def green(text):
  return color(text, "32")

def red(text):
  return color(text, "31")

def run_gitmem_test(gitmem_path, file_path, should_accept, is_branching):
  cmd = [gitmem_path, file_path, "-e", "-o", "/dev/null"]

  if is_branching:
    cmd.insert(2, "-b")

  try:
    result = subprocess.run(
      cmd,
      capture_output=True,
      text=True
    )
    accepted = (result.returncode == 0)
  except FileNotFoundError:
    print(f"Error: '{gitmem_path}' executable not found.")
    sys.exit(1)

  passed = (accepted == should_accept)
  status = green("PASS") if passed else red("FAIL")

  print(f"[{status}] {file_path} (exit code: {result.returncode})")
  return passed

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
  failing_tests = []

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
          is_branching = (subcategory == "branching")
        else:
          test_dir = base_dir
          is_branching = False

        for root, _, files in os.walk(test_dir):
          for file in files:
            file_path = os.path.join(root, file)

            total_tests += 1
            results[expectation][category][subcategory]["total"] += 1

            passed = run_gitmem_test(
              gitmem_path,
              file_path,
              should_accept,
              is_branching
            )

            if not passed:
              failed_tests += 1
              results[expectation][category][subcategory]["failed"] += 1
              failing_tests.append(file_path)

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

  if failing_tests:
    print("\nFailing tests:")
    for path in failing_tests:
      print(f"  {red(path)}")

  if failed_tests > 0:
    sys.exit(1)

if __name__ == "__main__":
  main()
