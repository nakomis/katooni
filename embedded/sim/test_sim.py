"""The simulator's API end to end: real HTTP, real JSON parsing (so the core's JSON writer is checked
for validity, not just shape). Standard library only.

    python3 embedded/sim/test_sim.py build/sim/katooni-sim
"""
import json
import socket
import subprocess
import sys
import time
import unittest
import urllib.request

SIM = sys.argv.pop(1) if len(sys.argv) > 1 else "build/sim/katooni-sim"


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class SimTest(unittest.TestCase):
    def setUp(self):
        self.port = free_port()
        self.proc = subprocess.Popen([SIM, "--port", str(self.port)], stdout=subprocess.PIPE, text=True)
        self.assertIn("katooni-sim on", self.proc.stdout.readline())

    def tearDown(self):
        self.proc.terminate()
        self.proc.wait(5)
        self.proc.stdout.close()

    def call(self, path, method="GET"):
        req = urllib.request.Request(f"http://127.0.0.1:{self.port}{path}", method=method)
        with urllib.request.urlopen(req, timeout=5) as r:
            body = r.read().decode()
            return r.status, r.headers.get("Content-Type"), body

    def json(self, path, method="GET"):
        status, ctype, body = self.call(path, method)
        self.assertEqual(status, 200)
        self.assertEqual(ctype, "application/json")
        return json.loads(body)

    def test_page(self):
        status, ctype, body = self.call("/")
        self.assertEqual(status, 200)
        self.assertTrue(ctype.startswith("text/html"))
        self.assertIn("<title>Katooni</title>", body)
        self.assertIn("/api/session", body)

    def test_empty(self):
        self.assertEqual(self.json("/api/session"), {"tested": 0, "faulty_total": 0, "bags": [], "faulty": []})
        self.assertEqual(self.json("/api/last"), {"seq": 0, "have": False})

    def test_bagging_through_the_api(self):
        cables = self.json("/api/sim/cables")
        self.assertIn("cc-240w", cables)
        first = self.json("/api/sim/plug?cable=cc-240w", "POST")
        self.assertTrue(first["result"]["new_bag"])
        self.assertEqual(first["result"]["letter"], "A")
        self.assertEqual(first["label"], "C–C 240W 40G ≤1m")
        self.assertEqual(len(first["nets"]), 16)
        self.assertTrue(all(n["present"] for n in first["nets"]))
        self.assertEqual(first["emarker_reply"]["status"], "ok")
        self.assertTrue(first["resistance"]["measured"])

        again = self.json("/api/sim/plug?cable=cc-240w", "POST")
        self.assertFalse(again["result"]["new_bag"])
        self.assertEqual(again["result"]["count"], 2)

        thin = self.json("/api/sim/plug?cable=cc-thin-liar", "POST")
        self.assertTrue(thin["resistance"]["too_thin"])
        self.assertIn("e-marker claims 5 A", thin["reason"])

        broken = self.json("/api/sim/plug?cable=cc-dplus-open", "POST")
        self.assertTrue(broken["result"]["faulty"])
        self.assertTrue(any("D+ open" in f for f in broken["faults"]))

        rj = self.json("/api/sim/plug?cable=rj45-reversed", "POST")
        self.assertEqual(rj["rj45_map"], [2, 1, 3, 4, 5, 6, 7, 8])

        s = self.json("/api/session")
        self.assertEqual(s["tested"], 5)
        self.assertEqual(s["faulty_total"], 2)
        self.assertEqual([b["letter"] for b in s["bags"]], ["A", "B"])
        self.assertEqual(s["bags"][0]["count"], 2)
        self.assertFalse(s["bags"][0]["singleton"])
        self.assertTrue(s["bags"][1]["singleton"])
        self.assertEqual(s["bags"][0]["spec"]["emarker"]["watts"], 240)
        self.assertEqual(s["faulty"][0]["reason"], "pair 1/2 reversed")   # newest first
        self.assertEqual(self.json("/api/last")["seq"], 5)

        status, _, _ = self.call("/api/session/new", "POST")
        self.assertEqual(status, 204)
        self.assertEqual(self.json("/api/session")["bags"], [])
        last = self.json("/api/last")
        self.assertEqual(last, {"seq": 6, "have": False})

    def test_every_canned_cable_gives_valid_json(self):
        for name in self.json("/api/sim/cables"):
            with self.subTest(cable=name):
                last = self.json(f"/api/sim/plug?cable={name}", "POST")
                self.assertTrue(last["have"])
                self.assertTrue(last["label"])
        self.json("/api/session")

    def test_unknown(self):
        with self.assertRaises(urllib.error.HTTPError) as e:
            self.call("/api/sim/plug?cable=nope", "POST")
        self.assertEqual(e.exception.code, 404)
        with self.assertRaises(urllib.error.HTTPError) as e:
            self.call("/nope")
        self.assertEqual(e.exception.code, 404)


if __name__ == "__main__":
    unittest.main(verbosity=1)
