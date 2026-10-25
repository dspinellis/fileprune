"""Regression checks for calendar retention; run with python3 test_human.py."""
import datetime as dt
import os
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

PROGRAM = str(Path(__file__).resolve().parent / "fileprune")


class HumanPeriods(unittest.TestCase):
    def run_dates(self, dates, *options):
        return subprocess.run(
            [PROGRAM, "-h", "-d", *options, *dates],
            text=True, capture_output=True, check=True,
        ).stdout.splitlines()

    def check_calendar(self, timezone):
        original = os.environ.get("TZ")
        os.environ["TZ"] = timezone
        time.tzset()
        try:
            today = dt.datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
            # Include leap days, missing days, multiple copies per day, and
            # both sides of calendar boundaries across several years.
            dates = [today - dt.timedelta(days=i, hours=-hour)
                     for i in range(1500) if i % 11 != 9
                     for hour in (0, 12)]
            expected = set()
            for i in range(8):
                day = (today - dt.timedelta(days=i)).date()
                copies = [d for d in dates if d.date() == day]
                if copies:
                    expected.add(max(copies))
            boundaries = []
            for i in range(13):
                year, month = divmod(today.year * 12 + today.month - 1 - i, 12)
                boundaries.append(dt.datetime(year, month + 1, 1))
            boundaries.extend(dt.datetime(year, 1, 1)
                              for year in range(min(d.year for d in dates), today.year + 1))
            for boundary in boundaries:
                expected.add(min(dates, key=lambda d: (
                    abs(d.timestamp() - boundary.timestamp()), -d.timestamp())))
            strings = [d.isoformat(sep=" ") for d in dates]
            retained = self.run_dates(strings, "-N")
            self.assertEqual(retained, [d.isoformat(sep=" ") for d in sorted(expected, reverse=True)])
            removed = self.run_dates(strings, "-n")
            self.assertEqual(set(removed), set(strings) - set(retained))
            self.assertEqual(len(self.run_dates(strings, "-N", "-c", "5")), 5)
            self.assertEqual(self.run_dates(strings, "-N", "-c", "0"), [])
            self.assertEqual(set(self.run_dates(strings, "-N", "-c", "1", "-K")), set(retained))
        finally:
            if original is None:
                os.environ.pop("TZ", None)
            else:
                os.environ["TZ"] = original
            time.tzset()

    def test_utc(self):
        self.check_calendar("UTC")

    def test_dst(self):
        self.check_calendar("Europe/Athens")

    def test_sparse_and_tie(self):
        dates = ["2000-01-01", "2000-12-31", "2001-01-02", "2001-02-01"]
        self.assertEqual(self.run_dates(dates, "-N"), ["2001-02-01", "2001-01-02", "2000-01-01"])
        self.assertEqual(self.run_dates(["2001-01-02"], "-N"), ["2001-01-02"])

    def test_long_history(self):
        dates = ["1900-01-02", "1950-01-02", "2000-01-02"]
        self.assertEqual(self.run_dates(dates, "-N"), list(reversed(dates)))

    def test_incompatible_options(self):
        for options in (["-f"], ["-e", "2"], ["-g", "180"], ["-p", "-c", "5"]):
            result = subprocess.run([PROGRAM, "-h", *options, "-d", "-N", "2020-01-01"],
                                    capture_output=True)
            self.assertNotEqual(result.returncode, 0)

    def test_actual_files(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / str(i) for i in range(4)]
            stamp = dt.datetime(2001, 6, 1).timestamp()
            for i, path in enumerate(paths):
                path.write_text("backup")
                os.utime(path, (stamp + i * 86400, stamp + i * 86400))
            expected = subprocess.check_output([PROGRAM, "-h", "-N", *map(str, paths)], text=True).splitlines()
            subprocess.run([PROGRAM, "-h", *map(str, paths)], check=True)
            self.assertEqual(set(map(str, Path(directory).iterdir())), set(expected))


if __name__ == "__main__":
    unittest.main()
