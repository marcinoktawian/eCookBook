#!/usr/bin/env python3
"""eCookBook server: recipe Markdown files on disk, a small JSON API and the web app.

Standard library only, so it runs on a bare python image.

    python serve.py --dir ./recipes --port 8082
"""

import argparse
import json
import os
import re
import tempfile
import time
import unicodedata
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

MAX_BODY_BYTES = 64 * 1024
NAME_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
WEB_DIR = Path(__file__).resolve().parent / "web"

INGREDIENTS = "Składniki"
STEPS = "Przygotowanie"
_POLISH = str.maketrans("ąćęłńóśźżĄĆĘŁŃÓŚŹŻ", "acelnoszzACELNOSZZ")
_STEP_NUMBER = re.compile(r"^\d+[.)]\s*")


class BadRequest(ValueError):
    """Client sent something the server refuses; the message is shown to the user."""


# --- Recipe format ------------------------------------------------------------------------


def slugify(title):
    s = unicodedata.normalize("NFKD", title.translate(_POLISH)).encode("ascii", "ignore").decode()
    s = re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-")
    return s[:60].strip("-") or "przepis"


def _unquote(s):
    if len(s) >= 2 and s[0] == s[-1] and s[0] in "\"'":
        return s[1:-1]
    return s


def parse(markdown):
    """Reads our recipe format back into form fields.

    Lines under a heading starting with "Składnik" become ingredients; everything else
    (the "Przygotowanie" section, other sections, text before any heading) becomes steps,
    one per paragraph or bullet.
    """
    title, tags, ingredients, steps = "", [], [], []
    lines = markdown.splitlines()
    i = 0
    if lines and lines[0].strip() == "---":
        i = 1
        while i < len(lines) and lines[i].strip() != "---":
            line = lines[i].strip()
            if line.startswith("title:"):
                title = _unquote(line[6:].strip())
            elif line.startswith("tags:"):
                raw = line[5:].strip().strip("[]")
                tags = [_unquote(t.strip()) for t in raw.split(",") if t.strip()]
            i += 1
        i += 1

    in_ingredients = False
    paragraph = []

    def flush():
        if paragraph:
            steps.append(_STEP_NUMBER.sub("", " ".join(paragraph)))
            paragraph.clear()

    for raw in lines[i:]:
        line = raw.strip()
        if line.startswith("#"):
            flush()
            in_ingredients = line.lstrip("#").strip().lower().startswith("składnik")
        elif not line:
            flush()
        elif in_ingredients:
            ingredients.append(line[2:].strip() if line[:2] in ("- ", "* ") else line)
        elif line[:2] in ("- ", "* "):
            flush()
            steps.append(line[2:].strip())
        else:
            paragraph.append(line)
    flush()
    return {"title": title, "tags": tags, "ingredients": ingredients, "steps": steps}


def render(recipe):
    tags = ", ".join(recipe["tags"])
    out = ["---", f"title: {recipe['title']}", f"tags: [{tags}]", "---", "", f"## {INGREDIENTS}", ""]
    out += [f"- {item}" for item in recipe["ingredients"]]
    out += ["", f"## {STEPS}", ""]
    for n, step in enumerate(recipe["steps"], 1):
        out += [f"{n}. {step}", ""]
    return "\n".join(out).rstrip() + "\n"


def _clean_line(value, field, max_len):
    if not isinstance(value, str):
        raise BadRequest(f"Pole „{field}” musi być tekstem.")
    value = " ".join(value.split())  # one line: newlines would break the file format
    if len(value) > max_len:
        raise BadRequest(f"Pole „{field}” jest za długie.")
    return value


def _clean_list(values, field, max_items, max_len):
    if not isinstance(values, list):
        raise BadRequest(f"Pole „{field}” musi być listą.")
    if len(values) > max_items:
        raise BadRequest(f"Pole „{field}” ma za dużo pozycji.")
    return [v for v in (_clean_line(v, field, max_len) for v in values) if v]


def validate(data):
    """Turns untrusted JSON from the web form into a recipe dict, or raises BadRequest."""
    if not isinstance(data, dict):
        raise BadRequest("Nieprawidłowe dane przepisu.")
    title = _clean_line(data.get("title", ""), "tytuł", 200)
    if not title:
        raise BadRequest("Podaj tytuł przepisu.")
    tags = _clean_list(data.get("tags", []), "tagi", 20, 40)
    # Commas and brackets would split or end the inline `tags: [...]` list.
    tags = list(dict.fromkeys(t for t in (re.sub(r"[,\[\]]", "", t).strip() for t in tags) if t))
    return {
        "title": title,
        "tags": tags,
        "ingredients": _clean_list(data.get("ingredients", []), "składniki", 200, 300),
        "steps": _clean_list(data.get("steps", []), "przygotowanie", 100, 5000),
    }


# --- Storage ------------------------------------------------------------------------------


class Store:
    def __init__(self, directory):
        self.dir = Path(directory)
        self.dir.mkdir(parents=True, exist_ok=True)

    def _path(self, name):
        if not NAME_RE.match(name or ""):
            raise FileNotFoundError(name)
        return self.dir / f"{name}.md"

    def list(self):
        out = []
        for path in sorted(self.dir.glob("*.md")):
            recipe = parse(path.read_text(encoding="utf-8", errors="replace"))
            out.append({"name": path.stem, **recipe})
        return out

    def get(self, name):
        text = self._path(name).read_text(encoding="utf-8", errors="replace")
        return {"name": name, **parse(text)}

    def _write(self, path, recipe):
        # Write next to the target and rename, so a reader (the E1001 sync) never sees half a file.
        fd, tmp = tempfile.mkstemp(dir=self.dir, prefix=".", suffix=".tmp")
        try:
            with os.fdopen(fd, "w", encoding="utf-8") as f:
                f.write(render(recipe))
                f.flush()
                os.fsync(f.fileno())
            os.replace(tmp, path)
        except BaseException:
            os.unlink(tmp)
            raise

    def create(self, recipe):
        base = slugify(recipe["title"])
        name, n = base, 2
        while (self.dir / f"{name}.md").exists():
            name, n = f"{base}-{n}", n + 1
        self._write(self.dir / f"{name}.md", recipe)
        return name

    def update(self, name, recipe):
        path = self._path(name)
        if not path.exists():
            raise FileNotFoundError(name)
        self._write(path, recipe)

    def delete(self, name):
        path = self._path(name)
        if not path.exists():
            raise FileNotFoundError(name)
        trash = self.dir / ".trash"
        trash.mkdir(exist_ok=True)
        os.replace(path, trash / f"{name}-{time.strftime('%Y%m%d-%H%M%S')}.md")


# --- HTTP ---------------------------------------------------------------------------------


class Handler(BaseHTTPRequestHandler):
    store = None  # set by make_server
    server_version = "eCookBook"

    def _send(self, status, body=b"", content_type="application/json; charset=utf-8"):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _json(self, status, value):
        self._send(status, json.dumps(value, ensure_ascii=False).encode("utf-8"))

    def _error(self, status, message):
        self._json(status, {"error": message})

    def _read_json(self):
        # Requiring a JSON content type also blocks cross-site form posts: a browser must send
        # a CORS preflight for it, and this server never approves one.
        if not self.headers.get("Content-Type", "").startswith("application/json"):
            raise BadRequest("Wymagany Content-Type: application/json.")
        length = int(self.headers.get("Content-Length") or 0)
        if length > MAX_BODY_BYTES:
            raise BadRequest("Przepis jest za duży.")
        try:
            return json.loads(self.rfile.read(length) or b"null")
        except (json.JSONDecodeError, UnicodeDecodeError):
            raise BadRequest("Nieprawidłowy JSON.")

    def _route(self):
        parts = [p for p in self.path.split("?")[0].split("/") if p]
        if parts[:2] == ["api", "recipes"] and len(parts) <= 3:
            return parts[2] if len(parts) == 3 else None
        raise FileNotFoundError(self.path)

    def _handle(self, method):
        try:
            if method == "GET" and self.path.split("?")[0] in ("/", "/index.html"):
                self._send(HTTPStatus.OK, (WEB_DIR / "index.html").read_bytes(), "text/html; charset=utf-8")
                return
            name = self._route()
            if method == "GET" and name is None:
                self._json(HTTPStatus.OK, self.store.list())
            elif method == "GET":
                self._json(HTTPStatus.OK, self.store.get(name))
            elif method == "POST" and name is None:
                created = self.store.create(validate(self._read_json()))
                self._json(HTTPStatus.CREATED, self.store.get(created))
            elif method == "PUT" and name is not None:
                self.store.update(name, validate(self._read_json()))
                self._json(HTTPStatus.OK, self.store.get(name))
            elif method == "DELETE" and name is not None:
                self.store.delete(name)
                self._send(HTTPStatus.NO_CONTENT)
            else:
                self._error(HTTPStatus.METHOD_NOT_ALLOWED, "Niedozwolona operacja.")
        except BadRequest as e:
            self._error(HTTPStatus.BAD_REQUEST, str(e))
        except FileNotFoundError:
            self._error(HTTPStatus.NOT_FOUND, "Nie ma takiego przepisu.")

    def do_GET(self):
        self._handle("GET")

    def do_POST(self):
        self._handle("POST")

    def do_PUT(self):
        self._handle("PUT")

    def do_DELETE(self):
        self._handle("DELETE")


def make_server(directory, host, port):
    handler = type("BoundHandler", (Handler,), {"store": Store(directory)})
    return ThreadingHTTPServer((host, port), handler)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dir", default="recipes", help="folder with recipe .md files")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8082)
    args = parser.parse_args()
    server = make_server(args.dir, args.host, args.port)
    print(f"eCookBook: serving {Path(args.dir).resolve()} on http://{args.host}:{args.port}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
