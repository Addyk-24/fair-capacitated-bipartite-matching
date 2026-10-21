import csv
import importlib.util
import json
import os
import tempfile
import unittest

MODULE_PATH = os.path.join(os.path.dirname(__file__), "..", "..", "applications", "convert_student_projects.py")
SPEC = importlib.util.spec_from_file_location("student_project_converter", MODULE_PATH)
CONVERTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CONVERTER)


class ConverterTests(unittest.TestCase):
    def convert(self, students, projects):
        root = tempfile.TemporaryDirectory(dir=os.path.dirname(__file__))
        source = os.path.join(root.name, "source")
        output = os.path.join(root.name, "output")
        os.makedirs(source)
        with open(os.path.join(source, "students.csv"), "w", newline="", encoding="utf8") as f:
            w = csv.DictWriter(f, fieldnames=["student_id", "group", "skills"])
            w.writeheader(); w.writerows(students)
        with open(os.path.join(source, "projects.csv"), "w", newline="", encoding="utf8") as f:
            w = csv.DictWriter(f, fieldnames=["project_id", "capacity", "minimum_coverage", "skills"])
            w.writeheader(); w.writerows(projects)
        return root, source, output

    @staticmethod
    def project(skills, threshold=""):
        return {"project_id": "p", "capacity": "1", "minimum_coverage": threshold,
                "skills": json.dumps(skills)}

    @staticmethod
    def skill(name, weight, mandatory=False):
        return {"name": name, "weight": weight, "mandatory": mandatory}

    def test_mandatory_coverage_and_weighted_utility(self):
        root, source, output = self.convert(
            [{"student_id": "a", "group": "g", "skills": "Python,SQL,Statistics"},
             {"student_id": "b", "group": "g", "skills": "Python,Statistics"}],
            [self.project([self.skill("Python", 35, True), self.skill("SQL", 25, True),
                           self.skill("Statistics", 25), self.skill("Visualization", 15)])])
        try:
            CONVERTER.convert(source, output)
            with open(os.path.join(output, "edges.csv"), encoding="utf8", newline="") as f:
                rows = list(csv.DictReader(f))
            self.assertEqual([(row["agent_id"], row["utility"]) for row in rows], [("0", "85")])
        finally:
            root.cleanup()

    def test_threshold_zero_keeps_zero_utility_edge(self):
        root, source, output = self.convert(
            [{"student_id": "a", "group": "g", "skills": ""}],
            [self.project([self.skill("Absent", 9)], "0")])
        try:
            CONVERTER.convert(source, output)
            with open(os.path.join(output, "edges.csv"), encoding="utf8", newline="") as f:
                rows = list(csv.DictReader(f))
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0]["utility"], "0")
        finally:
            root.cleanup()

    def test_default_coverage_threshold_is_35_percent(self):
        root, source, output = self.convert(
            [{"student_id": "a", "group": "g", "skills": "x"}],
            [self.project([self.skill("x", 30), self.skill("y", 70)])])
        try:
            CONVERTER.convert(source, output)
            with open(os.path.join(output, "edges.csv"), encoding="utf8", newline="") as f:
                self.assertEqual(list(csv.DictReader(f)), [])
            with open(os.path.join(output, "config.txt"), encoding="utf8") as f:
                self.assertIn("default_minimum_coverage=35", f.read())
        finally:
            root.cleanup()

    def test_default_threshold_and_validation_errors(self):
        students = [{"student_id": "a", "group": "g", "skills": "x"}]
        for skills, threshold, message in [
            ([self.skill("x", 1, True), self.skill("y", 1, True),
              self.skill("z", 1, True), self.skill("w", 1, True)], "", "maximum is 3"),
            ([self.skill("x", 1), self.skill("x", 2)], "", "duplicate skill"),
            ([self.skill("x", 0)], "", "finite and positive"),
            ([self.skill("x", 1)], "101", "from 0 to 100"),
            ([], "", "non-empty JSON array"),
        ]:
            root, source, output = self.convert(students, [self.project(skills, threshold)])
            try:
                with self.assertRaisesRegex(ValueError, message):
                    CONVERTER.convert(source, output)
            finally:
                root.cleanup()


if __name__ == "__main__":
    unittest.main()
