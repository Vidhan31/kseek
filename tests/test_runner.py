#!/usr/bin/env python3
"""Integration and unit tests for kseek."""
import os
import shutil
import sys
import tempfile
import time
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import gi
gi.require_version("Gio", "2.0")
gi.require_version("GLib", "2.0")
from gi.repository import Gio, GLib

from kseek import (
    INTROSPECTION_XML,
    KSeekRunner,
    QUERY_RE,
    _icon_for_extension,
    resolve_icon,
)


class TestKSeek(unittest.TestCase):
    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="kseek_test_")
        os.makedirs(os.path.join(self.test_dir, "src", "nested"), exist_ok=True)
        os.makedirs(os.path.join(self.test_dir, "docs"), exist_ok=True)
        os.makedirs(os.path.join(self.test_dir, ".git"), exist_ok=True)

        self.test_files = [
            os.path.join(self.test_dir, "main.py"),
            os.path.join(self.test_dir, "README.md"),
            os.path.join(self.test_dir, "src", "kseek.py"),
            os.path.join(self.test_dir, "src", "nested", "C++_advanced.cpp"),
            os.path.join(self.test_dir, "docs", "report [2024].pdf"),
            os.path.join(self.test_dir, "docs", "spaced name file.txt"),
            os.path.join(self.test_dir, ".hidden_config"),
            os.path.join(self.test_dir, ".git", "config"),
        ]
        for f in self.test_files:
            with open(f, "w") as fp:
                fp.write("test content\n")

        self.runner = KSeekRunner(search_root=self.test_dir)

    def tearDown(self):
        self.runner.cancel_active_search()
        shutil.rmtree(self.test_dir, ignore_errors=True)

    def _wait_for_results(self, results, max_wait=2.0):
        start = time.time()
        while time.time() - start < max_wait:
            while GLib.MainContext.default().iteration(False):
                pass
            if results:
                break
            time.sleep(0.02)
        while GLib.MainContext.default().iteration(False):
            pass

    def test_introspection_xml(self):
        """Verify D-Bus introspection XML conforms to org.kde.krunner1 spec."""
        node_info = Gio.DBusNodeInfo.new_for_xml(INTROSPECTION_XML)
        self.assertIsNotNone(node_info)
        self.assertEqual(len(node_info.interfaces), 1)

        iface = node_info.interfaces[0]
        self.assertEqual(iface.name, "org.kde.krunner1")

        method_names = [m.name for m in iface.methods]
        self.assertIn("Actions", method_names)
        self.assertIn("Match", method_names)
        self.assertIn("Run", method_names)
        self.assertIn("Teardown", method_names)
        self.assertIn("Config", method_names)
        self.assertIn("SetActivationToken", method_names)

    def test_query_regex(self):
        """Verify query prefix matching behavior."""
        m = QUERY_RE.match("f test")
        self.assertIsNotNone(m)
        self.assertEqual(m.group(1), "test")

        m2 = QUERY_RE.match("f   spaced query  ")
        self.assertIsNotNone(m2)
        self.assertEqual(m2.group(1), "spaced query  ")

        self.assertIsNone(QUERY_RE.match("g test"))
        self.assertIsNone(QUERY_RE.match("find test"))
        self.assertIsNone(QUERY_RE.match("f"))

    def test_icon_resolution(self):
        """Verify MIME icon caching and directory icon mapping."""
        self.assertEqual(resolve_icon(self.test_dir, is_dir=True), "inode-directory")

        py_icon = resolve_icon("test.py", is_dir=False)
        self.assertTrue("python" in py_icon or "text" in py_icon or "code" in py_icon)

        pdf_icon = resolve_icon("document.pdf", is_dir=False)
        self.assertTrue("pdf" in pdf_icon or "document" in pdf_icon or "application" in pdf_icon)

        cpp_icon = resolve_icon("test.cpp", is_dir=False)
        self.assertTrue("c" in cpp_icon or "text" in cpp_icon)

        icon1 = _icon_for_extension(".py")
        icon2 = _icon_for_extension(".py")
        self.assertEqual(icon1, icon2)

    def test_search_fuzzy_and_regex_safety(self):
        """Test search pipeline on edge cases."""
        if not self.runner.fd_bin or not self.runner.fzf_bin:
            self.skipTest("fd or fzf not installed")

        self.runner._current_request_id = 1
        results = []
        invocation_mock = _MockInvocation(lambda res: results.extend(res))

        self.runner._search_worker("f ksk", 1, invocation_mock)
        self._wait_for_results(results)

        self.assertTrue(len(results) > 0, "Fuzzy query 'ksk' should match kseek.py")
        match_texts = [r[1] for r in results]
        self.assertIn("kseek.py", match_texts)

        self.runner._current_request_id = 2
        results_regex = []
        inv_regex = _MockInvocation(lambda res: results_regex.extend(res))
        self.runner._search_worker("f C++", 2, inv_regex)
        self._wait_for_results(results_regex)

        self.assertTrue(len(results_regex) > 0, "Query 'C++' should match C++_advanced.cpp")
        self.assertIn("C++_advanced.cpp", [r[1] for r in results_regex])

        self.runner._current_request_id = 3
        results_bracket = []
        inv_bracket = _MockInvocation(lambda res: results_bracket.extend(res))
        self.runner._search_worker("f 2024", 3, inv_bracket)
        self._wait_for_results(results_bracket)

        self.assertTrue(len(results_bracket) > 0)
        self.assertIn("report [2024].pdf", [r[1] for r in results_bracket])

        git_matches = [r[1] for r in results if r[1] == "config"]
        self.assertEqual(len(git_matches), 0, ".git folder should be excluded")

    def test_search_cancellation_and_request_invalidation(self):
        """Test that superseding a search immediately invalidates the previous query's results."""
        if not self.runner.fd_bin or not self.runner.fzf_bin:
            self.skipTest("fd or fzf not installed")

        res_req1 = []
        res_req2 = []
        inv1 = _MockInvocation(lambda res: res_req1.extend(res))
        inv2 = _MockInvocation(lambda res: res_req2.extend(res))

        self.runner._current_request_id = 1
        self.runner._search_worker("f main", 1, inv1)

        self.runner._current_request_id = 2
        self.runner.cancel_active_search()
        self.runner._search_worker("f README", 2, inv2)

        self._wait_for_results(res_req2)

        self.assertTrue(len(res_req2) > 0)
        self.assertIn("README.md", [r[1] for r in res_req2])

        for r in res_req1:
            self.assertEqual(len(r), 0)

    def test_actions_list(self):
        """Verify Actions returns expected action triples."""
        inv = _MockInvocation(lambda res: None)
        self.runner._handle_method_call(
            None, "", "/kseek", "org.kde.krunner1", "Actions", GLib.Variant("()", ()), inv
        )
        self.assertIsNotNone(inv.last_value)
        actions = inv.last_value.unpack()[0]
        action_ids = [a[0] for a in actions]
        self.assertIn("open_app", action_ids)
        self.assertIn("show_item", action_ids)
        self.assertIn("copy_path", action_ids)
        self.assertIn("open_terminal", action_ids)

    def test_match_structure_and_plasma6_metadata(self):
        """Verify match tuples contain valid Plasma 6 types and properties."""
        match = self.runner._build_match("src/kseek.py", 0, 1)
        self.assertIsNotNone(match)
        match_id, text, icon, cat_rel, rel, props = match

        self.assertTrue(os.path.isabs(match_id))
        self.assertEqual(text, "kseek.py")
        self.assertIsInstance(icon, str)
        self.assertEqual(cat_rel, 70)
        self.assertEqual(rel, 1.0)

        self.assertIn("subtext", props)
        self.assertIn("category", props)
        self.assertIn("urls", props)
        self.assertIn("actions", props)
        self.assertIn("multiline", props)

        self.assertEqual(props["category"].unpack(), "Files & Folders")
        self.assertTrue(props["urls"].unpack()[0].startswith("file://"))
        self.assertFalse(props["multiline"].unpack())
        self.assertIn("open_app", props["actions"].unpack())
        self.assertIn("show_item", props["actions"].unpack())
        self.assertIn("copy_path", props["actions"].unpack())
        self.assertIn("open_terminal", props["actions"].unpack())

    def test_set_activation_token(self):
        """Verify SetActivationToken updates state properly."""
        self.runner.activation_token = ""
        token = "krunner-wayland-token-12345"
        self.runner.activation_token = token
        ctx = self.runner._create_app_launch_context()
        self.assertIsNotNone(ctx)
        self.assertEqual(self.runner.activation_token, token)

    def test_fd_custom_args_support(self):
        """Verify KSEEK_FD_ARGS allows enabling hidden files or custom options."""
        if not self.runner.fd_bin or not self.runner.fzf_bin:
            self.skipTest("fd or fzf not installed")

        self.runner._current_request_id = 98
        results_default = []
        inv_default = _MockInvocation(lambda res: results_default.extend(res))
        self.runner._search_worker("f hidden_config", 98, inv_default)
        self._wait_for_results(results_default)
        self.assertEqual(len(results_default), 0, "Hidden files should not be found by default")

        os.environ["KSEEK_FD_ARGS"] = "--hidden"
        try:
            self.runner._current_request_id = 99
            results = []
            inv = _MockInvocation(lambda res: results.extend(res))
            self.runner._search_worker("f hidden_config", 99, inv)
            self._wait_for_results(results)
            match_texts = [r[1] for r in results]
            self.assertIn(".hidden_config", match_texts)
        finally:
            os.environ.pop("KSEEK_FD_ARGS", None)


class _MockInvocation:
    def __init__(self, callback):
        self.callback = callback
        self.last_value = None

    def return_value(self, variant):
        self.last_value = variant
        if variant is not None:
            unpacked = variant.unpack()
            matches = unpacked[0] if isinstance(unpacked, tuple) else unpacked
            self.callback(matches)
        else:
            self.callback([])

    def return_error_literal(self, domain, code, message):
        pass


if __name__ == "__main__":
    unittest.main()
