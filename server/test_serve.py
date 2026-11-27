"""Tests for serve.py:  python -m unittest discover server"""

import hashlib
import http.client
import json
import tempfile
import threading
import unittest
from pathlib import Path

import serve

REPO_RECIPES = Path(__file__).resolve().parent.parent / "recipes"

SAMPLE = {
    "title": "Placki: ziemniaczane",
    "tags": ["polskie", "wegetariańskie"],
    "ingredients": ["1 kg ziemniaków", "1 jajko"],
    "steps": ["Zetrzyj ziemniaki.", "Smaż na złoto."],
}


class FormatTest(unittest.TestCase):
    def test_slugify_transliterates_polish(self):
        self.assertEqual(serve.slugify("Żurek z białą kiełbasą!"), "zurek-z-biala-kielbasa")
        self.assertEqual(serve.slugify("???"), "przepis")

    def test_render_then_parse_round_trips(self):
        self.assertEqual(serve.parse(serve.render(SAMPLE)), SAMPLE)

    def test_parses_hand_written_repo_recipe(self):
        r = serve.parse((REPO_RECIPES / "zurek.md").read_text(encoding="utf-8"))
        self.assertEqual(r["title"], "Żurek z białą kiełbasą")
        self.assertEqual(r["tags"], ["zupy", "mięso", "polskie"])
        self.assertEqual(len(r["ingredients"]), 14)
        self.assertEqual(len(r["steps"]), 7)
        self.assertTrue(r["steps"][0].startswith("Boczek pokrój"))  # "1. " stripped

    def test_validate_keeps_the_file_format_intact(self):
        r = serve.validate({"title": "a\nb", "tags": ["x, y", "[z]", "x, y"], "ingredients": ["", " s "]})
        self.assertEqual(r["title"], "a b")
        self.assertEqual(r["tags"], ["x y", "z"])
        self.assertEqual(r["ingredients"], ["s"])

    def test_validate_rejects_bad_input(self):
        for bad in [None, {"title": ""}, {"title": 5}, {"title": "x", "tags": "zupy"}, {"title": "x" * 201}]:
            with self.assertRaises(serve.BadRequest):
                serve.validate(bad)


class ApiTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.server = serve.make_server(self.tmp.name, "127.0.0.1", 0)
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.tmp.cleanup()

    def request(self, method, path, body=None, content_type="application/json"):
        conn = http.client.HTTPConnection("127.0.0.1", self.server.server_address[1])
        headers = {"Content-Type": content_type} if body is not None else {}
        conn.request(method, path, body=json.dumps(body) if body is not None else None, headers=headers)
        resp = conn.getresponse()
        data = resp.read()
        conn.close()
        return resp.status, json.loads(data) if data and resp.getheader("Content-Type", "").startswith("application/json") else data

    def test_create_read_update_delete(self):
        status, created = self.request("POST", "/api/recipes", SAMPLE)
        self.assertEqual(status, 201)
        self.assertEqual(created["name"], "placki-ziemniaczane")

        status, again = self.request("POST", "/api/recipes", SAMPLE)
        self.assertEqual(again["name"], "placki-ziemniaczane-2")  # no overwrite on same title

        status, listed = self.request("GET", "/api/recipes")
        self.assertEqual({r["name"] for r in listed}, {"placki-ziemniaczane", "placki-ziemniaczane-2"})

        status, updated = self.request("PUT", "/api/recipes/placki-ziemniaczane", {**SAMPLE, "title": "Placki"})
        self.assertEqual((status, updated["title"]), (200, "Placki"))

        status, _ = self.request("DELETE", "/api/recipes/placki-ziemniaczane")
        self.assertEqual(status, 204)
        self.assertEqual(self.request("GET", "/api/recipes/placki-ziemniaczane")[0], 404)
        trashed = list((Path(self.tmp.name) / ".trash").glob("placki-ziemniaczane-*.md"))
        self.assertEqual(len(trashed), 1)  # moved, not destroyed

    def test_rejects_path_traversal_and_unknown_routes(self):
        self.assertEqual(self.request("GET", "/api/recipes/..%2Fsecret")[0], 404)
        self.assertEqual(self.request("GET", "/api/recipes/../serve")[0], 404)
        self.assertEqual(self.request("GET", "/nope")[0], 404)

    def test_requires_json_content_type_for_writes(self):
        status, body = self.request("POST", "/api/recipes", SAMPLE, content_type="text/plain")
        self.assertEqual(status, 400)
        self.assertIn("Content-Type", body["error"])

    def test_device_sync_index_and_raw_files(self):
        _, created = self.request("POST", "/api/recipes", SAMPLE)
        status, index = self.request("GET", "/index.txt")
        self.assertEqual(status, 200)
        digest, name = index.decode().split()
        self.assertEqual(name, "placki-ziemniaczane.md")

        status, raw = self.request("GET", "/recipes/placki-ziemniaczane.md")
        self.assertEqual(status, 200)
        self.assertEqual(hashlib.sha256(raw).hexdigest(), digest)  # the device verifies this
        self.assertEqual(self.request("GET", "/recipes/..%2Fserve.md")[0], 404)

    def test_index_skips_trash_and_temp_files(self):
        self.request("POST", "/api/recipes", SAMPLE)
        self.request("DELETE", "/api/recipes/placki-ziemniaczane")
        (Path(self.tmp.name) / ".x.tmp").write_text("half")
        self.assertEqual(self.request("GET", "/index.txt")[1], b"")

    def test_serves_the_web_app(self):
        status, body = self.request("GET", "/")
        self.assertEqual(status, 200)
        self.assertIn(b"<title>", body)


if __name__ == "__main__":
    unittest.main()
